import json
import unittest

import mock_api


JSON = {"Content-Type": "application/json"}


class StepsTest(unittest.TestCase):
    def setUp(self):
        self.saved = [dict(p) for p in mock_api.PROBES]

    def tearDown(self):
        mock_api.PROBES[:] = self.saved

    def post(self, update):
        return mock_api.handle("POST", "/api/probes", json.dumps([dict(update, probe_id=1)]), JSON)

    def test_steps_are_saved_and_sorted(self):
        status, probes = self.post({"steps": [{"action": "Spritz", "temperature": 82}, {"action": "Wrap", "temperature": 74}]})
        self.assertEqual(200, status)
        steps = probes[0]["steps"]
        self.assertEqual(["Wrap", "Spritz"], [s["action"] for s in steps])
        self.assertEqual({"action", "temperature", "done", "ignored"}, set(steps[0]))
        self.assertEqual(-1, probes[0]["step_alert"])
        self.assertEqual([], self.post({"steps": []})[1][0]["steps"])

    def test_bad_steps_are_refused(self):
        cases = [
            ([{"action": "a", "temperature": t} for t in (10, 20, 30, 40)], "At most 3 steps"),
            ([{"action": "x" * 17, "temperature": 74}], "A step needs an action of 1 to 16 characters"),
            ([{"action": "a|b", "temperature": 74}], "A step needs an action of 1 to 16 characters"),
            ([{"action": "Wrap", "temperature": 95}], "A step should be below the target"),
            ([{"action": "Wrap", "temperature": 0}], "A step should be 1 or more degrees"),
        ]
        for steps, error in cases:
            status, data = self.post({"target_temperature": 95, "minimum_temperature": 0, "steps": steps})
            self.assertEqual((400, error), (status, data["error"]))

    def test_grill_status_has_next_step_only(self):
        self.assertEqual(200, self.post({"target_temperature": 200, "minimum_temperature": 0,
                                         "steps": [{"action": "Wrap", "temperature": 190}]})[0])
        _, grill = mock_api.handle("GET", "/api/grill", None)
        probe = grill["probes"][0]
        self.assertNotIn("steps", probe)
        self.assertEqual({"action": "Wrap", "temperature": 190}, probe["next_step"])
        self.assertEqual((-1, ""), (probe["step_alert"], probe["step_alert_action"]))
        # A lower target (from Home Assistant, say) is never refused; the step is kept but ignored
        self.assertEqual(200, self.post({"target_temperature": 150})[0])
        _, grill = mock_api.handle("GET", "/api/grill", None)
        self.assertIsNone(grill["probes"][0]["next_step"])
        _, probes = mock_api.handle("GET", "/api/probes", None)
        self.assertTrue(probes[0]["steps"][0]["ignored"])
        self.assertIn("steps", mock_api.handle("GET", "/api/info", None)[1]["capabilities"])

    def test_a_skipped_step_does_not_block_changes_to_the_others(self):
        self.assertEqual(200, self.post({"target_temperature": 95, "minimum_temperature": 0,
                                         "steps": [{"action": "Wrap", "temperature": 74}, {"action": "Spritz", "temperature": 82}]})[0])
        self.assertEqual(200, self.post({"target_temperature": 80})[0])
        # Spritz at 82 is above the new target but already stored: a change to Wrap is still saved
        status, probes = self.post({"steps": [{"action": "Wrap", "temperature": 70}, {"action": "Spritz", "temperature": 82}]})
        self.assertEqual(200, status)
        self.assertEqual([70, 82], [s["temperature"] for s in probes[0]["steps"]])
        # A new step above the target is still refused
        status, data = self.post({"steps": [{"action": "Wrap", "temperature": 70}, {"action": "Sauce", "temperature": 85}]})
        self.assertEqual((400, "A step should be below the target"), (status, data["error"]))


class TimersTest(unittest.TestCase):
    def setUp(self):
        mock_api.TIMERS.clear()
        mock_api.ALARM_SOUNDING = False

    def test_start_shows_in_the_status_and_cancel_removes_it(self):
        status, timer = mock_api.handle("POST", "/api/timers", json.dumps({"name": "Flip the ribs", "minutes": 20}), JSON)
        self.assertEqual(201, status)
        self.assertEqual("Flip the ribs", timer["name"])
        self.assertEqual(1200, timer["remaining_s"])
        _, grill = mock_api.handle("GET", "/api/grill", None)
        self.assertEqual([timer["id"]], [t["id"] for t in grill["timers"]])
        self.assertEqual(200, mock_api.handle("POST", "/api/timers/cancel", json.dumps({"id": timer["id"]}), JSON)[0])
        self.assertEqual(404, mock_api.handle("POST", "/api/timers/cancel", json.dumps({"id": timer["id"]}), JSON)[0])

    def test_bad_timers_are_refused(self):
        self.assertEqual(400, mock_api.handle("POST", "/api/timers", json.dumps({"minutes": 0}), JSON)[0])
        self.assertEqual(400, mock_api.handle("POST", "/api/timers", json.dumps({"name": "x" * 25, "minutes": 5}), JSON)[0])
        for _ in range(6):
            self.assertEqual(201, mock_api.handle("POST", "/api/timers", json.dumps({"minutes": 5}), JSON)[0])
        status, data = mock_api.handle("POST", "/api/timers", json.dumps({"minutes": 5}), JSON)
        self.assertEqual((409, "At most 6 timers"), (status, data["error"]))

    def test_a_timer_that_ran_out_sounds_until_muted(self):
        _, timer = mock_api.handle("POST", "/api/timers", json.dumps({"seconds": 0}), JSON)
        _, grill = mock_api.handle("GET", "/api/grill", None)
        self.assertTrue(grill["timers"][0]["done"])
        self.assertTrue(grill["alarm_sounding"])
        self.assertEqual(1, grill["alarm_timers"])
        mock_api.handle("POST", "/api/alarm/mute", "{}", JSON)
        _, grill = mock_api.handle("GET", "/api/grill", None)
        self.assertEqual([], grill["timers"])


class InfoTest(unittest.TestCase):
    def test_info_names_the_firmware_and_its_capabilities(self):
        status, data = mock_api.handle("GET", "/api/info", None)
        self.assertEqual(200, status)
        self.assertEqual("grilly-plus", data["firmware"])
        self.assertEqual(1, data["api_version"])
        self.assertIn("history", data["capabilities"])


class HistoryTest(unittest.TestCase):
    def test_all_connected_probes_have_coarse_history(self):
        status, data = mock_api.handle("GET", "/api/history", None)
        self.assertEqual(200, status)
        ids = [p["probe_id"] for p in data["probes"]]
        self.assertEqual([1, 2, 3], ids)
        coarse = data["probes"][0]["coarse"]
        self.assertEqual(60, coarse["interval"])
        self.assertTrue(len(coarse["values"]) > 10)
        self.assertNotIn("fine", data["probes"][0])

    def test_one_probe_has_fine_history_too(self):
        status, data = mock_api.handle("GET", "/api/history", None, query="probe=2")
        self.assertEqual(200, status)
        self.assertEqual([2], [p["probe_id"] for p in data["probes"]])
        self.assertEqual(10, data["probes"][0]["fine"]["interval"])
        self.assertEqual(180, len(data["probes"][0]["fine"]["values"]))

    def test_bad_probe_is_rejected(self):
        status, data = mock_api.handle("GET", "/api/history", None, query="probe=9")
        self.assertEqual(400, status)

    def test_clear_empties_one_probe(self):
        json_headers = {"Content-Type": "application/json"}
        status, _ = mock_api.handle("POST", "/api/history/clear", json.dumps({"probe_id": 1}).encode(), json_headers)
        self.assertEqual(200, status)
        _, data = mock_api.handle("GET", "/api/history", None, query="probe=1")
        self.assertEqual([], data["probes"][0]["coarse"]["values"])
        mock_api.CLEARED_AT.pop(1, None)

    def test_probes_report_eta_seconds(self):
        _, data = mock_api.handle("GET", "/api/grill", None)
        self.assertIn("eta_seconds", data["probes"][0])
        self.assertEqual(-1, data["probes"][3]["eta_seconds"])

    def test_age_is_zero_when_values_is_empty(self):
        json_headers = {"Content-Type": "application/json"}
        status, _ = mock_api.handle("POST", "/api/history/clear", json.dumps({"probe_id": 1}).encode(), json_headers)
        self.assertEqual(200, status)
        try:
            _, data = mock_api.handle("GET", "/api/history", None, query="probe=1")
            coarse = data["probes"][0]["coarse"]
            fine = data["probes"][0]["fine"]
            self.assertEqual([], coarse["values"])
            self.assertEqual(0, coarse["age"])
            self.assertEqual([], fine["values"])
            self.assertEqual(0, fine["age"])
        finally:
            mock_api.CLEARED_AT.pop(1, None)

    def test_clear_rejects_invalid_json(self):
        json_headers = {"Content-Type": "application/json"}
        status, data = mock_api.handle("POST", "/api/history/clear", b"not json", json_headers)
        self.assertEqual(400, status)
        self.assertEqual({"error": "Could not deserialize json"}, data)

    def test_clear_rejects_non_object_body(self):
        json_headers = {"Content-Type": "application/json"}
        status, data = mock_api.handle("POST", "/api/history/clear", json.dumps([1, 2]).encode(), json_headers)
        self.assertEqual(400, status)
        self.assertEqual({"error": "Could not deserialize json"}, data)


class UpdateTest(unittest.TestCase):
    JSON = {"Content-Type": "application/json"}

    def setUp(self):
        mock_api.reset_update()

    def tearDown(self):
        mock_api.reset_update()

    def test_latest_offers_a_newer_version(self):
        status, data = mock_api.handle("GET", "/api/update/latest", None)
        self.assertEqual(200, status)
        self.assertEqual("26.10.03", data["latest"])
        self.assertTrue(data["available"])
        self.assertEqual("idle", data["state"])
        self.assertEqual(mock_api.SETTINGS["firmware_version"], data["current"])
        self.assertGreater(data["size"], 0)

    def test_grill_reports_update_available(self):
        _, data = mock_api.handle("GET", "/api/grill", None)
        self.assertEqual("26.10.03", data["update_available"])

    def test_check_is_accepted_once_a_minute(self):
        status, data = mock_api.handle("POST", "/api/update/check", b"{}", self.JSON)
        self.assertEqual((202, {"success": True}), (status, data))
        _, latest = mock_api.handle("GET", "/api/update/latest", None)
        self.assertEqual("checking", latest["state"])
        status, data = mock_api.handle("POST", "/api/update/check", b"{}", self.JSON)
        self.assertEqual(429, status)
        self.assertEqual("Checked less than a minute ago", data["error"])

    def test_install_refuses_another_version(self):
        body = json.dumps({"version": "1.2.3"}).encode()
        status, data = mock_api.handle("POST", "/api/update/install", body, self.JSON)
        self.assertEqual(409, status)
        self.assertIn("error", data)

    def test_install_needs_the_admin_password(self):
        mock_api.ADMIN_PASSWORD = "secret"
        try:
            body = json.dumps({"version": "26.10.03"}).encode()
            status, _ = mock_api.handle("POST", "/api/update/install", body, self.JSON)
            self.assertEqual(401, status)
        finally:
            mock_api.ADMIN_PASSWORD = ""

    def test_install_is_accepted_and_finishes_later(self):
        body = json.dumps({"version": "26.10.03"}).encode()
        status, data = mock_api.handle("POST", "/api/update/install", body, self.JSON)
        self.assertEqual((202, {"success": True}), (status, data))
        status, _ = mock_api.handle("GET", "/api/grill", None)
        self.assertEqual(503, status)   # restarting
        mock_api.INSTALL_DONE_AT = 0   # pretend the 20 s have passed
        _, latest = mock_api.handle("GET", "/api/update/latest", None)
        self.assertEqual("26.10.03", latest["current"])
        self.assertFalse(latest["available"])
        _, grill = mock_api.handle("GET", "/api/grill", None)
        self.assertEqual("", grill["update_available"])
        mock_api.SETTINGS["firmware_version"] = "26.09.27"


class AdminResetTest(unittest.TestCase):
    JSON = {"Content-Type": "application/json"}

    def setUp(self):
        mock_api.ADMIN_PASSWORD = "forgotten"
        mock_api.SETTINGS["admin_password_set"] = True
        mock_api.RESET_STARTED_AT = None
        mock_api.RESET_ACTIVE = False
        mock_api.RESET_WRONG_TOTAL = 0

    def tearDown(self):
        mock_api.ADMIN_PASSWORD = ""
        mock_api.SETTINGS["admin_password_set"] = False
        mock_api.RESET_STARTED_AT = None
        mock_api.RESET_ACTIVE = False
        mock_api.RESET_WRONG_TOTAL = 0

    def reset(self, code, password=""):
        body = json.dumps({"code": code, "admin_password": password}).encode()
        return mock_api.handle("POST", "/api/admin/reset", body, self.JSON)

    def test_code_sets_a_new_password(self):
        status, data = mock_api.handle("POST", "/api/admin/reset/start", b"{}", self.JSON)
        self.assertEqual(202, status)
        self.assertEqual(300, data["seconds_left"])
        self.assertEqual((200, {"success": True}), self.reset("123456", "new"))
        self.assertEqual("new", mock_api.ADMIN_PASSWORD)
        self.assertEqual(410, self.reset("123456")[0])   # works once

    def test_empty_password_removes_it(self):
        mock_api.handle("POST", "/api/admin/reset/start", b"{}", self.JSON)
        self.assertEqual(200, self.reset("123456")[0])
        self.assertFalse(mock_api.SETTINGS["admin_password_set"])

    def test_five_wrong_codes_end_it(self):
        mock_api.handle("POST", "/api/admin/reset/start", b"{}", self.JSON)
        status, data = self.reset("000000")
        self.assertEqual((403, 4), (status, data["attempts_left"]))
        for _ in range(4):
            status, data = self.reset("000000")
        self.assertEqual((403, 0), (status, data["attempts_left"]))
        self.assertEqual(410, self.reset("123456")[0])
        self.assertEqual(429, mock_api.handle("POST", "/api/admin/reset/start", b"{}", self.JSON)[0])

    def test_fifteen_wrong_codes_lock_until_restart(self):
        for _ in range(3):
            mock_api.RESET_STARTED_AT = None   # skip the minute between codes
            self.assertEqual(202, mock_api.handle("POST", "/api/admin/reset/start", b"{}", self.JSON)[0])
            for _ in range(5):
                self.reset("000000")
        mock_api.RESET_STARTED_AT = None
        self.assertEqual(423, mock_api.handle("POST", "/api/admin/reset/start", b"{}", self.JSON)[0])

    def test_cancel_ends_the_code(self):
        mock_api.handle("POST", "/api/admin/reset/start", b"{}", self.JSON)
        self.assertEqual(200, mock_api.handle("POST", "/api/admin/reset/cancel", b"{}", self.JSON)[0])
        self.assertEqual(410, self.reset("123456")[0])

    def test_needs_a_password_to_reset(self):
        mock_api.ADMIN_PASSWORD = ""
        self.assertEqual(409, mock_api.handle("POST", "/api/admin/reset/start", b"{}", self.JSON)[0])


if __name__ == "__main__":
    unittest.main()

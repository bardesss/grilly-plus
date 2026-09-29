"""Mock grill API for tools/dev_server.py --mock: realistic data for development and screenshots."""
import base64
import json
import time
import urllib.parse

START = time.time()

# Mock-only: set True to make /api/grill report alarm_sounding, cleared by /api/alarm/mute.
# POST /api/_mock/alarm sets it True, for testing the web app's mute control without real hardware.
ALARM_SOUNDING = False
# Mock-only: probe_id that /api/grill reports as the cause of the sounding alarm, cleared with it.
ALARM_PROBE_ID = None

SETTINGS = {
    "name": "Big Green Egg", "uuid": "43c62ed2-4dc0-41a5-8f71-16db60155739", "firmware_version": "26.09.27",
    "temperature_unit": "celcius", "beep_enabled": True, "beep_volume": 4, "beep_degrees_before": 5,
    "beep_outside_target": True, "beep_on_ready": True, "cucaracha_enabled": False,
    "screen_timeout_minutes": 0, "backlight_timeout_minutes": 5, "backlight_brightness": 4,
    "opengrill_server": "", "mqtt_broker": "192.168.1.10", "mqtt_port": 1883, "mqtt_topic": "grilly-plus",
    "mqtt_user": "grill", "mqtt_password_set": True,
    "wifi_ssid": "HomeNet", "wifi_ip": "0.0.0.0", "wifi_subnet": "0.0.0.0", "wifi_gateway": "0.0.0.0",
    "wifi_dns": "0.0.0.0", "wifi_password_set": True,
    "local_ap_ssid": "GrillyPlus_A1B2C3", "local_ap_ip": "192.168.200.10", "local_ap_subnet": "255.255.255.0",
    "local_ap_gateway": "192.168.200.10", "local_ap_password_set": True, "admin_password_set": False,
}

# Mock-only: the update offered by "GitHub". check flips state to "checking" for 3 s; install "finishes" after 20 s.
UPDATE_LATEST = "26.10.03"
UPDATE_NOTES = "## 2026-10-03" + chr(10) + "- Update straight from GitHub" + chr(10) + "- Small fixes"
UPDATE_CHECKED_AT = time.time() - 2 * 3600
UPDATE_CHECKING_UNTIL = 0.0
LAST_CHECK_REQUEST = 0.0
INSTALL_DONE_AT = None
MOCK_START_VERSION = SETTINGS["firmware_version"]


def reset_update():
    global UPDATE_CHECKED_AT, UPDATE_CHECKING_UNTIL, LAST_CHECK_REQUEST, INSTALL_DONE_AT
    UPDATE_CHECKED_AT = time.time() - 2 * 3600
    UPDATE_CHECKING_UNTIL = 0.0
    LAST_CHECK_REQUEST = 0.0
    INSTALL_DONE_AT = None
    SETTINGS["firmware_version"] = MOCK_START_VERSION


def finish_install_when_due():
    if INSTALL_DONE_AT is not None and time.time() >= INSTALL_DONE_AT:
        SETTINGS["firmware_version"] = UPDATE_LATEST


def update_available():
    finish_install_when_due()
    return UPDATE_LATEST if SETTINGS["firmware_version"] != UPDATE_LATEST else ""


def update_latest():
    available = update_available() != ""
    return {
        "current": SETTINGS["firmware_version"], "latest": UPDATE_LATEST, "available": available,
        "checked_seconds_ago": int(time.time() - UPDATE_CHECKED_AT),
        "state": "checking" if time.time() < UPDATE_CHECKING_UNTIL else "idle", "error": "",
        "notes": UPDATE_NOTES, "size": 1262368, "last_install_error": "",
    }


PROBES = [
    {"probe_id": 1, "name": "Brisket", "target_temperature": 95.0, "minimum_temperature": 0.0, "connected": True,
     "probe_type": "grilleye_iris", "reference_kohm": 100, "reference_celcius": 25, "reference_beta": 4250,
     "offset_celcius": 0.0},
    {"probe_id": 2, "name": "Ribs", "target_temperature": 93.0, "minimum_temperature": 88.0, "connected": True,
     "probe_type": "grilleye_iris", "reference_kohm": 100, "reference_celcius": 25, "reference_beta": 4250,
     "offset_celcius": 0.0},
    {"probe_id": 3, "name": "Grill", "target_temperature": 0.0, "minimum_temperature": 0.0, "connected": True,
     "probe_type": "maverick_et733", "reference_kohm": 200, "reference_celcius": 25, "reference_beta": 4250,
     "offset_celcius": 0.0},
] + [
    {"probe_id": n, "name": "Probe %d" % n, "target_temperature": 0.0, "minimum_temperature": 0.0, "connected": False,
     "probe_type": "grilleye_iris", "reference_kohm": 100, "reference_celcius": 25, "reference_beta": 4250,
     "offset_celcius": 0.0}
    for n in range(4, 9)
]

CONNECTED_AT = {1: START - 18720, 2: START - 13200, 3: START - 19200}
BASE_TEMPERATURE = {1: 71.4, 2: 90.2, 3: 118.0}

# Mock-only: probe_id -> time.time() when its history was cleared via /api/history/clear
CLEARED_AT = {}


def temperature(probe):
    if not probe["connected"]:
        return 0.0
    drift = (time.time() - START) / 600.0
    return round(BASE_TEMPERATURE[probe["probe_id"]] + drift + probe.get("offset_celcius", 0.0), 1)


def history_values(probe, interval, count_limit):
    """Tenths of a degree, oldest first, every `interval` seconds up to now."""
    started = max(CONNECTED_AT[probe["probe_id"]], CLEARED_AT.get(probe["probe_id"], 0))
    now = time.time()
    count = min(int((now - started) // interval), count_limit)
    values = []
    for i in range(count):
        taken = now - (count - i) * interval
        # A warm-up from room temperature over the first 40 minutes, then the same drift as temperature()
        warmup = min(1.0, (taken - started) / 2400.0)
        base = 20.0 + (BASE_TEMPERATURE[probe["probe_id"]] - 20.0) * warmup
        values.append(round((base + (taken - START) / 600.0 + probe.get("offset_celcius", 0.0)) * 10))
    return values


def history(only_probe=None):
    result = []
    for p in PROBES:
        if not p["connected"] or (only_probe and p["probe_id"] != only_probe):
            continue
        coarse_values = history_values(p, 60, 119)
        entry = {"probe_id": p["probe_id"],
                  "coarse": {"interval": 60, "age": 20 if coarse_values else 0, "values": coarse_values}}
        if only_probe:
            fine_values = history_values(p, 10, 180)
            entry["fine"] = {"interval": 10, "age": 3 if fine_values else 0, "values": fine_values}
        result.append(entry)
    return {"probes": result}


def eta_seconds(probe):
    """The mock's curve rises 0.1 degree per minute; only target mode below the target has an eta."""
    if not probe["connected"] or probe["target_temperature"] <= 0 or probe["minimum_temperature"] > 0:
        return -1
    remaining = probe["target_temperature"] - temperature(probe)
    if remaining <= 0:
        return -1
    eta = int(remaining / 0.1 * 60)
    return -1 if eta > 86400 else eta


def grill():
    return {
        "name": SETTINGS["name"], "unique_id": SETTINGS["uuid"], "firmware_version": SETTINGS["firmware_version"],
        "update_available": update_available(),
        "hostname": "grilly-plus-%s.local" % SETTINGS["uuid"].replace("-", "")[:8].lower(),
        "battery_percentage": 82, "battery_charging": True, "battery_millivolts": 3950,
        "last_reset_reason": "software", "last_off_reason": "update",
        "wifi_connected": True, "wifi_ssid": SETTINGS["wifi_ssid"],
        "wifi_ip": "192.168.1.50", "wifi_signal": -58,
        "local_ap_ssid": SETTINGS["local_ap_ssid"], "local_ap_ip": SETTINGS["local_ap_ip"], "temperature_unit": SETTINGS["temperature_unit"],
        "alarm_sounding": ALARM_SOUNDING,
        "probes": [
            {"probe_id": p["probe_id"], "name": p["name"], "temperature": temperature(p),
             "minimum_temperature": p["minimum_temperature"], "target_temperature": p["target_temperature"],
             "connected": p["connected"],
             "connected_seconds": int(time.time() - CONNECTED_AT[p["probe_id"]]) if p["connected"] else 0,
             "alarm": p["probe_id"] == ALARM_PROBE_ID, "eta_seconds": eta_seconds(p)}
            for p in PROBES
        ],
    }


def probes():
    return [dict(p, temperature=temperature(p), eta_seconds=eta_seconds(p)) for p in PROBES]


# Kept here because SETTINGS only says whether it is set, like the firmware
ADMIN_PASSWORD = ""

# Same limits as the firmware (lib/JsonUtilities)
NUMBER_LIMITS = {"beep_volume": (0, 5), "backlight_brightness": (0, 5), "mqtt_port": (1, 65535)}


def admin_authorized(headers):
    """True when no admin password is set, or the request has it as Basic auth with user admin."""
    if ADMIN_PASSWORD == "":
        return True
    expected = "Basic " + base64.b64encode(("admin:" + ADMIN_PASSWORD).encode("utf-8")).decode("ascii")
    return (headers or {}).get("Authorization") == expected


WIFI_SCAN = [
    {"ssid": "HomeNet", "signal_strength": -52, "auth_method": "wpa2_psk"},
    {"ssid": "HomeNet-Guest", "signal_strength": -61, "auth_method": "wpa2_psk"},
    {"ssid": "Neighbours", "signal_strength": -78, "auth_method": "wpa2_wpa3_psk"},
    {"ssid": "Cafe Free WiFi", "signal_strength": -86, "auth_method": "open"},
]


def handle(method, path, body, headers=None, query=""):
    """Returns (status, json_body) for an /api request."""
    global ADMIN_PASSWORD, ALARM_SOUNDING, ALARM_PROBE_ID
    global UPDATE_CHECKED_AT, UPDATE_CHECKING_UNTIL, LAST_CHECK_REQUEST, INSTALL_DONE_AT
    headers = headers or {}
    finish_install_when_due()
    if INSTALL_DONE_AT is not None and SETTINGS["firmware_version"] != UPDATE_LATEST and path in ("/api/grill", "/api/info"):
        return 503, {"error": "The grill is updating"}   # the real grill doesn't answer while it restarts
    if method == "GET" and path == "/api/update/latest":
        return 200, update_latest()
    if method == "POST" and path == "/api/update/check":
        if time.time() - LAST_CHECK_REQUEST < 60:
            return 429, {"error": "Checked less than a minute ago"}
        LAST_CHECK_REQUEST = time.time()
        UPDATE_CHECKING_UNTIL = LAST_CHECK_REQUEST + 3
        UPDATE_CHECKED_AT = UPDATE_CHECKING_UNTIL
        return 202, {"success": True}
    if method == "POST" and path == "/api/update/install":
        if "json" not in headers.get("Content-Type", ""):
            return 415, {"error": "Content-Type should be application/json"}
        if not admin_authorized(headers):
            return 401, {"error": "Wrong admin password"}
        try:
            version = json.loads(body or b"{}").get("version")
        except (ValueError, AttributeError):
            return 400, {"error": "Could not deserialize json"}
        if update_available() == "" or version != UPDATE_LATEST:
            return 409, {"error": "That version isn't on offer, check for updates first"}
        INSTALL_DONE_AT = time.time() + 20
        return 202, {"success": True}
    if method == "GET" and path == "/api/grill":
        return 200, grill()
    if method == "GET" and path == "/api/history":
        params = urllib.parse.parse_qs(query)
        if "probe" in params:
            probe_id = int(params["probe"][0]) if params["probe"][0].isdigit() else 0
            if not 1 <= probe_id <= 8:
                return 400, {"error": "probe should be 1 to 8"}
            return 200, history(probe_id)
        return 200, history()
    if method == "POST" and path == "/api/history/clear":
        if not (headers or {}).get("Content-Type", "").startswith("application/json"):
            return 415, {"error": "Content-Type should be application/json"}
        try:
            parsed = json.loads(body or b"{}")
            if not isinstance(parsed, dict):
                raise ValueError("not an object")
        except ValueError:
            return 400, {"error": "Could not deserialize json"}
        probe_id = int(parsed.get("probe_id", 0))
        if not 1 <= probe_id <= 8:
            return 400, {"error": "probe_id should be between 1 and 8"}
        CLEARED_AT[probe_id] = time.time()
        return 200, {"success": True}
    if method == "POST" and path == "/api/alarm/mute":
        if not (headers or {}).get("Content-Type", "").startswith("application/json"):
            return 415, {"error": "Content-Type should be application/json"}
        ALARM_SOUNDING = False
        ALARM_PROBE_ID = None
        return 200, {"success": True}
    if method == "POST" and path == "/api/_mock/alarm":
        # Mock-only helper, not part of the real firmware API: lets the web app be tested without hardware.
        ALARM_SOUNDING = True
        ALARM_PROBE_ID = 1
        return 200, {"success": True}
    if method == "GET" and path == "/api/probes":
        return 200, probes()
    if method == "POST" and path == "/api/probes":
        updates = json.loads(body or b"[]")
        for update in updates:
            probe = next((p for p in PROBES if p["probe_id"] == int(update["probe_id"])), None)
            if probe is None:
                return 400, {"error": "probe_id should be between 1 and 8"}
            if "offset_celcius" in update and update["offset_celcius"] is not None:
                offset = float(update["offset_celcius"])
                if not -10.0 <= offset <= 10.0:
                    return 400, {"error": "offset_celcius should be between -10.0 and 10.0"}
        for update in updates:
            probe = next((p for p in PROBES if p["probe_id"] == int(update["probe_id"])), None)
            probe.update({k: v for k, v in update.items() if k != "probe_id"})
        return 200, probes()
    if method == "GET" and path == "/api/settings":
        return 200, SETTINGS
    if method == "POST" and path == "/api/settings":
        update = json.loads(body or b"{}")
        password = update.get("local_ap_password")
        if password and len(password) < 8:
            return 400, {"error": "local_ap_password should be empty or at least 8 characters"}
        for key, (low, high) in NUMBER_LIMITS.items():
            if key in update and not low <= int(update[key] or 0) <= high:
                return 400, {"error": "%s should be between %d and %d" % (key, low, high)}
        if "admin_password" in update and not admin_authorized(headers):
            return 401, {"error": "The current admin password is needed to change it"}
        for key, value in update.items():
            if key == "admin_password":
                ADMIN_PASSWORD = value
            if key.endswith("_password"):
                SETTINGS[key + "_set"] = value != ""
            else:
                SETTINGS[key] = value
        return 200, SETTINGS
    if method == "GET" and path == "/api/info":
        return 200, {
            "firmware": "grilly-plus", "firmware_version": SETTINGS["firmware_version"], "api_version": 1,
            "unique_id": SETTINGS["uuid"],
            "hostname": "grilly-plus-%s.local" % SETTINGS["uuid"].replace("-", "")[:8].lower(),
            "probe_count": 8,
            "capabilities": ["history", "eta", "clear_history", "alarm_mute", "alarm_probes",
                             "calibration_offset", "diagnostics", "ota_upload"],
        }
    if method == "GET" and path == "/api/wifiscan":
        time.sleep(1.5)
        return 200, WIFI_SCAN
    if method == "POST" and path == "/api/update":
        if headers.get("X-Grilly-Update") != "1":
            return 403, {"error": "Missing X-Grilly-Update header"}
        if not admin_authorized(headers):
            return 401, {"error": "Wrong admin password"}
        time.sleep(2)
        return 200, {"success": True}
    return 404, {"error": "Not found"}

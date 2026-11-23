<h1 align="center">Grilly+</h1>

<p align="center">
  <strong>New firmware for the Grilleye Max. Your thermometer, back in business and better than it left the factory.</strong>
</p>

<p align="center">
  <a href="https://github.com/bardesss/grilly-plus/releases/latest"><img src="https://img.shields.io/github/v/release/bardesss/grilly-plus?label=release&color=ff7a1a" alt="Latest release"></a>
  <a href="https://github.com/bardesss/grilly-plus/actions/workflows/checks.yml"><img src="https://github.com/bardesss/grilly-plus/actions/workflows/checks.yml/badge.svg" alt="Checks"></a>
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
</p>

The Grilleye Max is a great 8-probe thermometer whose maker stopped supporting it. Grilly+ gives it a second life: a fast web app on your phone, live graphs of every cook, a "ready in 1h 20m" estimate, alarms you can mute from the couch, and updates that install themselves from GitHub. No cloud, no account, no app store. Everything runs on the thermometer itself, on your own WiFi.

Grilly+ is a fork of [Free-Grilly](https://github.com/epiecs/free-grilly) by [Epiecs](https://github.com/epiecs) and [Questum](https://questum.be/), who reverse engineered the hardware and wrote the original firmware.

> [!WARNING]
> Grilly+ is unofficial firmware, not made or endorsed by GrillEye® / G&C. Flashing it is at your own risk and voids any remaining warranty. Read the [full disclaimer](#disclaimer) first.

---

## Why Grilly+

| | |
|---|---|
| 📱 **A real app, in your browser** | One card per probe with a live graph of the whole cook. Tap a card to set a name, a target temperature or a range. Works on any phone or computer, in dark or light mode. Nothing to install. |
| ⏱️ **Know when it's done** | Grilly+ watches how fast each probe heats up and tells you when it'll reach its target: "ready in ~1h 20m" in the app, "in 1:20" on the grill's own screen. During the stall it says so instead of guessing. |
| 🔔 **Alarms that make sense** | It beeps when food is ready or the grill leaves its range, and sends a Telegram, ntfy or Pushover message (or calls your webhook) when you're away from it. Mute it from your phone, and the app shows which probe went off. The probe's label blinks on the grill too. |
| 🔄 **Updates with one tap** | The grill checks GitHub for new releases once a day. Tap **Install**, and a minute later it runs the new version. Every download is checksum-verified, and a failed update leaves the old version running. |
| 🏠 **Plays well with your smart home** | Shows up in Home Assistant by itself over MQTT, plus a documented local REST API. |
| 🔒 **Yours, and only yours** | No cloud. An admin password protects updates, the hotspot gets its own password, and a forgotten admin password can be reset with a code from the grill's screen. |

## See it in action

**The web app** shows every probe at a glance. Tap one for the big chart and its settings.

<p align="center">
  <img src="docs/screenshots/app_grill_phone_dark.png" alt="Grill view on a phone" width="250">
  <img src="docs/screenshots/app_editor_phone_dark.png" alt="Probe editor on a phone" width="250">
  <img src="docs/screenshots/app_update_phone_dark.png" alt="Updating from GitHub on a phone" width="250">
</p>

<img src="docs/screenshots/app_editor_desktop_dark.png" alt="Probe editor with the temperature chart on a desktop" width="100%">

<img src="docs/screenshots/app_settings_desktop_light.png" alt="Settings on a desktop, light theme" width="100%">

**On the grill itself**, the screen adapts to how many probes you use:

<table>
  <tr>
    <td align="center"><img src="docs/screenshots/device_one_probe.png" alt="One probe" width="260"><br><sub>One probe: big reading, target bar and time to target</sub></td>
    <td align="center"><img src="docs/screenshots/device_two_probes.png" alt="Two probes" width="260"><br><sub>Two probes, with cook time and time to target</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screenshots/device_four_probes.png" alt="Four probes" width="260"><br><sub>Up to four probes, with READY / HIGH / LOW</sub></td>
    <td align="center"><img src="docs/screenshots/device_eight_probes.png" alt="Eight probes" width="260"><br><sub>Five to eight probes, with a thermometer per target</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screenshots/device_update.png" alt="Update mode" width="260"><br><sub>Installing an update from GitHub</sub></td>
    <td align="center"><img src="docs/screenshots/device_reset_code.png" alt="Admin password reset code" width="260"><br><sub>The code for resetting a forgotten admin password</sub></td>
  </tr>
</table>

---

## Contents

- [Features](#features)
- [Getting started](#getting-started)
- [Using the grill](#using-the-grill)
- [Updating the firmware](#updating-the-firmware)
- [Admin password](#admin-password)
- [Supported probes](#supported-probes)
- [Integrations: API, MQTT, Home Assistant](#integrations)
- [Development](#development)
- [Contributing](#contributing)
- [Built with AI](#built-with-ai)
- [Disclaimer](#disclaimer)

## Features

### On the grill
- **Temperature screens** for 1, 2, up to 4, and up to 8 probes, with the time to target, a target bar, and READY / HIGH / LOW labels. Long probe names are shortened to fit.
- **Alarms** when a probe reaches its target or leaves its range, following the "Beep when ready" and "Beep outside target" settings. The label of the probe that's beeping blinks. A muted alarm stays muted when you rename a probe or change a setting.
- **Info screen** with the grill's name, firmware, address, WiFi, and the hotspot's name and password.
- **Low-battery protection:** at 5 % or 3.2 V (not charging, confirmed for 15 seconds) the grill beeps three times and switches itself off cleanly, before an empty battery causes brownouts.
- **Reliable:** it starts back up by itself after an update, a crash or a factory reset. Empty probe sockets don't flicker, and the button is debounced so electrical noise can't fake a press.

### In the web app
- **Dashboard** with a card per connected probe: temperature, a graph of the whole cook with a dot at the current reading, the target line (labelled) or range band, and "ready in ~1h 20m". With a time to target, a dotted forecast runs from now up to the target and ends at the clock time it'll be ready.
- **Food presets:** pick steak medium-rare, pulled pork, chicken breast or one of about 20 other foods, and the target fills in. Safe temperatures follow the USDA. Add your own (also ranges, like your smoker's sweet spot), and hide the ones you never use.
- **Food icons** on the cards and on the grill's screen, so you see at a glance which probe is in the chicken.
- **Probe editor:** name, alarm (off / target / range) with large steppers, probe type, calibration offset, and a large chart with temperature labels, a "30 min / Whole cook" switch, **Download CSV** and **Clear history**.
- **Temperature history** on the grill: the last 30 minutes at 10 seconds per reading plus the whole cook at a coarser step, in a fixed ~5 KB, so even a 24-hour cook fits. It's kept until a restart, until a probe has been unplugged for more than 10 minutes, or until you clear it.
- **Time to target**, worked out from the last 10 minutes of readings in target mode. After a sudden drop, such as opening the lid, it picks up again after about 3 minutes of climbing.
- **Stall detection:** when brisket or pork shoulder sits flat for 20 minutes between 60 and 80 °C, the card says "In the stall" and the grill shows STALL, so you know why the time to target is gone.
- **Lid open:** in range mode, opening the lid makes the grill temperature drop fast. Grilly+ notices and holds back the "too low" alarm for a few minutes while the grill recovers; the card says "Lid open".
- **Alarm in the browser:** a dialog says which probe went off and why, with a big Mute button. The page beeps along with the grill, vibrates on Android and shows a bell in the tab title. Switch it off per browser under **Settings › This browser**.
- **Phone notifications** through Telegram, ntfy and/or Pushover for ready, almost ready, too hot or too cold, the stall, a low battery (20 % and 10 %) and a timer that runs out, each with its own switch. They also arrive when the beeper is off. Set it up under **Settings › Notifications**, where each service folds open and shows whether it is set up, and tap **Send test**:
  - **Telegram:** create a bot with @BotFather, paste its token and your chat ID.
  - **ntfy:** pick a hard-to-guess topic, fill in `https://ntfy.sh/your-topic` (or your own ntfy server) and subscribe to it in the [ntfy app](https://ntfy.sh). No account needed.
  - **Pushover:** paste your user key and the API token of an application you create on [pushover.net](https://pushover.net).
  - **Webhook:** fill in a URL that gets a JSON POST for every notification, for example a Home Assistant webhook trigger (`http://homeassistant.local:8123/api/webhook/your-id`). The body has `event` (`ready`, `almost_ready`, `too_hot`, `too_cold`, `stall`, `battery_low`, `timer_done` or `test`), `grill`, `message`, `unit`, `probe` (`id`, `name`, `temperature`, `target`, `minimum`), `battery` and `timer` (`name`, and `probe` for a rest timer).
- **Mute** a sounding alarm from your phone. The card of the probe that triggered it is highlighted.
- **Settings save themselves.** Network changes are applied together with one button, and every password field has a show/hide button.
- **About page** with the battery voltage and why the grill last restarted or switched off (crash, watchdog, update, button, factory reset, empty battery).
- Works on phones and desktops, in a dark or light theme that follows your device.

### Updates and security
- **Update from GitHub** with one tap, or **install from a file**. See [Updating the firmware](#updating-the-firmware).
- **Admin password** to protect firmware updates, with a reset code on the grill's screen if you forget it.
- **Generated hotspot password** on new and factory-reset grills, shown on the Info screen, so the grill's own WiFi isn't open to anyone in range. Grills upgraded from an older version keep the hotspot password they had.
- Passwords are never sent back by the API, and web pages on other sites can't change your settings or push firmware.

### Connectivity
- **Two ways in:** the grill's own hotspot (`http://192.168.200.10`) and your home WiFi.
- **Find it by name:** `http://grilly-plus-xxxxxxxx.local`, on both networks. The exact name is on the Info screen and the About page. (Some Android browsers don't resolve `.local` names; the IP address always works.)
- **Static IP or DHCP.** A static IP that's missing a gateway falls back to DHCP instead of making the grill unreachable.
- **MQTT** and a **local REST API**. See [Integrations](#integrations).
- **Opengrill (experimental):** support for the upcoming Opengrill server by [Epiecs](https://github.com/epiecs). It isn't public yet, so leave the Opengrill setting empty.

## Getting started

### 1. Install Grilly+

Follow the [flashing guide](docs/how_to_flash.md). You can flash from your browser over USB, no tools needed.

**Coming from Free-Grilly?** Install Grilly+ through Free-Grilly's own `/update` page with the `-ota.bin` file from the [latest release](https://github.com/bardesss/grilly-plus/releases/latest). Grilly+ uses the same settings storage, so your WiFi, probe and MQTT settings stay, including your MQTT topic prefix. Only fresh installs and factory resets get the new defaults (`Grilly+` as name, `grilly-plus` as MQTT prefix, `GrillyPlus_xxxxxx` as hotspot name).

### 2. Connect it to your WiFi

1. **Switch it on:** hold the side button until you hear the beeps, then let go.
2. **Find its hotspot:** the grill starts its own WiFi network, `GrillyPlus_xxxxxx`. Hold the button for about 1 second, until 2 beeps, then let go to see its name and password on the Info screen (past 2 seconds it switches off).
3. **Connect** your phone or computer to that network.
4. **Open** `http://192.168.200.10` in a browser.
5. **Enter your home WiFi** under **Settings**, then **Network**, and tap **Apply**.
6. **Done.** The grill joins your WiFi and gets an address from your router. Reach it at `http://grilly-plus-xxxxxxxx.local` or its IP address, both shown on the Info screen. The hotspot keeps working too.

The [WiFi guide](docs/how_to_connect_to_wifi.md) walks through it with screenshots.

## Using the grill

### The button

| Press | What it does |
|---|---|
| Hold ~2 s while off (2 beeps) | Switch on |
| Short press | Next screen; mutes a sounding alarm; takes a password reset code off the screen |
| Hold 2 s (2 beeps) | Info screen with WiFi and hotspot details |
| Hold 3 s (3 beeps) | Switch off |
| Hold 10 s (3 long beeps) | Factory reset: clears **all** settings |

### Timers

Tap **+ Timer** on the Grill page, give it a name ("Flip the ribs") and the minutes, or tap 5, 10, 15, 30 or 60. Up to 6 timers run at once, on the grill itself, so they keep going with the app closed. The grill's top bar shows the one that ends first. When one runs out the grill beeps like a probe alarm and shows **TIME'S UP** with its name; a short press on the button stops it and goes back to the page you were on. The app shows it in the alarm popup, and **Mute** stops it too. Tap the cross on a timer to stop it early. Timers are kept in the grill's memory, so switching the grill off or restarting it clears them. With phone notifications set up you also get "⏰ Flip the ribs: time's up" (switch: **A timer runs out**).

Meat needs to rest before you slice it. Set a **Rest timer** in a probe's editor (in Target mode) and a timer "Rest: Brisket" starts by itself when the probe reaches its target. Picking a food fills in a usual rest time, like 60 minutes for brisket or 5 for a steak; 0 turns it off. The grill keeps the rest time per probe, like the target.

### Steps

For the moments before the target, like wrapping a brisket at 74 °C or saucing ribs: in a probe's editor (Target mode) tap **+ Add a step** under the target, pick or type the action (Wrap, Spritz, Sauce, Flip, Check, Baste or your own) and the temperature. Up to 3 per probe. When the probe climbs through a step the grill beeps once, like the early warning, and shows the step on its screen for 2 minutes (a short press closes it); the probe's card says "Wrap now". A step fires once: it doesn't fire again when the temperature drops while you wrap, and steps the probe has already passed when you set them up (or after a restart) don't fire at all. If the target goes below a step, the step is skipped until you raise the target again. Without steps you only see the **+ Add a step** link. Switch the beep off with **Beep at a step** under Settings.

### Probes and alarms

Tap a probe card in the app to set it up:
- **Off:** just show the temperature.
- **Target:** beep when the probe reaches the target (and optionally a few degrees before). This mode also shows the time to target.
- **Range:** beep when the temperature leaves the min-max range. Ideal for the grill or smoker temperature.
  Opening the lid drops the temperature fast. Grilly+ sees that and waits up to 5 minutes before calling it too low (**Settings › Alarms › Lid open pause**, 0 turns it off).

Not sure what temperature to use? Pick a food under **Food** and the target fills in. A probe that still has its default name ("Probe 3") gets the food's name.

Your own presets go on top of that list. Set a probe up the way you like it and tap **Save as preset** in its editor, or add, edit and delete them under **Settings › Food presets**. A preset can be a target or a range, has an icon, optionally a group of its own and, for a target, a rest time. That card also lets you switch off built-in presets you never use, or copy one to tweak it. Up to 30 presets, kept on the grill, so every phone and browser has the same list.

Each probe can have an icon for what's on it: beef, lamb, pork, poultry, fish, sausage, vegetables, the pit or smoker, or other. Picking a food sets it, or choose one under **Icon** in the editor. It shows on the probe's card and on the grill's screen, next to the temperature with one probe, next to the probe number with two to four, and after the temperature with five or more.

While the app is open, an alarm opens a dialog that says which probe went off and why, with **Mute** and **Close**, and the page beeps along with the grill. Browsers only allow sound after you tapped the page once, and phones pause pages in the background, so keep the page on screen. **Settings › This browser** has a test sound and a switch to turn it off.

**Calibration offset** (-10 to 10 °C) corrects a probe that reads a little high or low. It belongs to the socket, not the probe, so keep the probe in the same socket.

## Updating the firmware

### From GitHub (recommended)

1. Open the web app and go to **Settings**, then **Firmware updates**. When a new release is out, it says "*version* is available" and the Settings tab shows a dot. **What's new in this release** opens the release notes on GitHub; **Check now** checks right away instead of waiting for the daily check.
2. If you set an admin password, enter it under **Current admin password**.
3. Tap **Install** and confirm. Keep the grill on the charger or above 30 % battery.
4. The grill restarts into update mode: the screen dims and shows a progress bar. After about a minute it restarts into the new version and the page reloads. If anything goes wrong, it keeps the old version and the app tells you why.

The grill only downloads from this repository's releases, and only installs a file whose size and SHA-256 checksum match what GitHub lists. An interrupted update is safe: the new firmware is written next to the running one and only takes over after a complete, verified write.

**First time:** updating from GitHub arrived in release 26.11.22. On an older version, install 26.11.22 or newer from a file once; after that, every update can come from GitHub.

### From a file

1. Download the latest `grilly-plus-yyyy-mm-dd-ota.bin` from [Releases](https://github.com/bardesss/grilly-plus/releases/latest).
2. In the web app, go to **Settings**, then **Firmware updates**.
3. Under **Install from a file**, choose the `-ota.bin` file and press **Update**. If you set an admin password, enter it under **Current admin password** first.
4. The grill installs it, restarts on its own, and the page reloads.

## Admin password

Set an admin password at the bottom of **Settings**, then **Firmware updates**, so only you can install firmware, from GitHub or from a file. Once it's set, changing or removing it needs the current one (the app asks for it; the API takes it as Basic auth with user `admin`). It can't be changed over MQTT.

### Forgot the admin password?

Reset it from the app, as long as you can see the grill's screen. All your other settings (WiFi, probes, alarms, calibration) stay as they are.

1. In the web app, go to **Settings**, then **Firmware updates**.
2. Under **Current admin password**, tap **Forgot the admin password?**
3. The grill beeps once and its screen shows a 6-digit code, valid for 5 minutes.
4. Type the code in the app. Enter a new admin password, or leave it empty to remove the password.
5. Tap **Reset password**. The new password works right away.

A code works once. After 5 wrong tries it ends, and you can ask for a new one a minute later. After 15 wrong tries in total, the grill stops giving out codes until you restart it. **Cancel** in the app takes the code off the grill's screen, and so does a short press on the button (handy for a code you didn't ask for). If all else fails, a factory reset (hold the button for 10 seconds) clears the admin password, together with all other settings.

## Supported probes

The Grilleye Iris probes that came with the grill work best, but Grilly+ supports more:

| Probe type     | ref °C | ref kΩ | ref beta | Notes                                         |
|----------------|--------|--------|----------|-----------------------------------------------|
| Grilleye Iris  | 25     | 100    | 4250     | The best probe you can use: fast and accurate |
| Ikea Fantast   | 25     | 230    | 4250     | Slow; not for quick measurements              |
| Maverick ET733 | 25     | 200    | 4250     |                                               |
| Weber iGrill   | 25     | 100    | 3830     |                                               |

Any other **NTC** probe works as a `custom` probe if you know its reference temperature (°C), reference resistance (kΩ) and beta value. The [probe calculator spreadsheet](docs/probe_calculator.xlsx) helps you work them out.

## Integrations

### REST API

The grill has a local JSON API, documented in [openapi.yaml](docs/openapi.yaml) ([view it online](https://editor-next.swagger.io/?url=https://raw.githubusercontent.com/bardesss/grilly-plus/refs/heads/master/docs/openapi.yaml)). Some highlights:

- `GET /api/grill` and `GET /api/probes`: live status and probe readings, with `eta_seconds` per probe; `/api/grill` also has `update_available`.
- `GET /api/info`: which firmware runs and which features (`capabilities`) it has, for apps and integrations.
- `GET /api/history` and `POST /api/history/clear`: the temperature history.
- `GET /api/update/latest`, `POST /api/update/check` and `POST /api/update/install`: updating from GitHub.
- `POST /api/admin/reset/start`, `/api/admin/reset/cancel` and `/api/admin/reset`: resetting a forgotten admin password with the code from the screen.

### MQTT

Point the grill at your MQTT broker under **Settings** and it publishes the grill status, probes and settings. You can also change probes and settings over MQTT, including partial updates that only touch the keys you send, mute an alarm, and clear a probe's history. Topics and payloads are in [docs/mqtt.md](docs/mqtt.md).

### Home Assistant

Once the grill is connected to the same MQTT broker as Home Assistant (with the MQTT integration set up), it shows up by itself under **Settings › Devices & services › MQTT** as one device named after your grill. You get, for each probe, the temperature, target, time to target, stalled and alarm, plus the battery, charging, WiFi signal, alarm, update available, **Next timer** (when the first running timer ends) and a **Mute alarm** button. A probe's entities are unavailable while it isn't plugged in, and the whole device while the grill is off. Turn **Home Assistant discovery** off under **Settings › MQTT** to remove the device again.

If you'd rather make the entities yourself, turn discovery off and start from the example below. It is based on a starting point by [@woutercoppens](https://github.com/woutercoppens) (see [Free-Grilly issue #5](https://github.com/epiecs/free-grilly/issues/5)), and adds a temperature and target sensor for all 8 probes plus the battery. Replace the device id in `state_topic` with your grill's (shown under the MQTT topic in the settings). If you upgraded from Free-Grilly, your topic prefix is still `free-grilly` unless you changed it.

<details>
<summary>Home Assistant MQTT sensors (YAML)</summary>

```yaml
# Probe 1 (Defines Anchors)
- name: "Grilly+ Probe 1 Temperature"
  unique_id: "grilly_plus_probe_1_temp"
  <<: &probe_temp_defaults
    state_topic: "grilly-plus/7107d787-30c7-4d20-9b1b-db4cfd14fca4/grill" #Adjust
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement
    payload_available: "online"
    payload_not_available: "offline"
    device: &device_info
      identifiers: "grilly_plus_7107d787" #Adjust
      name: "Grilly+ Thermometer"
      manufacturer: "Grilly+"
      sw_version: "25.08.27"
  value_template: "{{ value_json.probes[0].temperature }}"
  availability_template: "{{ 'online' if value_json.probes[0].connected == true else 'offline' }}"

- name: "Grilly+ Probe 1 Target"
  unique_id: "grilly_plus_probe_1_target"
  <<: &probe_target_defaults
    state_topic: "grilly-plus/7107d787-30c7-4d20-9b1b-db4cfd14fca4/grill" #Adjust
    unit_of_measurement: "°C"
    device_class: temperature
    icon: mdi:target
    payload_available: "online"
    payload_not_available: "offline"
    device: *device_info
  value_template: "{{ value_json.probes[0].target_temperature }}"
  availability_template: "{{ 'online' if value_json.probes[0].connected == true else 'offline' }}"

# Probe 2
- name: "Grilly+ Probe 2 Temperature"
  unique_id: "grilly_plus_probe_2_temp"
  <<: *probe_temp_defaults
  value_template: "{{ value_json.probes[1].temperature }}"
  availability_template: "{{ 'online' if value_json.probes[1].connected == true else 'offline' }}"
- name: "Grilly+ Probe 2 Target"
  unique_id: "grilly_plus_probe_2_target"
  <<: *probe_target_defaults
  value_template: "{{ value_json.probes[1].target_temperature }}"
  availability_template: "{{ 'online' if value_json.probes[1].connected == true else 'offline' }}"

# Probe 3
- name: "Grilly+ Probe 3 Temperature"
  unique_id: "grilly_plus_probe_3_temp"
  <<: *probe_temp_defaults
  value_template: "{{ value_json.probes[2].temperature }}"
  availability_template: "{{ 'online' if value_json.probes[2].connected == true else 'offline' }}"
- name: "Grilly+ Probe 3 Target"
  unique_id: "grilly_plus_probe_3_target"
  <<: *probe_target_defaults
  value_template: "{{ value_json.probes[2].target_temperature }}"
  availability_template: "{{ 'online' if value_json.probes[2].connected == true else 'offline' }}"

# Probe 4
- name: "Grilly+ Probe 4 Temperature"
  unique_id: "grilly_plus_probe_4_temp"
  <<: *probe_temp_defaults
  value_template: "{{ value_json.probes[3].temperature }}"
  availability_template: "{{ 'online' if value_json.probes[3].connected == true else 'offline' }}"
- name: "Grilly+ Probe 4 Target"
  unique_id: "grilly_plus_probe_4_target"
  <<: *probe_target_defaults
  value_template: "{{ value_json.probes[3].target_temperature }}"
  availability_template: "{{ 'online' if value_json.probes[3].connected == true else 'offline' }}"

# Probe 5
- name: "Grilly+ Probe 5 Temperature"
  unique_id: "grilly_plus_probe_5_temp"
  <<: *probe_temp_defaults
  value_template: "{{ value_json.probes[4].temperature }}"
  availability_template: "{{ 'online' if value_json.probes[4].connected == true else 'offline' }}"
- name: "Grilly+ Probe 5 Target"
  unique_id: "grilly_plus_probe_5_target"
  <<: *probe_target_defaults
  value_template: "{{ value_json.probes[4].target_temperature }}"
  availability_template: "{{ 'online' if value_json.probes[4].connected == true else 'offline' }}"

# Probe 6
- name: "Grilly+ Probe 6 Temperature"
  unique_id: "grilly_plus_probe_6_temp"
  <<: *probe_temp_defaults
  value_template: "{{ value_json.probes[5].temperature }}"
  availability_template: "{{ 'online' if value_json.probes[5].connected == true else 'offline' }}"
- name: "Grilly+ Probe 6 Target"
  unique_id: "grilly_plus_probe_6_target"
  <<: *probe_target_defaults
  value_template: "{{ value_json.probes[5].target_temperature }}"
  availability_template: "{{ 'online' if value_json.probes[5].connected == true else 'offline' }}"

# Probe 7
- name: "Grilly+ Probe 7 Temperature"
  unique_id: "grilly_plus_probe_7_temp"
  <<: *probe_temp_defaults
  value_template: "{{ value_json.probes[6].temperature }}"
  availability_template: "{{ 'online' if value_json.probes[6].connected == true else 'offline' }}"
- name: "Grilly+ Probe 7 Target"
  unique_id: "grilly_plus_probe_7_target"
  <<: *probe_target_defaults
  value_template: "{{ value_json.probes[6].target_temperature }}"
  availability_template: "{{ 'online' if value_json.probes[6].connected == true else 'offline' }}"

# Probe 8
- name: "Grilly+ Probe 8 Temperature"
  unique_id: "grilly_plus_probe_8_temp"
  <<: *probe_temp_defaults
  value_template: "{{ value_json.probes[7].temperature }}"
  availability_template: "{{ 'online' if value_json.probes[7].connected == true else 'offline' }}"
- name: "Grilly+ Probe 8 Target"
  unique_id: "grilly_plus_probe_8_target"
  <<: *probe_target_defaults
  value_template: "{{ value_json.probes[7].target_temperature }}"
  availability_template: "{{ 'online' if value_json.probes[7].connected == true else 'offline' }}"

# Battery
- name: "Grilly+ Battery"
  unique_id: "grilly_plus_battery"
  state_topic: "grilly-plus/7107d787-30c7-4d20-9b1b-db4cfd14fca4/grill" #Adjust
  unit_of_measurement: "%"
  device_class: battery
  state_class: measurement
  payload_available: "online"
  payload_not_available: "offline"
  availability_template: "{{ 'online' if value_json.wifi_connected else 'offline' }}"
  value_template: "{{ value_json.battery_percentage }}"
  device: *device_info
```

</details>

### Opengrill (experimental)

The Opengrill server isn't public yet. Once it is, enter its address in the Opengrill field in the settings. While the field is empty, Grilly+ doesn't try to reach an Opengrill server.

## Development

The web app lives in `web/`: plain HTML, CSS and JavaScript, with no build step while you work on it.

1. Edit the files in `web/`.
2. Run `python tools/dev_server.py --mock` for mock data, or `python tools/dev_server.py <grill-ip>` to use a real grill.
3. Open http://localhost:8000 and reload after each change.

`pio run` builds the web app into the firmware automatically: one gzipped page, without comments and indentation, at most 50 KB. Comments are free, so write them; just keep strings and template literals on one line, because lines are trimmed.

Tests (they also run in CI on every pull request):

```
node --test "web/tests/*.test.js"
python -m unittest discover -s tools -p "test_*.py"
pio test -e native     # firmware logic (history, time to target, low battery, updates, password reset); needs a host C++ compiler
```

Building your own Grilly+ hardware? [docs/diy_hardware.md](docs/diy_hardware.md) is a work-in-progress design for a DIY unit.

## Contributing

Ideas, bug reports and pull requests are welcome.

- **Found a bug or have an idea?** Check the [Issues](https://github.com/bardesss/grilly-plus/issues) first, and open a new one with as much detail as you can if it isn't there yet.
- **Want to change something?** Fork the repository, make your change on a branch, and open a [pull request](https://github.com/bardesss/grilly-plus/pulls) against `master`. The checks must pass before it can be merged.

The detailed history of every release is in [changelog.md](changelog.md). Fixes that are useful for everyone are offered back to [Free-Grilly](https://github.com/epiecs/free-grilly) where possible.

## Built with AI

Grilly+ is developed with the help of AI coding agents (Anthropic's Claude). Every change goes through a pull request with automated tests and a code review, and firmware changes are tested on a real Grilleye Max before they're merged. Still, read the changes yourself if you rely on them, and report anything that looks wrong in the [Issues](https://github.com/bardesss/grilly-plus/issues).

## Disclaimer

- This firmware is unofficial and **not** affiliated with or endorsed by the original manufacturer (GrillEye® by G&C).
- Flashing custom firmware carries risks. You could damage or "brick" your Grilleye Max.
- You proceed entirely **at your own risk**. The authors and contributors are not responsible for any damage to your device.
- Flashing custom firmware voids any remaining manufacturer warranty.

Grilly+ is released under the [MIT License](https://opensource.org/licenses/MIT).

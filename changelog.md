# Changelog (firmware only)

## Unreleased

### New
- The graph on a probe card shows where the cook is going: with a time to target, a dotted forecast runs from the current reading to the target and ends at the clock time it'll be ready ("now … 18:05"). A dot marks the current reading, and the target line has its temperature on it. The lowest-highest label under the graph only stays for probes without an alarm
- Stall detection: when a probe in target mode stays within 1.5° for 20 minutes somewhere between 60 and 80 °C, with the target at least 5° higher, the app shows "In the stall" where the time to target goes and the grill's screen shows STALL. It ends by itself once the temperature climbs again
- Lid open: in range mode, a drop of 8° or more within a minute (opening the lid) holds back the "too low" alarm until the temperature is back within 2° of where it was, or until the lid pause is over. Meanwhile the app says "Lid open" and the grill shows LID instead of LOW. Settings > Alarms > Lid open pause sets the minutes (default 5, 0 = off). With it on, "too low" also has to last 30 seconds before it beeps; "too high" isn't held back
- Telegram notifications: the grill sends a message when a probe is ready, almost ready (the early warning), too hot or too cold, or goes into the stall, and when the battery drops to 20 % and 10 %. They also go out when the beeper is off or the alarm was muted. Set it up under Settings > Notifications with a bot token from @BotFather and your chat ID, pick the events, and use Send test. A failed message is tried 3 times and the card says why it failed. HTTPS to Telegram is checked against the Mozilla root certificates
- ntfy notifications, next to Telegram: fill in a topic URL (`https://ntfy.sh/your-topic`, or your own ntfy server, also over plain http on your home network) and subscribe to the topic in the ntfy app. An access token is optional; it is tied to the server, so changing the URL to another server without a new token removes it. The topic URL is kept secret like a password, because anyone who knows a topic on ntfy.sh can read along. Ready and outside-the-range messages come with high priority. Send test tries every service that is set up and shows a line for each
- Pushover notifications: paste your user key and the API token of an application you create on pushover.net. Ready and outside-the-range messages come with high priority
- Webhook: every notification also goes as a JSON POST to a URL of your choice, for Home Assistant, Node-RED and the like. The body has the event (`ready`, `almost_ready`, `too_hot`, `too_cold`, `stall`, `battery_low`, `test`), the grill, the message, the unit, the probe (id, name, temperature, target, minimum) and the battery. The URL is kept secret like a password, because a Home Assistant webhook URL is its own key
- Home Assistant discovery: with MQTT set up, the grill shows up in Home Assistant by itself as one device, with the temperature, target, time to target, stall and alarm of every probe, the battery, charging, WiFi signal, alarm, update available and a Mute alarm button. Probes that aren't plugged in show as unavailable, and the whole device when the grill is off. Switch it off under Settings > MQTT to remove the device

### API and MQTT
- `GET /api/notify` (per service: `{"sending", "telegram": {"set_up", "last"}, "ntfy": {...}, "pushover": {...}, "webhook": {...}}`) and `POST /api/notify/test`; `telegram_token` (write-only, `telegram_token_set`), `telegram_chat_id`, `ntfy_url` (write-only, `ntfy_url_set` and `ntfy_server`), `ntfy_token` (write-only, `ntfy_token_set`), `pushover_user` and `pushover_token` (write-only, `pushover_user_set` and `pushover_token_set`), `webhook_url` (write-only, `webhook_url_set` and `webhook_server`) and `notify_ready`, `notify_almost`, `notify_outside`, `notify_stall`, `notify_battery` in the settings
- `stalled` and `lid_open` for every probe in `/api/grill`, `/api/probes` and the MQTT `grill` and `probes` topics; `lid_pause_minutes` in the settings (0 to 30)
- `mqtt_ha_discovery` in the settings (default on); the retained `<prefix>/<uuid>/status` topic (`online`, or `offline` as the last will); discovery configs under `homeassistant/`

### Fixes
- With the lid pause on, "too low" could beep right away instead of after 30 seconds below the minimum (about half the time)
- Install from GitHub with an admin password set but not filled in: the confirmation closed and only a small line said why. The app now asks for the password before the confirmation, and scrolls to the field
- When the browser closed the confirmation by itself (Android's back gesture, Escape twice), Install and Apply did nothing until the page was reloaded

## 2026-11-22.1
Alarm dialog, food presets, CSV export and a reset for a forgotten admin password. Grills on 2026-11-22 find this release under Settings > Firmware updates and install it with one tap; older versions install the `-ota.bin` file there.

### Web app
- When an alarm goes off, the app opens a dialog on every page that says which probe and why ("Pulled pork is ready · 95.2°C · target 95°"), with Mute and Close. Close hides it until the next alarm and leaves the grill beeping
- While the page is open it beeps along with the grill, vibrates on Android and shows a bell in the tab title. Settings > This browser switches the sound off for that browser and has a test sound. With the sound on, the page keeps checking every 5 seconds when it's in the background
- Food presets in the probe editor: pick a food, such as steak medium-rare, brisket or chicken breast, and the target temperature fills in, in °C or °F. A probe that still has its default name gets the food's name
- Download CSV in the probe editor saves the whole cook as a spreadsheet file: time and temperature, named after the probe, for example grilly-plus-pulled-pork-2026-11-22-1405.csv

### Updates and security
- Forgot the admin password? Settings > Firmware updates has a link that makes the grill show a 6-digit code on its screen for 5 minutes. Entering it in the app sets a new admin password, or removes it, and keeps every other setting
- A code works once and ends after 5 wrong tries; after 15 wrong tries in total no new code is given until the grill restarts; Cancel in the app or a short press on the button takes the code off the screen

### Grill screen
- Probe names are cut to their space with a ".": the 1-probe screen stops before the target bar, the 2-probe screen before the timer, the 4-probe screen inside its cell
- The 2-probe screen shows the time to target ("in 1:23") right of the temperature
- The 5-8 probe screen shows one decimal like the other screens
- An update error wraps over three lines instead of being cut at 25 characters
- The Info screen cuts long grill names, WiFi names and hotspot passwords at the edge

### API
- `POST /api/admin/reset/start` (shows the code; 409 without an admin password, 429 within a minute of the last code, 423 after too many wrong codes) and `POST /api/admin/reset` with `{"code": "…", "admin_password": "…"}` (403 wrong code with `attempts_left`, 410 no valid code), `POST /api/admin/reset/cancel`; `admin_reset` in the `/api/info` capabilities. Not over MQTT

## 2026-11-22
Update from GitHub. Install this release from Settings > Firmware updates with the `-ota.bin` file; from then on the grill finds and installs new releases itself.

### New
- Update from GitHub: the grill checks the Grilly+ releases once a day and when you tap "Check now". The web app shows "*version* is available" with the release notes, a dot on the Settings tab, and an Install button
- Install restarts the grill into an update mode (WiFi only, dimmed screen, progress bar), downloads the release over HTTPS, checks its size and SHA-256 checksum and restarts into the new version in about a minute. On any failure it keeps the old version and the app shows the reason
- Installing needs at least 30 % battery or the charger, and the admin password when one is set
- The firmware updates card is ordered by the steps: current admin password, update from GitHub, install from a file, and the admin password settings at the bottom
- Confirmations (installing an update, applying network settings) use the app's own dialog instead of the browser's
- Edge no longer shows a second show/hide button in password fields

### API and MQTT
- `GET /api/update/latest`, `POST /api/update/check` (202, 429 when asked again within a minute) and `POST /api/update/install` with `{"version": "…"}` (admin password; 409 when it isn't the offered version, 412 when the battery is too low)
- `update_available` in `/api/grill` and the MQTT `grill` topic; `github_update` in the `/api/info` capabilities

### Other
- Version comparison, release parsing and the battery check have unit tests that run in CI

## 2026-09-28.2
Temperature graphs and time to target. Install it from Settings > Firmware updates with the `-ota.bin` file.

### New
- Temperature graph on every probe card: the whole cook, with the target line or range band, and under it the time span and the temperature range
- Large chart in the probe editor (tap a card): temperature labels and grid, a "30 min / Whole cook" switch, and "Clear history" (two taps)
- Time to target: "ready in ~1h 20m" on the card and in the editor, "in 1:20" on the grill's probe screen, and `eta_seconds` in `/api/grill`, `/api/probes` and MQTT. Worked out from the last 10 minutes in target mode; after a sharp drop (lid opened, probe taken out) it's back after about 3 minutes of climbing
- The grill keeps a temperature history per probe: the last 30 minutes at 10-second steps plus the whole cook at a coarser step, in a fixed ~5 KB. It's kept until a restart, until a probe has been unplugged for more than 10 minutes, or until you clear it
- On the grill's screen, the label of a probe whose alarm is sounding blinks
- Low-battery protection: at 5 % or 3.2 V (not charging, confirmed for 15 seconds) the grill beeps three times and switches off cleanly; About shows "Battery empty"
- Settings in two fixed columns on desktops, so a tall card no longer leaves a gap
- The probe editor explains that the calibration offset belongs to the socket, not to the probe

### API and MQTT
- `GET /api/history` (whole cook of all connected probes) and `GET /api/history?probe=N` (plus the last 30 minutes), streamed
- `POST /api/history/clear` with `{"probe_id": N}`, and MQTT `<prefix>/<uuid>/config/clear_history`
- `GET /api/info`: firmware, version, API version and `capabilities`, for apps and integrations
- `last_off_reason` can be `low_battery`

### Other
- Firmware logic (history, time to target, low battery) has unit tests that run in CI
- README: new screenshots, the new features, and a note that Grilly+ is built with AI coding agents

## 2026-09-28.1
Hotfix for 2026-09-28. Everyone who installed 2026-09-28 should update: install it from Settings > Firmware updates with the `-ota.bin` file.

### Fixes
- After upgrading to 2026-09-28 from an older version, all probes showed as unplugged. The new calibration offset was missing from the stored settings and was read as "not a number", which made every reading invalid. A missing or invalid offset now counts as 0, and the stored settings repair themselves on the next save

## 2026-09-28
Tested on a real Grilleye Max. Install it from Settings > Firmware updates with the `-ota.bin` file.

### New
- Reach the grill by name: `http://grilly-plus-xxxxxxxx.local` (mDNS), on your home WiFi and on its own hotspot. The name is on the device's Info screen and the About page, and your router shows it too
- Mute a sounding alarm from the web app (a bar on every page) or over MQTT (`<prefix>/<uuid>/config/mute`)
- The card of the probe that triggered an alarm is highlighted, and `/api/grill` reports `alarm_sounding` and a per-probe `alarm`
- Calibration offset per probe (-10 to +10 °C) in the probe editor, for probes that read a little high or low
- Diagnostics on the About page: why the grill last restarted or switched off, and the battery voltage (`last_reset_reason`, `last_off_reason`, `battery_millivolts` in `/api/grill`)
- Show/hide button on every password field
- The About page shows the hotspot name and address as well as the WiFi address
- Favicon and iPhone home screen icon
- Probe cards are shown two per row on tablets and desktops, so up to 8 probes always fill whole rows

### Fixes
- A failed battery reading is detected and ignored instead of showing 65535 %, so the low battery warning works again
- The early warning beep ("degrees before") no longer pauses probe readings for 2.4 seconds or cuts off other beeps
- The power button is debounced, so electrical noise can't cut a long press short or fake a press
- Data shared between the firmware's tasks (settings, probe names, IP address) is locked while it's changed, preventing rare crashes
- More stack for the probes, alarm and battery tasks
- After a wrong current admin password, correcting it retries the change without typing the new password again

### Other
- Every pull request is built and tested automatically (GitHub Actions)

## 2026-09-27
First Grilly+ release, a fork of Free-Grilly 2026-04-18. Settings are kept when upgrading from Free-Grilly (same settings storage), and the first update can be installed through Free-Grilly's existing /update page.

### New web app
- New web app, built for phones and desktops alike, replacing the Bootstrap pages
    - One dashboard with a card per connected probe: temperature, target or range, progress, "to go" / ready / in range / too low / too high, and how long the probe has been connected
    - Tap a probe card to edit its name, alarm (off, target or range) and probe type, with large steppers; reference values are only shown for custom probes
    - Settings save themselves; network changes (WiFi and hotspot) are collected and applied together with one button after a confirmation
    - WiFi scan in the Network card, sorted by signal
    - Battery (with charging) and WiFi signal indicators with icons, and a banner when the grill can't be reached
    - Dark and light theme following your device, floating navigation bar on phones
    - About view with firmware and connection details and credits to Free-Grilly
    - One compressed page of about 19 KB instead of about 311 KB, cached by the browser until the next firmware update; frees about 290 KB of flash
    - Old page urls (/probes, /settings, /about, /update) redirect into the app
- Save errors are shown instead of "saved successfully", and the entered values are kept

### Firmware updates and security
- Firmware updates are installed from the Settings view (Firmware updates card) with upload progress; the grill restarts by itself and the page reloads. ElegantOTA is removed
- Optional admin password protects firmware updates; changing or removing it needs the current one, and it can't be changed over MQTT
- Only real OTA firmware files are accepted, a -full.bin is refused
- New and factory-reset devices get a generated hotspot password, shown on the device's Info screen; upgraded devices keep their current hotspot password
- Passwords are never returned by the api or published over MQTT
- Web pages on other sites can no longer change settings or install firmware (json-only posts, no CORS for writes, required update header)

### Fixes
- The grill boots straight back up after an update, a factory reset or a crash instead of shutting down
- The backlight switches off at shutdown and the power rails stay off while asleep
- Fixed a crash when a probe is unplugged while its details page is shown on the device
- Empty probe sockets no longer flicker between connected and disconnected or show "nan"
- Alarms no longer replay when a probe is saved (for example after renaming it or an MQTT reconnect)
- "Beep when ready" and "Beep outside target" are respected
- Range alarms of 4 degrees or less re-arm again; a minimum of 0 or below means target mode
- The overview for 5 to 8 probes on the device shows the configured unit instead of always Celsius
- Disabling beeps no longer wipes the saved volume
- The Cucaracha alarm setting is saved
- The settings page no longer saves the DHCP address as a static IP; an incomplete static IP falls back to DHCP
- Switching from a static IP back to DHCP works without a reboot
- The gateway is used as DNS fallback for a static IP
- Restart the display timeouts when settings are saved, so a new brightness stays visible
- Missing keys in api and MQTT updates keep their current value instead of being stored as "null" or 0; values are validated before anything is saved
- The api, MQTT and Opengrill tasks no longer share one json document, which could garble messages or crash the device
- WiFi scan lists at most 20 networks so the response always fits

### MQTT and Opengrill
- Changing the MQTT broker, port, topic or credentials takes effect right away and subscribes to the new config topics
- Reconnects back off from 5 seconds to a minute and stop when MQTT or Opengrill is disabled
- Rejected MQTT config messages are reported on `<prefix>/<uuid>/error`
- Saving probes publishes the probes topic
- Opengrill connects on the Opengrill port instead of the MQTT port

### Api
- `/api/grill` returns `connected_seconds` per probe
- `/api/settings` returns `<name>_password_set` instead of passwords
- New `POST /api/update` for firmware updates

### Other
- Renamed to Grilly+: new defaults for fresh installs (name `Grilly+`, mqtt prefix `grilly-plus`, AP name `GrillyPlus_xxxxxx`), MQTT and Opengrill client ids start with `grilly-plus-`
- Firmware release files are named `grilly-plus-yyyy-mm-dd-*.bin`
- The build platform version is pinned so builds are reproducible
- The release script works on Linux, macOS and Windows and stops on errors
- Development tools: a dev server with mock data, and unit tests for the web app and the build script

## 2026-04-18
- Cleaned up MQTT code
- Cleaned up network code
- Applied tweaks and fixes to wifi init in main to drastically improve connection to wifi on cold boot
- Updated mqtt connect/reconnect behavior with better handling
- Added Opengrill settings to
    - Web settings
    - Api
    - Api documentation
    - Config storage management
- Added opengrill handlers in jsonutilities
- Added opengrill task runner in main loop
- Cleaned up mqtt reconnect code in main loop
- Main loop blocks less for mqtt/opengrill receive loop
- Fixed issue with dns lookups failing
- cleaned up wifi/dns code
- added extra checks that mqtt/opengrill can only connect if wifi connected
- mqtt data is now sent properly when updating probes
- opengrill update is sent when changing the grill name
- opengrill update is sent when probes are updates
- for opengrill we correctly send nullptr for probes that have no minimum temperature

## 2026-03-30
- Fix settings not being able to be updated - #25
- Added easter egg in settings

## 2026-03-22
- Fix display update lock on screen timeout - PR #24 - Thanks @ctrochalakis!
- Fix typo in probe 8 calibration settings loading - PR #23 - Thanks @ctrochalakis!

## 2025-12-27
- Mqtt settings now get applied when changed via api/mqtt/web interface without needing a reload (Enhancement for issue #22)

## 2025-12-22
- Added the option to set the brightness for the backlight of the screen (Enhancement for issue #10)
- The mqtt code now first checks for retained messages from external systems in case of disconnects
- Fixed openapi spec to correctly show probes under the grill endpoint
- Merged Display beta branch (full release notes available under releases)

## 2025-08-26
- Updated the web interface to show the mqtt topics when setting up mqtt
- Updated the documentation with a guide on how to use mqtt

## 2025-08-05
- bugfix for issue #16 due to incorrect buffer sizes

## 2025-08-04 - Deprecated due to bug
- Enhancement for issue #14 to be able to set names for probes
- Fixed but with labels not showing correctly on the main page

## 2025-07-27
- Fix for issue #12 where you could only set custom values for probe 1
- Added support for i-grill probes (Thanks @Robbie1983)

## 2025-07-17
- Fix for issue #7 where Fahrenheit was not displayed on the lcd

## 2025-06-14
- Added the option to set the timeout for the backlight and/or the screen (Enhancement for issue #2)
- Web interface
    - You can now toggle to only view connected probes in the webinterface - Thanks @Bardesss!
    - Added inputs for mqtt and backlight enhancements
- MQTT
    - Fixed bug with string literal used in the wrong location
    - Fixed bug where we forgot to cast to the correct data type
    - Added the option to set a mqtt username and password (Enhancement for issue #4)
- API
    - Added documentation and fields for mqtt user/pass and screen/backlight enhancements

## 2025-06-01
- Fixed bug where stacksize was not big enough to run the factory reset
- Added a config flag that checks if Free-Grilly has internet access

## 2025-05-30

- Internal code refactoring
    - Json data is now in a seperate class for re-use
    - Tweaked heap/stack usage for a more responsive experience
    - And many more
- Added mqtt support
    - free grilly can be configured to use a mqtt broker
- Sending mqtt data
    - send the grill status (wifi/temps) every second `<prefix>/<uuid>/grill`
    - sends a message when probes have been changed `<prefix>/<uuid>/probes`
    - sends a message when settings have been changed `<prefix>/<uuid>/settings`
    - On bootup a message with the retain flag will be published to `probes` and `settings`
- Configurable via mqtt
    - probes configuration can be changed via `<prefix>/<uuid>/config/probes`
    - settings configuration can be changed via `<prefix>/<uuid>/config/settings`
    - Probes and settings will check for a retained mqtt message and apply this if found. Afterwards the retained message will be cleared.
- Mqtt data is sent in json and follows the `openapi` spec
- Web ui has been updated to allow configuring the mqtt broker via the web ui
- API has been updated to allow configuring the mqtt broker via api
- Updated the openapi spec to include the new mqtt data (mqtt topic)

## 2025-05-18

- added support for Maverick ET733 probes (web/backend/api)
- added support for setting a port for the mqtt broker
- Added the mqtt broker url and port to the api + api spec

## 2025-05-16

- firmware version bump to 2025-05-16
- current info screen (wifi/version/..) is now shown after pressing and holding the button for 1 second
- fixed the bug where an alarm sounds when booting or connecting a new probe for probes without a target temperature

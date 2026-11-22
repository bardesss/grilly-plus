// Firmware updates card: admin password, and installing an -ota.bin firmware file.
(() => {
  let version, adminInput, adminRemove, warning, note, fileInput, authField, authInput, updateButton, progress, notice;
  let saver;
  let ghStatus, ghDetails, ghNotes, ghInstall, ghCheck, ghFailed, ghNotice;
  let latest = null;       // last answer of /api/update/latest
  let busy = false;        // a check or install is running
  let retryAdmin = null;   // an admin password change waiting for the right current password
  let resetStart, resetPanel, resetHint, resetCode, resetPassword, resetSubmit, resetCancel, resetNotice;
  let resetUntil = 0;      // Date.now() when the code on the grill's screen runs out
  let resetTimer = null;

  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

  // After the restart the grill takes a few seconds to come back, poll until it answers
  async function waitForGrill(attempts = 45) {
    await sleep(4000);
    for (let attempt = 0; attempt < attempts; attempt++) {
      try {
        const status = await Api.get("/api/grill", 3000);
        return status.firmware_version;
      } catch (error) {
        await sleep(2000);
      }
    }
    return null;
  }

  // Changing the admin password and installing firmware both need the current admin password, when one is set
  function currentPasswordMissing(show) {
    const settings = Settings.current();
    if (!settings || !settings.admin_password_set || authInput.value !== "") return false;
    show("Enter the current admin password first.");
    authInput.focus();
    return true;
  }

  function changeAdmin(password) {
    retryAdmin = password;
    if (currentPasswordMissing((text) => Controls.showNote(note, "error", new Error(text)))) return;
    saver.change({ admin_password: password }, { immediate: true });
  }

  // Forgotten admin password: the grill shows a code on its screen, entering it here sets a new password
  function showResetCountdown() {
    const left = (resetUntil - Date.now()) / 1000;
    if (left <= 0) {
      clearInterval(resetTimer);
      resetTimer = null;
      resetHint.textContent = "The code ran out. Ask for a new one.";
      resetSubmit.disabled = true;
      return;
    }
    resetHint.textContent = "The grill's screen now shows a 6-digit code (valid for " + Format.countdown(left) + "). " +
      "Enter it with a new admin password, or leave the password empty to remove it.";
  }

  async function startReset() {
    resetStart.disabled = true;
    resetNotice.textContent = "";
    try {
      const answer = await Api.post("/api/admin/reset/start", {});
      resetUntil = Date.now() + answer.seconds_left * 1000;
      resetPanel.hidden = false;
      resetStart.hidden = true;
      resetSubmit.disabled = false;
      resetCode.value = "";
      resetPassword.value = "";
      showResetCountdown();
      clearInterval(resetTimer);
      resetTimer = setInterval(showResetCountdown, 1000);
      resetCode.focus();
    } catch (error) {
      resetPanel.hidden = false;
      resetHint.textContent = "";
      resetNotice.textContent = error.message;
    } finally {
      resetStart.disabled = false;
    }
  }

  function closeReset() {
    clearInterval(resetTimer);
    resetTimer = null;
    resetPanel.hidden = true;
    resetStart.hidden = false;
    resetNotice.textContent = "";
    resetCode.value = "";
    resetPassword.value = "";
  }

  // Cancel also takes the code off the grill's screen
  function cancelReset() {
    const codeShowing = resetTimer !== null;
    closeReset();
    if (codeShowing) Api.post("/api/admin/reset/cancel", {}).catch(() => {});
  }

  async function submitReset() {
    const code = resetCode.value.trim();
    if (!/^[0-9]{6}$/.test(code)) {
      resetNotice.textContent = "The code has 6 digits.";
      resetCode.focus();
      return;
    }
    const password = resetPassword.value;
    resetSubmit.disabled = true;
    resetNotice.textContent = "";
    try {
      await Api.post("/api/admin/reset", { code, admin_password: password });
      closeReset();
      authInput.value = password;   // the new password is the current one from now on
      retryAdmin = null;
      Controls.showNote(note, "saved", password ? "Admin password reset" : "Admin password removed");
      const settings = await Api.get("/api/settings");
      Settings.fill(settings);
    } catch (error) {
      const left = error.body && typeof error.body.attempts_left === "number" ? error.body.attempts_left : null;
      if (error.status === 403 && left > 0) {
        resetNotice.textContent = "Wrong code, " + left + (left === 1 ? " try" : " tries") + " left.";
        resetSubmit.disabled = false;
        resetCode.select();
      } else {
        resetNotice.textContent = error.message;
        resetSubmit.disabled = error.status === 403 || error.status === 410;
        if (resetSubmit.disabled) {
          clearInterval(resetTimer);
          resetTimer = null;
          resetHint.textContent = "";
          resetStart.hidden = false;
        }
      }
    }
  }

  function setProgress(fraction) {
    progress.firstElementChild.style.width = Math.round(fraction * 100) + "%";
  }

  async function runUpdate() {
    const file = fileInput.files[0];
    if (!file) return;
    if (currentPasswordMissing((text) => { notice.textContent = text; })) return;
    updateButton.disabled = true;
    fileInput.disabled = true;
    progress.hidden = false;
    setProgress(0);
    notice.textContent = "Uploading…";
    try {
      await Api.upload("/api/update", file, authInput.value, setProgress);
      notice.textContent = "Installing… the grill restarts.";
      const newVersion = await waitForGrill();
      if (newVersion) {
        notice.textContent = "Updated. The grill now runs " + newVersion + ".";
        setTimeout(() => location.reload(), 1500);   // load the web app of the new firmware
      } else {
        notice.textContent = "The grill hasn't come back yet. Reload this page in a minute.";
      }
    } catch (error) {
      notice.textContent = error.message;
      progress.hidden = true;
    } finally {
      fileInput.disabled = false;
      updateButton.disabled = !fileInput.files[0];
    }
  }

  function renderLatest() {
    const view = latest ? Format.updateStatus(latest) : { kind: "unknown", text: "Not checked yet" };
    const available = view.kind === "available";
    ghStatus.textContent = view.text;
    ghStatus.classList.toggle("update-available", available);
    ghDetails.hidden = !available || !latest.notes;
    ghNotes.textContent = available ? latest.notes : "";
    ghInstall.hidden = !available;
    if (available) ghInstall.textContent = "Install " + latest.latest;
    ghInstall.disabled = busy;
    ghCheck.disabled = busy || (latest && latest.state === "checking");
    const failed = latest && latest.last_install_error;
    ghFailed.hidden = !failed;
    ghFailed.textContent = failed ? "The last update failed: " + latest.last_install_error : "";
  }

  async function loadLatest() {
    try {
      latest = await Api.get("/api/update/latest", 5000);
    } catch (error) {
      return;   // keep what we showed
    }
    renderLatest();
  }

  async function checkNow() {
    if (busy) return;
    busy = true;
    ghNotice.textContent = "";
    renderLatest();
    try {
      await Api.post("/api/update/check", {});
      const deadline = Date.now() + 30000;
      await loadLatest();
      while (latest && latest.state === "checking" && Date.now() < deadline) {
        await sleep(2000);
        await loadLatest();
      }
    } catch (error) {
      ghNotice.textContent = error.message;
    } finally {
      busy = false;
      renderLatest();
    }
  }

  async function installFromGitHub() {
    if (busy || !latest || !latest.available) return;
    const version = latest.latest;
    const status = App.getStatus();
    const probes = status && status.probes && status.probes.some((probe) => probe.connected);
    const confirmed = await Dialog.confirm({
      title: "Install " + version + "?",
      message: "The grill restarts into update mode for about a minute. Keep it on the charger or above 30 % battery." +
        (probes ? " The graphs of the connected probes start over." : ""),
      confirm: "Install",
    });
    if (!confirmed || busy) return;
    if (currentPasswordMissing((text) => { ghNotice.textContent = text; })) return;
    busy = true;
    renderLatest();
    ghNotice.textContent = "";
    try {
      await Api.post("/api/update/install", { version }, 8000, authInput.value);
      ghNotice.textContent = "Updating to " + version + "… the grill will be back in about a minute";
      const newVersion = await waitForGrill(85);
      if (newVersion === version) {
        ghNotice.textContent = "Updated to " + version + ".";
        setTimeout(() => location.reload(), 1500);   // load the web app of the new firmware
        return;
      }
      if (newVersion) {
        await loadLatest();
        // The failure line under the status already shows the reason; only say something when it can't
        ghNotice.textContent = latest && latest.last_install_error ? "" : "The update failed for an unknown reason.";
      } else {
        ghNotice.textContent = "The grill hasn't come back yet. Check its screen and reload in a minute.";
      }
    } catch (error) {
      ghNotice.textContent = error.message;
    } finally {
      busy = false;
      renderLatest();
    }
  }

  function build() {
    const card = document.createElement("section");
    card.className = "card settings-card";
    card.innerHTML =
      '<h2 class="card-title"><span>Firmware updates</span><span class="save-note" role="status"></span></h2>' +
      '<p>Installed version: <strong data-version></strong></p>' +
      '<label class="field" data-auth-field><span>Current admin password</span><input type="password" autocomplete="current-password" data-auth></label>' +
      '<button type="button" class="link-button" data-reset-start>Forgot the admin password?</button>' +
      '<div class="reset-panel" data-reset-panel hidden>' +
      '<p class="hint" data-reset-hint></p>' +
      '<label class="field"><span>Code from the grill&#39;s screen</span>' +
      '<input type="text" inputmode="numeric" autocomplete="one-time-code" maxlength="6" data-reset-code></label>' +
      '<label class="field"><span>New admin password</span><input type="password" autocomplete="new-password" data-reset-password></label>' +
      '<div class="update-actions">' +
      '<button type="button" class="button primary" data-reset-submit>Reset password</button>' +
      '<button type="button" class="link-button" data-reset-cancel>Cancel</button>' +
      '</div>' +
      '<p class="notice" role="status" data-reset-notice></p>' +
      '</div>' +
      '<h3 class="subheading first">Update from GitHub</h3>' +
      '<p class="update-status" role="status" data-gh-status>Not checked yet</p>' +
      '<details class="update-notes" data-gh-details hidden><summary>What&#39;s new</summary><p data-gh-notes></p></details>' +
      '<p class="warning" data-gh-failed hidden></p>' +
      '<div class="update-actions">' +
      '<button type="button" class="button primary" data-gh-install hidden>Install</button>' +
      '<button type="button" class="link-button" data-gh-check>Check now</button>' +
      '</div>' +
      '<p class="notice" role="status" data-gh-notice></p>' +
      '<h3 class="subheading">Install from a file</h3>' +
      '<label class="field"><span>Firmware file</span><input type="file" accept=".bin" data-file></label>' +
      '<p class="hint">Use the grilly-plus-…-ota.bin file from the releases page.</p>' +
      '<button type="button" class="button primary" data-update disabled>Update</button>' +
      '<div class="progress upload" hidden><i></i></div>' +
      '<p class="notice" role="status" data-file-notice></p>' +
      '<h3 class="subheading">Admin password</h3>' +
      '<p class="hint">Protects firmware updates. Changing or removing it needs the current one above.</p>' +
      '<label class="field"><span>New admin password</span><input type="password" autocomplete="new-password" data-admin></label>' +
      '<button type="button" class="link-button" data-admin-remove>Remove saved password</button>' +
      '<p class="warning" data-warning>No admin password is set. Anyone on your network can install firmware.</p>';

    version = card.querySelector("[data-version]");
    adminInput = card.querySelector("[data-admin]");
    adminRemove = card.querySelector("[data-admin-remove]");
    warning = card.querySelector("[data-warning]");
    note = card.querySelector(".save-note");
    fileInput = card.querySelector("[data-file]");
    authField = card.querySelector("[data-auth-field]");
    authInput = card.querySelector("[data-auth]");
    updateButton = card.querySelector("[data-update]");
    progress = card.querySelector(".progress");
    notice = card.querySelector("[data-file-notice]");
    ghStatus = card.querySelector("[data-gh-status]");
    ghDetails = card.querySelector("[data-gh-details]");
    ghNotes = card.querySelector("[data-gh-notes]");
    ghInstall = card.querySelector("[data-gh-install]");
    ghCheck = card.querySelector("[data-gh-check]");
    ghFailed = card.querySelector("[data-gh-failed]");
    ghNotice = card.querySelector("[data-gh-notice]");
    resetStart = card.querySelector("[data-reset-start]");
    resetPanel = card.querySelector("[data-reset-panel]");
    resetHint = card.querySelector("[data-reset-hint]");
    resetCode = card.querySelector("[data-reset-code]");
    resetPassword = card.querySelector("[data-reset-password]");
    resetSubmit = card.querySelector("[data-reset-submit]");
    resetCancel = card.querySelector("[data-reset-cancel]");
    resetNotice = card.querySelector("[data-reset-notice]");
    resetStart.addEventListener("click", startReset);
    resetSubmit.addEventListener("click", submitReset);
    resetCancel.addEventListener("click", cancelReset);
    resetCode.addEventListener("keydown", (event) => { if (event.key === "Enter") submitReset(); });
    resetPassword.addEventListener("keydown", (event) => { if (event.key === "Enter") submitReset(); });
    ghInstall.addEventListener("click", installFromGitHub);
    ghCheck.addEventListener("click", checkNow);

    saver = createSaver({
      send: async (fields) => {
        const result = await Api.post("/api/settings", fields, 5000, authInput.value);
        authInput.value = fields.admin_password;   // the new password is the current one from now on
        return result;
      },
      onStatus: (state, detail) => {
        Controls.showNote(note, state, detail);
        if (state === "saved") retryAdmin = null;
        // Only a wrong current password is worth retrying, other errors need a new change
        if (state === "error" && !(detail && detail.status === 401)) retryAdmin = null;
        if (state === "saved" && detail) Settings.fill(detail);
      },
    });
    adminInput.addEventListener("change", () => {
      if (adminInput.value !== "") changeAdmin(adminInput.value);
    });
    adminRemove.addEventListener("click", () => changeAdmin(""));
    // Correcting the current password retries a refused change, without typing the new one again
    authInput.addEventListener("change", () => {
      if (retryAdmin !== null && authInput.value !== "") changeAdmin(retryAdmin);
    });
    fileInput.addEventListener("change", () => { updateButton.disabled = !fileInput.files[0]; notice.textContent = ""; });
    updateButton.addEventListener("click", runUpdate);
    return card;
  }

  function fill(settings) {
    const isSet = !!settings.admin_password_set;
    version.textContent = settings.firmware_version;
    adminInput.placeholder = isSet ? "Saved, type to change" : "Not set";
    if (document.activeElement !== adminInput && !saver.hasPending()) adminInput.value = "";
    adminRemove.hidden = !isSet;
    warning.hidden = isSet;
    authField.hidden = !isSet;
    if (!isSet && !resetPanel.hidden) closeReset();
    resetStart.hidden = !isSet || !resetPanel.hidden;
    if (!busy) loadLatest();
  }

  Settings.addCard({ id: "updates", build, fill });
})();

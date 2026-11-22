// Confirmation dialog in the app's own style, instead of the browser's confirm() box.
// Dialog.confirm({ title, message, confirm, cancel }) resolves to true when confirmed.
const Dialog = (() => {
  let open = null;   // the pending dialog, so a second call can't stack on top of it

  // At the start of the page, not the end: password managers (Bitwarden) keep their own autofill
  // elements last in <body> and move whatever comes after them. Moving a dialog takes it out of the
  // top layer, so it flashed away the moment it opened.
  function place(dialog) {
    document.body.prepend(dialog);
  }

  // Shown with showModal() and still on top. False after something moved it in the page.
  function isModal(dialog) {
    try {
      return dialog.open && dialog.matches(":modal");
    } catch (error) {
      return dialog.open;   // a browser without :modal
    }
  }

  // Back on top after something moved it. The close event this may cause finds it open again.
  function reshow(dialog) {
    if (!dialog.isConnected) place(dialog);
    if (dialog.open) dialog.close();
    dialog.showModal();
  }

  function confirm({ title, message, confirm: confirmText = "OK", cancel: cancelText = "Cancel" }) {
    if (open) return Promise.resolve(false);
    const dialog = document.createElement("dialog");
    dialog.className = "confirm-dialog";
    dialog.setAttribute("aria-labelledby", "confirm-dialog-title");
    dialog.innerHTML =
      '<h2 id="confirm-dialog-title"></h2>' +
      '<p class="confirm-dialog-message"></p>' +
      '<div class="confirm-dialog-buttons">' +
      '<button type="button" class="button" data-cancel></button>' +
      '<button type="button" class="button primary" data-confirm></button>' +
      "</div>";
    dialog.querySelector("h2").textContent = title;
    dialog.querySelector("p").textContent = message;
    const cancelButton = dialog.querySelector("[data-cancel]");
    const confirmButton = dialog.querySelector("[data-confirm]");
    cancelButton.textContent = cancelText;
    confirmButton.textContent = confirmText;
    place(dialog);

    return new Promise((resolve) => {
      open = dialog;
      let restores = 0;
      const watcher = new MutationObserver(() => {
        if (open !== dialog || isModal(dialog) || restores >= 3) return;
        restores++;
        reshow(dialog);
        cancelButton.focus();
      });
      const finish = (answer) => {
        if (open !== dialog) return;
        open = null;
        watcher.disconnect();
        if (dialog.open) dialog.close();
        dialog.remove();
        resolve(answer);
      };
      confirmButton.addEventListener("click", () => finish(true));
      cancelButton.addEventListener("click", () => finish(false));
      // Escape closes the dialog through the cancel event
      dialog.addEventListener("cancel", (event) => { event.preventDefault(); finish(false); });
      // The browser can also close it without a cancelable cancel event (Android's back gesture,
      // Escape pressed twice): without this the dialog stayed "open" here and every later call
      // returned false right away, so buttons that ask for a confirmation did nothing
      dialog.addEventListener("close", () => { if (!dialog.open) finish(false); });
      // A tap on the dimmed backdrop (outside the box) cancels as well
      dialog.addEventListener("click", (event) => {
        const box = dialog.getBoundingClientRect();
        const inside = event.clientX >= box.left && event.clientX <= box.right &&
                       event.clientY >= box.top && event.clientY <= box.bottom;
        if (!inside) finish(false);
      });
      dialog.showModal();
      watcher.observe(document.body, { childList: true, subtree: true });
      cancelButton.focus();   // the safe choice has focus, Enter doesn't restart the grill by accident
    });
  }

  return { confirm, place, isModal, reshow };
})();

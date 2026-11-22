// Confirmation dialog in the app's own style, instead of the browser's confirm() box.
// Dialog.confirm({ title, message, confirm, cancel }) resolves to true when confirmed.
const Dialog = (() => {
  let open = null;   // the pending dialog, so a second call can't stack on top of it

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
    document.body.appendChild(dialog);

    return new Promise((resolve) => {
      open = dialog;
      const finish = (answer) => {
        if (open !== dialog) return;
        open = null;
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
      dialog.addEventListener("close", () => finish(false));
      // A tap on the dimmed backdrop (outside the box) cancels as well
      dialog.addEventListener("click", (event) => {
        const box = dialog.getBoundingClientRect();
        const inside = event.clientX >= box.left && event.clientX <= box.right &&
                       event.clientY >= box.top && event.clientY <= box.bottom;
        if (!inside) finish(false);
      });
      dialog.showModal();
      cancelButton.focus();   // the safe choice has focus, Enter doesn't restart the grill by accident
    });
  }

  return { confirm };
})();

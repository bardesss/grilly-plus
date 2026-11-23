# Flashing guide

## Prerequisites

Before you begin, ensure you have the following:

* **Hardware:**
    * A Grilleye Max device.
    * A USB cable to connect the Grilleye Max to your computer.
* **Software:**
    * A computer running Windows, macOS, or Linux.
    * [CP210x USB to UART Bridge Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) installed. Download and install the drivers appropriate for your operating system.
* **Firmware File:**
    * Download the latest `grilly-plus-<version>-full.bin` file (for example `grilly-plus-2026-11-22.3-full.bin`) from the [Releases page](https://github.com/bardesss/grilly-plus/releases) of this repository. Make sure to use the `full` variant: the `-ota.bin` file is only for updates.

## Flash via the Espressif web interface - short version without screenshots

1. Surf to [https://espressif.github.io/esptool-js/](https://espressif.github.io/esptool-js/) using a chrome based browser Chrome/Edge
2. Under **program** select a baudrate of `115200`
3. Click connect and choose the correct device from the list. Look for a device with **CP2102N** in its name
4. Set the flash address to `0x0` and select the latest `full` firmware.
5. Click on **program** and wait for the programming to finish
6. Refresh the webpage
7. Under console select a baudrate of `115200` and click on **start**
8. Select your correct device again
9. Click the reset button
10. You should now see output in the console
11. Boot your Grilly+ by holding the side button until you hear beeping

## Flash via the Espressif web interface - long version with screenshots

1. Surf to [https://espressif.github.io/esptool-js/](https://espressif.github.io/esptool-js/) using a chrome based browser Chrome/Edge
2. Under **program** select a baudrate of `115200`
    
    ![program](screenshots/program.png "Program")

3. Click connect and choose the correct device from the list. Look for a device with **CP2102N** in its name

    ![device selector](screenshots/deviceselector.png "Device selector")

4. Set the flash address to `0x0` and select the latest `full` firmware.

    ![flash address](screenshots/flash_address.png "Flash address")

5. Click on **program** and wait for the programming to finish

    ![programming finished](screenshots/programming_finished.png "Programming finished")

6. Refresh the webpage
7. Under console select a baudrate of `115200` and click on **start**

    ![console baud](screenshots/console_baud.png "Console baud")

8. Select your correct device again
9.  Click the reset button

    ![console reset and output](screenshots/console_reset.png "Console reset and output")

10. You should now see output in the console
11. Boot your Grilly+ by holding the side button until you hear beeping

## Manual Flashing via USB if the web browser does not work

This process will replace the original firmware on your Grilleye Max.

- Extra prerequisites
    - [Python](https://www.python.org/downloads/) installed.
    - [esptool](https://github.com/espressif/esptool) version 5 or newer: install it via pip with `pip install esptool`.
        - You can also download ready-made binaries for Windows/Linux/macOS: https://github.com/espressif/esptool/releases
        - Older esptool versions (v4) are called as `esptool.py` and use `write_flash` instead of `write-flash`; the rest of the command is the same.

> **Watch out!:** For the first flash you will need to use the `full` variant of the firmware. This contains extra data which is not included in the normal updates (bootloader, partitions,...).

1.  **Connect the Device:** Connect your Grilleye Max to your computer using the USB cable.
2.  **Find Your Serial Port:**
    - **Windows:** Open Device Manager (search `devmgmt.msc`). Look under "Ports (COM & LPT)". You should see an entry like "Silicon Labs CP210x USB to UART Bridge (COMx)". Note the `COMx` number (e.g., `COM3`).
    - **macOS:** Open Terminal and run `ls /dev/cu.*`. Look for something like `/dev/cu.usbserial-XXXX` or `/dev/cu.SLAB_USBtoUART`.
    - **Linux:** Open Terminal and run `ls /dev/ttyUSB*`. Look for something like `/dev/ttyUSB0`.
3.  **Flash the Firmware:**
    - Open your Terminal or Command Prompt.
    - Navigate to the directory where you downloaded the `grilly-plus-<version>-full.bin` file.
    - Run the `esptool` command, replacing `<YOUR_SERIAL_PORT>` with the port name you found in step 2:

        ```bash
        esptool --port <YOUR_SERIAL_PORT> write-flash 0x0 grilly-plus-<version>-full.bin
        ```
        *Example for Linux:* `esptool --port /dev/ttyUSB0 write-flash 0x0 grilly-plus-2026-11-22.3-full.bin`
        *Example for Windows:* `esptool --port COM3 write-flash 0x0 grilly-plus-2026-11-22.3-full.bin`
        *Example for macOS:* `esptool --port /dev/cu.SLAB_USBtoUART write-flash 0x0 grilly-plus-2026-11-22.3-full.bin`

4.  **Wait:** The flashing process will take a minute or two. Do not disconnect the device. `esptool` will indicate when it's complete.
5.  **Reboot:** Once flashing is successful, disconnect and reconnect the USB cable, or power cycle the device. To start the device hold the power button until the Grilleye beeps.

## Updating later

You only need USB for the first install. After that, update from the web app: **Settings**, then **Firmware updates**. Install a new release straight from GitHub, or install a downloaded `-ota.bin` file under **Install from a file**. See [Updating the firmware](../README.md#updating-the-firmware) in the README.

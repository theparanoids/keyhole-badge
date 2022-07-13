# Paranoids DEFCON Badge
> PCB design and firmware for a badge given out at DEFCON 2022

## Table of Contents

- [Background](#background)
- [Install](#install)
- [Usage](#usage)
- [Contribute](#contribute)
- [License](#license)

## Background

The Paranoids have designed a sound-reactive LED electronic conference badge for DEFCON 2022. This repository contains the PCB design files for the badge and the source code to the firmware that runs on the microcontroller.

This badge is designed around a Nordic [nRF52833](https://www.nordicsemi.com/Products/nRF52833) SoC embedded in a Laird [BL653](https://www.lairdconnect.com/wireless-modules/bluetooth-modules/bluetooth-5-modules/bl653-series-bluetooth-51-802154-nfc-module) module. The badge features wireless connectivity (Bluetooth Low Energy in this firmware implementation), a NFC coil, a microphone, buttons, and LEDs.

The firmware is built using the [nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK) and [Zephyr RTOS](https://zephyrproject.org/). It advertises BLE beacons and listens for beacons coming from neighboring badges, lighting up the LEDs according to the number of badges detected. It also blinks the LEDs either in response to sound or in other patterns, which are unlocked by inputting secret codes using the buttons.

## Install

In order to edit the PCB design files, you will need to install [Kicad](https://www.kicad.org/) version 6 or above. The badge design is located in the `eda/` subdirectory. A pogo pin jig used during manufacturing is located in the `jig/` subdirectory. Opening the `.kicad_pro` project file should automatically load both the schematic and PCB design.

If you only want to view the schematic or the final board layout, PDFs of the final version are located in [final-artefacts](final-artefacts).

In order to compile the firmware, you will need to install the [Nordic nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nrf-connect-sdk) v1.9.0. This can be installed according to the official instructions. If you are using nRF Connect for Desktop and the Toolchain Manager, select "Open Terminal" from the arrow dropdown after installing the SDK. From this terminal, the firmware can be built using the following command:

```bash
cd fw/
west build
```

The output will be located in `fw/build/zephyr/zephyr.elf`.

There are miscellaneous scripts written in Python that were used during testing and development. These require Python3, pySerial, NumPy, and scikit-image. These can be installed via pip using the following commands:

```bash
# Create a new Python virtual environment
python3 -m venv venv
# Activate the virtual environment
source venv/bin/activate
# Install libraries
pip install pyserial numpy scikit-image
```

LED positions were computed using [SolveSpace](https://solvespace.com/). This is saved in the [notes.slvs](eda/notes.slvs) file.

## Usage

The Paranoids badge does not contain any kind of bootloader or firmware update functionality, so installing new firmware requires connecting to SWD (Serial Wire Debug) test pads on the back of the badge. This can be done either with a temporary jig or by soldering wires or pin headers. The test points are labeled on the [backside fabrication drawing](final-artefacts/badge/pdf/badge_eda-B_Fab.pdf). Connections that are required are as follows:

* `TP3 / +V` -- Microcontroller supply voltage. This may be labeled as "VCC" or "Vtarget" or similar. Corresponds to pin 1 of the ARM 10-pin connector. **Caution**: This pin is intended to be used as a voltage reference and not as a means to power the badge. Although powering the badge from this test point does work, doing so will bypass the reverse polarity protection circuitry. There is also no protection against back-powering the battery, which can result in a dangerous condition. If power is supplied using this test point, ensure that connections are correct and that batteries are not installed. Nominal voltage 3 V.
* `TP9 / GND` -- Ground reference. Corresponds to pins 3/5/9 of the ARM 10-pin connector.
* `TP5 / SWDCLK` -- SWD clock pin. Corresponds to pin 4 of the ARM 10-pin connector.
* `TP6 / SWDIO` -- SWD data pin. Corresponds to pin 2 of the ARM 10-pin connector.
* `TP4 / ~RST` (optional) -- reset pin (active low). Corresponds to pin 10 of the ARM 10-pin connector. Not required in most cases as reset can be triggered over SWD.
* `TP7 / DBGTX` (optional) -- debug serial port, output from the board. In this firmware, this is configured with a 115200 baud 8n1 serial port for printing debug messages.
* `TP8 / DBGRX` (optional) -- debug serial port, input into the board. In this firmware, this is technically configured as part of the debug console, but input is never read.

All signals are referenced to the voltage on `TP3`.

Programming new firmware requires an external ARM SWD debug probe, such as a [Black Magic Probe](https://1bitsquared.com/products/black-magic-probe) or a [J-Link](https://www.segger.com/products/debug-probes/j-link/). It is also possible to [use a Raspberry Pi](https://learn.adafruit.com/programming-microcontrollers-using-openocd-on-raspberry-pi) or any other compatible debug adapter.

Example commands for programming using a Black Magic Probe:

```
$ arm-none-eabi-gdb build/zephyr/zephyr.elf
(gdb) target extended-remote /dev/cu.usbmodemXXXXXXXX
(gdb) monitor swdp_scan
(gdb) attach 1
(gdb) load
```

### Firmware debugging features

The badge connects the USB pins of the nRF52 chip to a USB micro-B connector on the bottom of the board. In this firmware, this is configured as a CDC-ACM serial port featuring a debug console (different from the one on the test points). Typing `help` will show a list of debug commands.

This USB debug console allows for capturing sound onto a connected computer. To capture sound, send the command `debug sound on`. The badge will then continuously output a stream of audio samples in signed 16-bit little endian format at 16 kHz with no framing. The [test_sound.py](fw/src/test_sound.py) script demonstrates capturing approximately 10 seconds of audio via this mechanism and writing it to a raw file which can be opened in tools such as [Audacity](https://www.audacityteam.org/).

If the flash memory of the nRF52 is blank, the firmware will start in factory test mode. Completion of factory testing is stored in flash memory, so none of the badges given out at DEFCON will start in test mode. As long as the Zephyr NVS data at the end of flash memory is not erased when programming new firmware, the badge will not enter test mode again. Test mode can be either re-entered after completion or forcefully bypassed without completing it by using the USB debug console. The factory test procedure goes through the following phases:

1. Both eyes should be red. The edge LEDs should have a pattern of three lit LEDs animating and moving around in a loop. The other LEDs on the edge should be off. One LED should be blue, the next should be green, and the final lit LED should be red. The primary purpose of this test phase is to ensure that there are no breaks in the LED chain. To proceed to the next phase, press button SW1.
2. Both eyes should be green. One LED on the edge, immediately below the eyes, should be white. All other LEDs on the edge should be off. To proceed to the next phase, press button SW2.
3. Both eyes should be blue. Two LEDs on the edge, immediately below the eyes, should be white. All other LEDs on the edge should be off. To proceed to the next phase, press button SW3.
4. Both eyes should be white. Three LEDs on the edge, immediately below the eyes, should be white. All other LEDs on the edge should be off. To proceed to the next phase, press button SW4.
5. Both eyes should be white. All LEDs on the edge should be white. At this point in the test procedure, the edge LEDs, all three primary colors of the eye LEDs, and all buttons will have been tested. This phase with all LEDs lit also serves as a brief test of maximum current draw in order to try to catch obvious issues with the 5 V SMPS. To proceed to the next phase (finishing and exiting test mode), play a 440 Hz tone. This tests the microphone.

## Contribute

Please refer to [the contributing.md file](Contributing.md) for information about how to get involved.

## Maintainers
- R: rou@yahooinc.com

## License
- This project is licensed under the terms of the [Apache 2.0](LICENSE-Apache-2.0) open source license. Please refer to [LICENSE](LICENSE) for the full terms.

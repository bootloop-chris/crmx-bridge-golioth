# CRMX Bridge

An early demo of the [BootLoop Agent](https://bootloop.ai)!

*This repo is forked from a personal project of mine [https://github.com/chris-markus/crmx-bridge](https://github.com/chris-markus/crmx-bridge). Some clean-up commits added and upgraded to work with the most recent version of the IDF.*

Commit [77b8aad](https://github.com/bootloop-chris/crmx-bridge-golioth/commit/77b8aad5b4f520016bc5265554ae8e2e5ca4bd7b) was entirely generated and tested end-to-end by the BootLoop Agent. Email `chris [AT] bootloop [DOT] ai` for a video of the process.

---

Original README below:

Firmware and hardware for a CRMX bridge based around an ESP32s3 and a [TimoTwo module](https://lumenradio.com/products/timotwo/).

## Caveats

This is a personal project under active development. There are known (and probably unknown) bugs in the hardware. It has been open-sourced for visibility but use the hardware and firmware at your own risk!

### Known Issues

* The power button circuit has no memory outside of the microcontroller, so while downloading firmware, one of two things must be done:
  * Jump J105 to disable the power button circuit and unconditionally power the device while USB is connected
  * Hold down the power button for the duration of the software update

## Contents

* [crmx-bridge](./crmx-bridge/) - Hardware design files for the main PCBA
* [crmx-bridge-hmi](./crmx-bridge-hmi/) - Hardware design files for the Interface PCBA
* [crmx-bridge-firmware](./crmx-bridge-firmware/) - Firmware for the ESP32s3

## Firmware

Note that the firmware is pretty incomplete. It has a subset of functionality (can forward wired DMX to CRMX) but lacks control over most settings via the HMI among other omissions.
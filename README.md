# Backplane Firmware

This is the firmware for the backplane

* Controller-serial - This is the controller firmware, this is connected directly to a raspberry pi as a HAT
* Backplane - This is the backplane firmware, and is used to talk I2C to the controller

## Controller-HTTP
This runs on the arduino that will be used as the controller, it presents an Serial API to turn slots on and off and communicates via I2C to the backplane boards.
Communication to the controller arduino is handled by [spackler](https://github.com/spacklerindustries/spackler)

## Backplane
This runs on each backplane, each backplane module has a unique I2C address that needs to be configured in firmware
Note:
> Future revisions of the PCB will have jumpers/dip switches to set the address

# Limitations
* ~32 kilobytes of memory available on an arduino nano, pushing it to the limits
* Not very fast, as long as there isn't too much happening in the backplane})}}`

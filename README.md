# Backplane Firmware

This is the firmware for the backplane

* ~~Controller - This is the controller firmware, this is used to talk MQTT to the rest of the services~~
* Controller-http - This is the controller firmware, this is used to talk HTTP to the rest of the services
* Backplane - This is the backplane firmware, and is used to talk I2C to the controller

## Controller-HTTP
This runs on the arduino that will be used as the controller, it presents an HTTP API to turn slots on and off and communicates via I2C to the backplane boards.
### Configuration
The IP address of the GreensKeeper server needs to be predetermined and configured into the firmware before hand.
Note:
> I'm looking to have this configurable via an API call

### Power on
The value 2 is used to turn a slot on
```
curl http://{controller-ip}/{i2c]/{slot}/2
```
### Power off
The value 3 is used to soft shutdown a slot (if the OS supports it)
```
curl http://{controller-ip}/{i2c]/{slot}/3
```
### Hard power off
The value 5 is used to immediately shut down a slot (no soft shutdown
```
curl http://{controller-ip}/{i2c]/{slot}/5
```
### Slot status changes
When the backplane sends a status update to the controller, the controller will POST to the greenskeeper server to the `caddydata` API endpoint with the changes.

## Backplane
This runs on each backplane, each backplane module has a unique I2C address that needs to be configured in firmware
Note:
> Future revisions of the PCB will have jumpers/dip switches to set the address

# Limitations
* ~32 kilobytes of memory available on an arduino nano, pushing it to the limits
* Not very fast, as long as there isn't too much happening in the backplane

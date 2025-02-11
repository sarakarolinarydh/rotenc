# Rotary Encoder Driver
The purpose of this project is to create a linux device driver for interfacing
with a standard rotary encoder.
The driver is written for a Raspberry Pi4 with kernel version 6.1.21-v7l+
utilising the device tree overlay.
The current value of the encoder is exposed through the driver device file.

## Description
The encoder has three output pins, channel a, channel b and switch as well as
VCC and GND. The GPIO pins are defined in the device tree overlay and in this
project GPIO7, GPIO17 and GPIO18 are used.

[<img src="rotary_encoder.png" width="250"/>](rotary_encoder.png)

The current value is exposed through the driver device file: `/dev/rotenc`.

To read the value once run this command:

```
sudo cat /dev/rotenc
```

To monitor the value coninuously run:

```
i=0; while [ $i ]; do sudo cat /dev/rotenc; sleep 1; ((i++)); done
```

The value increases when the knob is rotated clockwise and decreases when
rotated counter clockwise. The value is reset to zero when the switch is
pressed.

## Getting Started

### Installing
Before starting, make sure the kernel header files and the compiler for the
device tree overlay are installed.

1. Clone the repository
1. Compile and add the overlay 
1. Run `make all` to build the driver
1. Run `make load` to load the driver

Now the driver is ready to use.

To uninstall the driver, run `make unload` and remove the overlay.

## Authors
Sara Rydh

## License
This project is licensed under the GPL License - see the LICENSE.md
file for details.

# CS4222 Project Task 2

## Compiling

To build the code for the specific device:

1. Ensure that the current directory contains the `receiver.c` and `transmitter.c` files.
2. Run `make [receiver/transmitter]` to build the necessary images
   1. The `Makefile` already includes the `TARGET` and `BOARD` values
3. Flash the image onto the SensorTag using Uniflash

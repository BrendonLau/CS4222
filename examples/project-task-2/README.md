# CS4222 Project Task 2

## Compiling

To build the code for the specific device:

1. Ensure that the current directory contains the `receiver.c` and `sender.c` files.
2. Run `make [receiver/sender]` to build the necessary images
   1. The `Makefile` already includes the `TARGET` and `BOARD` values
   2. If your host machine is MacOS, a helper script (`build_mac.sh`) helps builds the project binaries and suffixes it with a `.bin` extension to make it easier to load into Uniflash
3. Flash the image onto the SensorTag using Uniflash

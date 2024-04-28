# CS4222 Project Task 2

## Compiling

To build the code for the specific device:

1. Ensure that the current directory contains the `task_2_group_8_receiver.c`, `task_2_group_8_transmitter.c` and `task_2_group_8_typedef.h` files.
2. Run `make task_2_group_8_receiver task_2_group_8_transmitter` to build the necessary images
   1. The `Makefile` already includes the `TARGET` and `BOARD` values
   2. If your host machine is MacOS, a helper scdript (`build_mac.sh`) helps builds the project binaries and suffixes it with a `.bin` extension to make it easier to load into Uniflash
3. Flash the image onto the SensorTag using Uniflash

# Task 1 readings

Value of CLOCK_SECOND: 128
Number of clock ticks per second in 1s (real time) using etimer: 128
Value of RTIMER_SECOND: 65536
Number of clock ticks per second in 1s (real time) using rtimer: 65536

# Name & ID of group members

Brendon Lau Cheng Yang A0220999N
Ng Jun Kang A0218325J
Pang Yuan Ker A0227560H
Yusuf Bin Musa A0218228E

# Assignment 2

> [Link to assignment document](https://weiserlab.github.io/wirelessnetworking/Assignment2_v3.pdf)

## Set up

1. Recursively clone the repository: `git clone https://github.com/BrendonLau/CS4222.git --recursive`
2. Build the necessary binary: `make <filename>`
   1. The `TARGET` and `BOARD` variables have been set in the `Makefile` for convenience
3. Flash the binary using [Uniflash](https://www.ti.com/tool/download/UNIFLASH)
   1. Device: `CC2650F128 (On-Chip)`
   2. Connection: `Texas Instrument XDS110 USB Debug Probe`
4. View the debugging console using
   1. MacOS: `make PORT=</dev/tty.usbmodem*> login`
      1. May have to trial and error to figure out which `/dev/tty.usbmodem*` to use, display possibilities using `ls /dev/tty*`
   2. Windows: TODO
5. Run the image by clicking `Verify image`

> [!NOTE]
> When flashing on MacOS, you would need to add a `.bin` suffix at the end of the binary in order to select it through Uniflash

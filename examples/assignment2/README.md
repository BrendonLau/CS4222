# CS4222 Assignment 2

> [Link to assignment document](https://weiserlab.github.io/wirelessnetworking/Assignment2_v3.pdf)

## Task 1

|                                                 |       |
| ----------------------------------------------- | ----- |
| `CLOCK_SECOND` value                            | 128   |
| `etimer` clock ticks per second in 1s real time | 128   |
| `RTIMER_SECOND` value                           | 65536 |
| `rtimer` clock ticks per second in 1s real time | 65536 |

## Set up

1. Recursively clone the repository: `git clone https://github.com/BrendonLau/CS4222.git --recursive`
2. Build the necessary binary: `make <filename>`
   * The `TARGET` and `BOARD` variables have been set in the `Makefile` for convenience
3. Flash the binary using [UniFlash](https://www.ti.com/tool/download/UNIFLASH)
   * Device: `CC2650F128 (On-Chip)`
   * Connection: `Texas Instrument XDS110 USB Debug Probe`
4. View the debugging console on
   * MacOS: `make PORT=</dev/tty.usbmodem*> login`
      * May have to trial and error to figure out which `/dev/tty.usbmodem*` to use, display possibilities using `ls /dev/tty*`
   * Windows: Refer to step 3.3 in [this document](https://weiserlab.github.io/wirelessnetworking/Assignment1_Windows.pdf).
5. Run the image by clicking `Verify Image` on UniFlash or using the sensor on the board.

> [!NOTE]
> When flashing on MacOS, you would need to add a `.bin` suffix at the end of the binary in order to select it through Uniflash

## Group members

| Name                   | Student ID |
| ---------------------- | ---------- |
| Brendon Lau Cheng Yang | A0220999N  |
| Ng Jun Kang            | A0218325J  |
| Pang Yuan Ker          | A0227560H  |
| Yusuf Bin Musa         | A0218228E  |

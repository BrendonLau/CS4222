#!/bin/bash

# Helper script to build the project and suffix the output files with .bin
# This is useful for Uniflash on MacOS to identify the files to flash

rm -rf *.cc26x0-cc13x0
make sender
mv sender.cc26x0-cc13x0 sender.cc26x0-cc13x0.bin

make receiver
mv receiver.cc26x0-cc13x0 receiver.cc26x0-cc13x0.bin


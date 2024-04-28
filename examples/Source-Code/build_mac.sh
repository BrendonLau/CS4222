#!/bin/bash

# Helper script to build the project and suffix the output files with .bin
# This is useful for Uniflash on MacOS to identify the files to flash

rm -rf *.cc26x0-cc13x0
make task_2_group_8_receiver task_2_group_8_transmitter
mv task_2_group_8_transmitter.cc26x0-cc13x0 task_2_group_8_transmitter.cc26x0-cc13x0.bin
mv task_2_group_8_receiver.cc26x0-cc13x0 task_2_group_8_receiver.cc26x0-cc13x0.bin


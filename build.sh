#!/bin/bash
gcc -o custom_frames.so custom_frames.c $(yed --print-cflags) $(yed --print-ldflags)

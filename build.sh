#!/bin/sh

set -xe

# cc -o stb_truetype.o -c stb_truetype.c -O2
# cc -o stb_image.o -c stb_image.c -O2

cc -o main main.c stb_truetype.o stb_image.o -ggdb -lm

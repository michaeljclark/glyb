#!/bin/bash

video=build/video

test -d ${video}/ppm || mkdir -p ${video}/ppm
test -d ${video}/png || mkdir -p ${video}/png

for f in ${video}/ppm/*ppm ; do convert -quality 100 $f ${video}/png/`basename $f ppm`png ; done

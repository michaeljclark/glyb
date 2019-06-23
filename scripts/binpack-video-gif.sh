#!/bin/bash

video=build/video

test -d ${video}/ppm || mkdir -p ${video}/ppm
test -d ${video}/gif || mkdir -p ${video}/gif

rm -f ${video}/ppm/* ${video}/gif/binpack.gif

./build/glbinpack --offline \
	--frame-size 1024x576 --frame-step 5 --frame-count 10000 \
	--template "${video}/ppm/binpack-%09d.ppm"

ffmpeg -framerate 25 -i "${video}/ppm/binpack-%09d.ppm" \
	-s 1024x576 -f gif -loop 1000 \
	${video}/gif/binpack.gif -hide_banner

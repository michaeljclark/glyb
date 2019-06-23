#!/bin/bash

video=build/video

test -d ${video}/ppm || mkdir -p ${video}/ppm
test -d ${video}/mp4 || mkdir -p ${video}/mp4

rm -f ${video}/ppm/* ${video}/mp4/glyphic-*.mp4

./build/glyphic --offline --quadruple \
	--frame-size 1920x1080 --frame-rate 60 \
	--frame-skip 900 --frame-count 900 \
	--template "${video}/ppm/glyphic-%09d-regular.ppm"

ffmpeg -framerate 25 -i "${video}/ppm/glyphic-%09d-regular.ppm" \
	-s 1920x1080 -crf 0 \
	-preset veryslow -threads 36 \
	${video}/mp4/glyphic-regular.mp4 -hide_banner

./build/glyphic --offline --quadruple --enable-msdf \
	--frame-size 1920x1080 --frame-rate 60 \
	--frame-skip 900 --frame-count 900 \
	--template "${video}/ppm/glyphic-%09d-msdf.ppm"

ffmpeg -framerate 25 -i "${video}/ppm/glyphic-%09d-msdf.ppm" \
	-s 1920x1080 -crf 0 \
	-preset veryslow -threads 36 \
	${video}/mp4/glyphic-msdf.mp4 -hide_banner

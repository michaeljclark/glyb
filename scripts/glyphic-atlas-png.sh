#!/bin/bash

atlas=build/atlas

test -d ${atlas}/png || mkdir -p ${atlas}/png

rm -f ${atlas}/png/*

./build/glyphic --offline --quadruple \
	--frame-skip 100 --frame-count 1 \
	--template "/dev/null" \
	--dump-atlases "${atlas}/png/glyphic-%09d-regular.png"

./build/glyphic --offline --quadruple --enable-msdf \
	--frame-skip 100 --frame-count 1 \
	--template "/dev/null" \
	--dump-atlases "${atlas}/png/glyphic-%09d-msdf.png"

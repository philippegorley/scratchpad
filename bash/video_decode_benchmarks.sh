#!/usr/bin/env bash

function usage() {
    echo "Usage: ${FUNCNAME[0]} <input_video>"
}

if [ ! -f $1 ]; then
    usage
    exit
fi

INPUT_VIDEO=$1
VIDEO_CODEC=`ffprobe -v error -select_streams v:0 -show_entries stream=codec_name -of default=nokey=1:noprint_wrappers=1 $INPUT_VIDEO`

for DECODER_NAME in libvpx vp8
do
    for NUM_THREADS in 1 2 3 4
    do
        rm -f /tmp/out.yuv
        echo "Decoder $DECODER_NAME with $NUM_THREADS threads"
        time ffmpeg -v 0 -threads $NUM_THREADS -c:v $DECODER_NAME -i $INPUT_VIDEO /tmp/out.yuv
        echo ""
    done
done

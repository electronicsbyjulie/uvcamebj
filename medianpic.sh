#!/bin/bash

ls -1 /home/pi/Pictures/ > /tmp/pixls
count=$(wc -l /tmp/pixls | sed 's/[^0-9]*//g')
medianidx=$((count/2))
medfn=$(head /tmp/pixls -n $medianidx | tail -n 1)
meddt=${medfn:0:8}
echo $meddt
date -d $meddt +"%Y %b %d"


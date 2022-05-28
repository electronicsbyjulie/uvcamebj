#!/bin/bash

echo $$ > /tmp/czpid

while [ 1 ]
do
    isrun=$(ps -ef | grep ctrlr | grep -v grep)
    
    if [ "$isrun" = "" ]
    then
        /home/pi/ctrlr &
    fi
    
    sleep 0.25
done

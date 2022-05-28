#!/bin/bash

if [[ -f "/tmp/booted" ]]
then
exit 
fi

echo "yep" > "/tmp/booted"

~/ctrlr


#!/bin/bash

sudo pkill unclutter
sudo pkill raspistill
sudo kill "$(< /tmp/czpid)"
sudo pkill ctrlr
# sudo pkill unclutter
# sudo pkill raspistill


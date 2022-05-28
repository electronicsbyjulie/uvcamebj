#!/bin/bash

cd ~

wget --no-check-certificate 'https://docs.google.com/uc?export=download&id=1DnXqi4O7V8wqeSImf1BjE1614ApuLuBg' -O /tmp/src.tar.gz

czsum=$(md5sum -b /tmp/src.tar.gz)
czsmf=$(cat ~/srcmd5)

if [[ "$czsum" != "$czsmf" ]]
then
	cp /tmp/src.tar.gz ~/src.tar.gz

	tar -zxvf src.tar.gz

	sudo chmod +x *.sh
	
	echo $czsum > ~/srcmd5

	piv=$(cat /proc/cpuinfo | grep Revision)
	if [[ ${piv:11:6} < "a020d3" ]]
	then 
		sed -i 's/\-lbcm2835//g' ~/makefile
	fi

	make
	
	sudo pkill ctrlr
fi

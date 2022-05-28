#!/bin/bash

# These have to be done manually
# sudo apt-get install -y realvnc-vnc-server
# sudo apt-get install -y gedit
# sudo apt-get install -y rclone
# sudo apt-get install -y unclutter
# sudo apt-get install -y libgtk-3-dev
# sudo apt-get install -y libi2c-dev
# sudo apt-get install -y libjpeg-dev
# sudo apt-get install -y libpng-dev

wget --no-check-certificate 'https://docs.google.com/uc?export=download&id=1DnXqi4O7V8wqeSImf1BjE1614ApuLuBg' -O ~/src.tar.gz

tar -zxvf src.tar.gz

piv=$(cat /proc/cpuinfo | grep Revision)
if [[ ${piv:11:6} < "a020d3" ]]
then 
	sed -i 's/\-lbcm2835//g' ~/makefile
fi

make

echo "kneads utdape" > ~/srcmd5

ln -s /home/pi/ctrlr /home/pi/Desktop/ctrlr
echo "sudo pkill raspi" > /home/pi/Desktop/kill_overlay.sh
echo "cd ~\n~/swupd.sh" > /home/pi/Desktop/software update.sh

sudo chmod +x *.sh
sudo chmod +x /home/pi/Desktop/kill_overlay.sh

#write out current crontab
sudo crontab -l > /tmp/mycron
#echo new cron into cron file
echo "06,13,19,28,31,35,43,49,58 * * * * /bin/bash /home/pi/gdbkp.sh >> /home/pi/cron.log" >> /tmp/mycron
echo "04 * * * * tail -n 3000 /home/pi/cron.log > /home/pi/cron.log" >> /tmp/mycron
echo "08 * * * * tail -n 3000 /home/pi/act.log > /home/pi/act.log" >> /tmp/mycron
echo "16 * * * * tail -n 3000 /home/pi/bkp.log > /home/pi/bkp.log" >> /tmp/mycron
#install new cron file
sudo crontab /tmp/mycron
rm /tmp/mycron

echo "tmpfs /tmp tmpfs nodev,nosuid,size=128M 0 0" | sudo tee -a /etc/fstab

echo "@xset s off" | sudo tee -a /etc/xdg/lxsession/LXDE-pi/autostart
echo "@xset -dpms" | sudo tee -a /etc/xdg/lxsession/LXDE-pi/autostart
echo "@xset s noblank" | sudo tee -a /etc/xdg/lxsession/LXDE-pi/autostart
echo "@/bin/bash /home/pi/c1.sh" | sudo tee -a /etc/xdg/lxsession/LXDE-pi/autostart

# sudo mkdir -p /home/pi/.config/lxsession/LXDE
# cat /etc/xdg/lxsession/LXDE-pi/autostart | sudo tee /home/pi/.config/lxsession/LXDE/autostart
# echo "@/bin/bash /home/pi/c1.sh" | sudo tee -a /home/pi/.config/lxsession/LXDE/autostart

cd ~
wget https://raw.githubusercontent.com/adafruit/Raspberry-Pi-Installer-Scripts/master/adafruit-pitft.sh
chmod +x adafruit-pitft.sh
sudo ./adafruit-pitft.sh

sudo reboot




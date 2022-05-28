#!/bin/bash

sudo renice -n 10 $$

if pgrep -x "imgcomb" > /dev/null
then
    echo "ok" > /dev/null
else
    sudo rm /tmp/*.jpg
    sudo rm /tmp/*.bmp
fi

amonline=$(/sbin/ifconfig | grep "inet " | grep "cast ")
if [[ -z $amonline ]]; then exit; fi

# sudo pkill rclone

if pgrep -x "rclone" > /dev/null
then
    d="$(date '+%Y%m%d %H%M%S')"
    echo "$d rclone already running" | sudo tee -a /home/pi/bkp.log
    exit
fi

d="$(date '+%Y%m%d %H%M%S')"
echo "$d gdbkp begin" | sudo tee -a /home/pi/bkp.log

# source code
/home/pi/gdsrc.sh

sudo pkill rclone

rems=$(sudo rclone listremotes)

for remm in $rems
do
    Remote="";
    for morceau in $(echo $remm | tr ":" "\n")
    do
        Remote=$morceau
        break
    done
    
    echo "Sync'ing to $Remote..."
    
    # today's files
    d="$(date +%Y%m%d)"
    f="${d:0:7}"
    sudo rclone copy -v --include "$d.*.*" /home/pi/Pictures/ $Remote:computers/$HOSTNAME/pix/$d | sudo tee -a /home/pi/bkp.log
    sudo rclone copy -v --include "$d.*.*" /home/pi/Videos/ $Remote:computers/$HOSTNAME/pix/$d | sudo tee -a /home/pi/bkp.log
done

# yesterweek's files
for dayz in {1..7}
do
    for remm in $rems
    do
        Remote="";
        for morceau in $(echo $remm | tr ":" "\n")
        do
            Remote=$morceau
            break
        done
        
        echo "Sync'ing to $Remote..."


        # d="$(date -d 'yesterday 13:00' '+%Y%m%d')"
        d="$(date -d "$dayz days ago 13:00" '+%Y%m%d')"
        f="${d:0:7}"
        echo "Backing up $d"
        sudo rclone copy -v --include "$d.*.*" /home/pi/Pictures/ $Remote:computers/$HOSTNAME/pix/$d | sudo tee -a /home/pi/bkp.log
        sudo rclone copy -v --include "$d.*.*" /home/pi/Videos/ $Remote:computers/$HOSTNAME/pix/$d | sudo tee -a /home/pi/bkp.log
    done
done

d="$(date '+%Y%m%d %H%M%S')"
echo "$d gdbkp end" | sudo tee -a /home/pi/bkp.log


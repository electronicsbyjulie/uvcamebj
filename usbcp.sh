

devices=$(ls /media/pi | tr ";" "\n")

for usbd in $devices
do
    echo "$usbd"
    destdir="/media/pi/$usbd/infrared"
    
    files=$(ls -1 /home/pi/Pictures/*.png)
    lastfd=""
    for fname in $files
    do
        # echo "from file name $fname "
        fd="${fname:18:8}"
        # echo "found fd $fd\n"
        
        if [ "$fd" != "$lastfd" ]
        then
           
            echo "fd $fd dosent makch $lastfd \n"
        
            mkdir -p "$destdir/$fd"
            cp -n -v /home/pi/Pictures/$fd*.png "$destdir/$fd"
            cp -n -v /home/pi/Videos/$fd*.mp4 "$destdir/$fd"
        fi
        
        lastfd="$fd"
    done
    
    devsda=$(df -h | grep "/media/pi/$usbd" | tr " " "\n")
    echo "$devsda"
    for sda in $devsda
    do
        echo "$sda"
        umount "$sda"
        sudo eject "$sda"
        break
    done
    
    break
done


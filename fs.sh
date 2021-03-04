if [ ! -f "fs.img" ]; then 
    dd if=/dev/zero of=fs.img bs=512k count=1024
    mkfs.vfat -F 32 fs.img
fi

sudo mount fs.img /mnt
sudo mkdir /mnt/bin
sudo cp ./xv6-user/_init /mnt/init
sudo cp ./xv6-user/_sh /mnt/sh
sudo cp ./xv6-user/_cat /mnt/bin/cat
sudo cp ./xv6-user/_echo /mnt/bin/echo
sudo cp ./xv6-user/_grep /mnt/bin/grep
sudo cp ./xv6-user/_ls /mnt/bin/ls
sudo cp ./xv6-user/_kill /mnt/bin/kill
sudo cp ./xv6-user/_mkdir /mnt/bin/mkdir
sudo cp ./xv6-user/_xargs /mnt/bin/xargs
sudo cp ./xv6-user/_rm /mnt/bin/rm
sudo cp ./xv6-user/_sleep /mnt/bin/sleep
sudo cp ./xv6-user/_find /mnt/bin/find
sudo umount /mnt

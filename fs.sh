dd if=/dev/zero of=fs.img bs=512k count=2048
mkfs.vfat -F 32 fs.img

sudo mount fs.img /mnt
sudo mkdir /mnt/xv6sh
sudo cp ./xv6-user/_init /mnt/init
sudo cp ./xv6-user/_sh /mnt/sh
sudo cp ./xv6-user/_cat /mnt/xv6sh/cat
sudo cp ./xv6-user/_echo /mnt/xv6sh/echo
sudo cp ./xv6-user/_grep /mnt/xv6sh/grep
sudo cp ./xv6-user/_ls /mnt/xv6sh/ls
sudo cp ./xv6-user/_kill /mnt/xv6sh/kill
sudo cp ./xv6-user/_mkdir /mnt/xv6sh/mkdir
sudo cp ./xv6-user/_xargs /mnt/xv6sh/xargs
sudo cp ./xv6-user/_rm /mnt/xv6sh/rm
sudo cp ./xv6-user/_sleep /mnt/xv6sh/sleep
sudo cp ./xv6-user/_find /mnt/xv6sh/find
sudo umount /mnt

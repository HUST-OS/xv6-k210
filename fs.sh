dd if=/dev/zero of=fs.img bs=512k count=2048
mkfs.vfat -F 32 fs.img

sudo mount fs.img /mnt
sudo cp ./xv6-user/_init /mnt/init
sudo cp ./xv6-user/_sh /mnt/_sh
sudo cp ./xv6-user/_cat /mnt/cat
sudo cp ./xv6-user/_echo /mnt/echo
sudo cp ./xv6-user/_grep /mnt/grep
sudo cp ./xv6-user/_ls /mnt/ls
sudo cp ./xv6-user/_kill /mnt/kill
sudo cp ./xv6-user/_mkdir /mnt/mkdir
sudo cp ./xv6-user/_xargs /mnt/xargs
sudo umount /mnt


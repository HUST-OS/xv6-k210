# 文件系统

鉴于SD卡多是FAT32文件系统，我们将xv6原本的文件系统替换成了FAT32文件系统，并且尽可能地与原文件系统的接口保持一致或类似。目前在qemu平台上可以运行用户程序，包括shell。

## 修改了什么

对于文件系统，我们保留了xv6的disk、buf和file descriptor这几层的实现，而大体上做了以下修改：

+ FAT32不支持日志系统，我们去掉了xv6文件系统的log层（log.c）；
+ FAT32没有inode，文件的元数据直接存放在目录项中，因此我们去掉了`struct inode`，替换为目录项`struct dirent`（directory entry）；
+ FAT32没有link，因此删除了相关的系统调用；
+ 重新实现xv6文件系统（fs.c）中的各个函数，将函数接口中的inode替换成了entry，函数命名上保留原函数的特征但也做了修改以示区分，如`ilock`变为`elock`、`writei`变为`ewrite`等等；
+ 关于buf层，由于FAT32的一个簇的大小较大，并且依不同的存储设备而变，因此我们目前以扇区为单位作缓存。

## 存在的问题

对FAT32的支持只是一个初步的实现，并且还未经过严格的测试，也存在许多不足。这里需要说明几点。

+ 目前只实现了文件读，文件写有初步实现但不完善，主要有新建文件无法生成时间戳、文件名校验和等问题；
+ 由于FAT32目录结构与xv6目录结构的不同，还未实现相关的接口为`ls`等程序提供支持；
+ FAT32的文件属性不支持设备类型，而原xv6文件系统中有`mknod`系统调用新建一个设备，并可以使用`open`打开。控制台输入输出就是这样打开的。为打开控制台标准输入输出，我们改用了另一种实现。

## 如何在qemu上挂载文件镜像

首先需要一个FAT32文件镜像，在Linux下可以通过`dd`命令和`mkfs`命令生成一个镜像，将生成的fs.img文件放在xv6-k210目录下即可。

```bash
$ dd if=/dev/zero of=fs.img bs=512k count=2048
$ mkfs.vfat -F 32 fs.img
```
注意：`dd`命令中的`bs`和`count`的参数只是参考，这会生成1GB的镜像。

## 如何在镜像中存入用户程序

首先通过`mount`命令，将镜像挂载到你指定的目录（这里以/mnt为例），然后将build之后xv6-user目录下文件名以"\_"为前缀的用户程序拷贝至镜像中，再取消挂载以保存写入结果即可。也可以在镜像中建立一些文本文件进行读测试。

注意：必须将xv6-user目录下的"\_init"拷贝至镜像根目录中，并且重命名为"init"（这是xv6原本的设定，目前我们暂未更改），还需拷贝"\_sh"至镜像中，才能启动shell。编写的用户程序也需要放在xv6-user目录下，同时需要添加至Makefile的`UPROGS`目标中，才能连同内核一起编译链接。

```bash
$ make build
$ (sudo)mount fs.img /mnt
$ (sudo)cp xv6-user/_init /mnt/init
$ (sudo)cp xv6-user/_sh /mnt
$ (sudo)cp xv6-user/... /mnt
$ ...
$ (sudo)umount /mnt
```

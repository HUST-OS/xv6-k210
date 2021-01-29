# XV6-RISCV On K210
[English](./README.md) [中文](./README_cn.md)   
在 `K210` 开发板上运行 `xv6-riscv` 操作系统  

```
 (`-')           (`-')                   <-.(`-')                            
 (OO )_.->      _(OO )                    __( OO)                            
 (_| \_)--.,--.(_/,-.\  ,--.    (`-')    '-'. ,--.  .----.   .--.   .----.   
 \  `.'  / \   \ / (_/ /  .'    ( OO).-> |  .'   / \_,-.  | /_  |  /  ..  \  
  \    .')  \   /   / .  / -.  (,------. |      /)    .' .'  |  | |  /  \  . 
  .'    \  _ \     /_)'  .-. \  `------' |  .   '   .'  /_   |  | '  \  /  ' 
 /  .'.  \ \-'\   /   \  `-' /           |  |\   \ |      |  |  |  \  `'  /  
`--'   '--'    `-'     `----'            `--' '--' `------'  `--'   `---''   
```

<!-- ![run-k210](./img/xv6-k210_on_k210.gif)   -->

## 依赖
+ `k210` 开发板或者 `qemu-system-riscv64`
+ RISC-V GCC 编译链: [riscv-gnu-toolchain](https://github.com/riscv/riscv-gnu-toolchain.git)

## 下载
```bash
git clone https://github.com/SKTT1Ryze/xv6-k210
```

## 编译
首先您需要连接 `k210` 开发板到电脑，然后检查 USB 端口：  
```bash
ls /dev/ | grep USB
```
在我的机器上的情况是将会显示 `ttyUSB0`，这就是 USB 端口。  
然后运行以下命令：    

```bash
cd xv6-k210
make build
```

## 在 k210 开发板上运行
运行以下命令：  
```bash
make run
```

某些情况下您需要修改 `USB 端口`，端口名称可以通过前面说的步骤得到，然后运行以下命令:  
```bash
make run k210-serialport=`USB 端口`(默认是 ttyUSB0)
```
Ps: 在 `Linux` 上这个端口大部分情况是 `ttyUSB0`, 如果您使用 `Windows` 或者 `MacOS`，这个文档可以帮助到您：[maixpy-doc](https://maixpy.sipeed.com/zh/get_started/env_install_driver.html#)  

## 在 qemu-system-riscv64 模拟器上运行
首先确保 `qemu-system-riscv64` 已经下载到您的机器上并且加到了环境变量中。  
然后运行以下命令：  
```bash
make run platform=qemu
```
Ps: 按 `Ctrl + A` 然后 `X` 退出 `qemu`。   
目前 `qemu` 平台已经初步支持文件系统并且能运行文件系统上的用户态程序，具体请移步[这里](./doc/fs.md)。  

## 快速在 `qemu` 平台上运行 `shell`
```bash
$ dd if=/dev/zero of=fs.img bs=512k count=2048
$ mkfs.vfat -F 32 fs.img
$ make build
$ (sudo)mount fs.img /mnt
$ (sudo)cp xv6-user/_init /mnt/init
$ (sudo)cp xv6-user/_sh /mnt
$ (sudo)cp xv6-user/_cat /mnt
$ (sudo)cp xv6-user/init.c /mnt
$ (sudo)umount /mnt
$ make run platform=qemu
```

进入 `qemu` 后输入命令 `_cat init.c`，将会读取 `fs.img` 中的 `init.c` 文件内容并输出到终端。  
这里的 `init.c` 文件可以是任意文本文件。此外，`shell`支持下列快捷键：  
- Ctrl-H -- 退格  
- Ctrl-U -- 删除行  
- Ctrl-D -- 文件尾（EOF）  
- Ctrl-P -- 打印进程列表

## 进度
- [x] 多核启动
- [x] 裸机 printf
- [x] 内存分配
- [x] 页表
- [x] 时钟中断
- [x] S 态外部中断
- [x] 接收 `UARTHS` 串口数据
- [x] SD card 驱动
- [x] 进程管理
- [x] 文件系统（qemu）
- [ ] 文件系统（k210）
- [x] 用户程序（qemu）
- [ ] 用户程序（k210）

## TODO
完善 k210 平台上的文件系统和用户态程序。  


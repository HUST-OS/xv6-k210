# XV6-RISCV On K210
Run xv6-riscv on k210 board  
[English](./README.md) [中文](./README_cn.md)   

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

![run-k210](./img/xv6-k210_run.gif)  

## Dependencies
+ `k210 board` or `qemu-system-riscv64`
+ RISC-V Toolchain: [riscv-gnu-toolchain](https://github.com/riscv/riscv-gnu-toolchain.git)

## Installation
```bash
git clone https://github.com/HUST-OS/xv6-k210
```

## Build
First you need to connect your k210 board to your PC.  
And check the `USB serial port`:  
```bash
ls /dev/ | grep USB
```
In my situation it will be `ttyUSB0`  

```bash
cd xv6-k210
make build
```

## Run on k210 board
Instead of the original file system, xv6-k210 runs with FAT32. You might need an SD card with FAT32 format.  
To start `shell`, you need to rename the "_init" and "_sh" in the "/xv6-user" to "init" and "sh" after building, 
then copy them to the root of your SD card.   
Or you can directly run the command as below with your SD card connected to your PC (SD card reader required).

Warning: this will format your SD card and clean your original data!
```bash
make sdcard sd="your SD card device's path"
```

To run on k210:
```bash
make run
```

Sometimes you should change the `USB serial port`:  
```bash
make run k210-serialport=`Your-USB-port`(default by ttyUSB0)
```
Ps: Most of the k210-port in Linux is ttyUSB0, if you use Windows or Mac OS, this doc 
may help you: [maixpy-doc](https://maixpy.sipeed.com/zh/get_started/env_install_driver.html#)  

## Run on qemu-system-riscv64
First, make sure `qemu-system-riscv64` is installed on your system.  
Second, make a disk image file with FAT32 file system.
```bash
make fs
```
It will generate a disk image file `fs.img`, and compile some user programs like `shell` then copy them into the `fs.img`.  
As long as the `fs.img` exists, you don't need to do this every time before running, unless you want to update it.

Finally, start running.
```bash
make run platform=qemu
```

Ps: Press Ctrl + A then X to quit qemu.

## About shell

The shell commands are user programs, too. Those program should be put in a "/bin" directory in your SD card or the `fs.img`.  
Now we support a few useful commands, such as `cd`, `ls`, `cat` and so on.

In addition, `shell` supports some shortcut keys as below:

- Ctrl-H -- backspace  
- Ctrl-U -- kill a line  
- Ctrl-D -- end of file (EOF)  
- Ctrl-P -- print process list  

## Progress
- [x] Multicore boot
- [x] Bare-metal printf
- [x] Memory alloc
- [x] Page Table
- [x] Timer interrupt
- [x] S mode extern interrupt
- [x] Receive uarths message
- [x] SD card driver
- [x] Process management
- [x] File system
- [x] User program
- [ ] Steady keyboard input(k210)

## TODO
Keyboard input on k210 platform.  


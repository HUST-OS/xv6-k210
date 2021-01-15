# XV6-RISCV On K210
Run xv6-riscv on k210 board  

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

![run-k210](./img/xv6-k210_on_k210.gif)  

## Dependencies
+ `k210 board` or `qemu-system-riscv64`
+ RISC-V Toolchain: [riscv-gnu-toolchain](https://github.com/riscv/riscv-gnu-toolchain.git)

## Installation
```bash
git clone https://github.com/SKTT1Ryze/xv6-k210
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
First make sure you have the `qemu-system-riscv64` on your system.  
Then run the command:  
```bash
make run platform=qemu
```
Ps: Press Ctrl + A then X to quit qemu. 
Besides, file system and uesr programs are available on qemu. More details [here](./doc/fs.md).

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
- [ ] File system
- [ ] User program

## TODO
The rest part of xv6-kernel and xv6-fs

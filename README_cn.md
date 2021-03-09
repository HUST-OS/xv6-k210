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
git clone https://github.com/HUST-OS/xv6-k210
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
Xv6-k210 采用 FAT32 文件系统，而不是其原本的文件系统。您需要一张 FAT32 格式的 SD 卡才能运行。
在编译项目后，您需要将 “/xv6-user” 目录下的 “_init” 和 “_sh” 重命名为 “init” 和 “sh”，并拷贝至 SD 卡的根目录下。
或者，您可以将 SD 卡连至主机（需要读卡器），再直接运行以下命令。

警告：这会格式化您的 SD 卡并清除卡上的原有数据！
```bash
make sdcard sd="your SD card device's path"
```

运行以下命令以在 `k210` 上运行：  
```bash
make run
```

某些情况下您需要修改 `USB 端口`，端口名称可以通过前面说的步骤得到，然后运行以下命令:  
```bash
make run k210-serialport=`USB 端口`(默认是 ttyUSB0)
```
Ps: 在 `Linux` 上这个端口大部分情况是 `ttyUSB0`, 如果您使用 `Windows` 或者 `MacOS`，这个文档可以帮助到您：[maixpy-doc](https://maixpy.sipeed.com/zh/get_started/env_install_driver.html#)  

## 在 qemu-system-riscv64 模拟器上运行
首先，确保 `qemu-system-riscv64` 已经下载到您的机器上并且加到了环境变量中；  
其次，需要一个 FAT32 磁盘镜像文件；
```bash
make fs
```
这会生成一个镜像文件 `fs.img` ，编译一些用户程序（如 `shell`）并拷贝至镜像中。只要 `fs.img` 存在并且不需要修改，您不必每次运行前都执行这个命令。

最后，开始运行：
```bash
make run platform=qemu
```

Ps: 按 `Ctrl + A` 然后 `X` 退出 `qemu`。     

## 关于 Shell
Shell 命令其实也是用户程序。这些程序应当放置在 SD 卡或 `fs.img` 文件镜像的 "/bin" 目录下。  
目前已经支持几个常用命令，如 `cd`，`ls`，`cat` 等。

此外，`shell` 支持下列快捷键：  
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
- [x] 文件系统
- [x] 用户程序
- [ ] 稳定的键盘输入（k210）

## TODO
完善 k210 平台上的键盘输入。  


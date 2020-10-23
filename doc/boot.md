# K210 多核启动移植记录
2020/10/18，将 xv6 移植到 k210 的进展：成功启动了 k210 的双核。  

## xv6 在 qemu 上的启动过程
xv6 在 qemu 里面是没有使用 SBI 实现来启动的，这种情况下它的内核被放在地址 0x80000000 处，qemu 将内核加载后直接执行内核的第一行指令。  
这个第一行指令是在 kernel 目录下的汇编文件 entry.S 里面：  
```
	# qemu -kernel loads the kernel at 0x80000000
        # and causes each CPU to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
_entry:
	# set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)
        la sp, stack0
        li a0, 1024*4
	csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
	# jump to start() in start.c
        call start
spin:
        j spin

```
在这个汇编文件里面内核会做一些准备工作，这里是会分配内核栈空间，每个核分配 4096 byte 的空间，然后跳转到 start.c 文件里面的 start() 函数中运行。  
在 start（） 函数中，xv6 内核还是运行在 M 态，做一些 M 态级别的准备工作，比如设置中断代理，M 态中断使能和入口，还有时钟中断的初始化。  
这些准备工作完成之后，xv6 内核就切换到了 S 态去运行，并跳转到 main.c 
## SBI 实现提供的 Bootloader 功能

## 在 k210 板子上用 SBI 实现启动双核


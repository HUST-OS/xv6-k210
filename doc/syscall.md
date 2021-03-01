+ [System Call的执行](#Syscall在xv6-k210中的实现与执行)
+ [System Call的添加](#如何在xv6-k210中添加syscall)

<br>
<br>

# System Call
System Call即系统调用（以下简称syscall），用于在用户程序中执行一些特权模式下才能完成的操作（如I/O操作）。
对于处理器而言，syscall是一种同步发生的事件，在基于RISC-V的操作系统中由`ecall`指令（Environment Call）产生。
当系统调用事件结束后，处理器将返回到ecall指令的下一条指令继续执行。

原版xv6的文档将“system call”、“exception”和“(device) interrupt”统称为“trap”。
在RISC-V指令集手册的1.6节中，对“exception”、“interrupt”和“trap”的定义是这样的：

> We use the term <u>*exception*</u> to refer to an unusual condition occurring at run time associated with
an instruction in the current RISC-V hart. We use the term <u>*interrupt*</u> to refer to an external
asynchronous event that may cause a RISC-V hart to experience an unexpected transfer of control.
We use the term <u>*trap*</u> to refer to the transfer of control to a trap handler caused by either an
exception or an interrupt.

从这个角度来看，trap在RISC-V的语境下指的是控制权的转换，也就是特权级的转换。这可能与我们平常使用的术语有一定出入，需要注意。

## 在RISC-V中请求系统调用
在RISC-V的用户程序中，使用`ecall`指令请求syscall。操作系统提供不同功能的syscall，对应不同的请求调用号。
因此请求前，需要向`a7`寄存器中写入所请求的syscall的调用号。以下为一个请求8号调用的例子：

```
li  a7, 8
ecall
```

## 系统调用的处理过程
Syscall涉及到特权态的转换，需要经过trap过程。对于用户程序而言，一般是从用户模式（U模式）陷入到监管模式（S模式）。
我们知道，操作系统中有中断向量表，保存着处理各类中断的中断例程的入口，syscall的中断处理例程自然也在其中。
而对于不同类型的调用请求，syscall的处理例程也有相应的系统调用表，保存不同类型的系统调用功能函数的入口。
根据用户事先写入寄存器的系统调用号，syscall的处理例程调用相应的功能函数，完成系统调用，随后返回至原先的特权态。

<br>
<br>

# Syscall在xv6-k210中的实现与执行
Syscall是如何实现与执行的呢？以下从用户程序出发，分析在xv6-k210中系统调用的过程。

## 一、Syscall的封装与使用
使用C语言编写用户程序时，可以直接编写内联汇编的形式，执行`ecall`指令，但这不够美观便捷。
xv6-k210将系统调用的汇编指令段进行了封装，提供了一个C函数的接口，在用户程序中，可以直接像使用普通函数一样调用，
包括传递参数和接收返回值。

在xv6-uesr/uesr.h中，声明了各个Syscall封装后的函数接口。在用户程序中包含该头文件，即可使用其中的函数。这些函数又是怎么实现的呢？
xv6-k210使用了perl语言脚本（xv6-uesr/usys.pl），重定向输出生成一个汇编源文件，这些函数就是在其中实现的。
不需要perl语言的知识，也能大致理解该文件中代码的含义。文件中部分代码如下：

```perl
print "#include \"kernel/include/syscall.h\"\n";

sub entry {
    my $name = shift;
    print ".global $name\n";
    print "${name}:\n";
    print " li a7, SYS_${name}\n";
    print " ecall\n";
    print " ret\n";
}
    
entry("fork");
entry("exit");
entry("wait");
......
```

其中`sub`就是perl语言中的函数定义。这里不深究perl语言的语法含义，只需知道，在xv6-k210编译时，
通过将***print***函数的输出重定向至一个汇编源文件（具体可查看Makefile），该文件中将会生成若干类似下述的汇编片段：

```
#include "kernel/include/syscall.h"
.global fork
fork:
    li a7, SYS_fork
    ecall
    ret
.global exit
exit:
    li a7, SYS_exit
    ecall
    ret
......
```

这就实现了xv6-uesr/uesr.h中声明的函数。此处包含了kernel/include/syscall.h文件，因为该文件中宏定义了各个系统调用的调用号。

```C
// System call numbers
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
......
```

综上所述，我们知道xv6-k210为用户程序提供了`ecall`指令的封装，使得用户程序可以很方便地请求系统调用。

## 二、特权态的转换过程

执行`ecall`指令后，系统中发生了什么呢？其实不仅是系统调用，各类中断发生后，首先都会经过这一过程——特权态的转换。
在RISC-V中，各个特权级都有一组状态与控制寄存器（CSRs），当在U模式下中断发生时（除了时钟中断），RISC-V硬件系统会自动完成下列操作：

1. 如果发生的中断是设备中断，且`sstatus`寄存器的`SIE`位为0，则不继续下列的操作；
2. 关闭外部设备中断，即将`sstatus`寄存器的`SIE`位设为0；
3. 拷贝`pc`寄存器的值至`sepc`寄存器；
4. 保存当前的特权态信息至`sstatus`寄存器的`SPP`字段；
5. 设置`scause`寄存器，其值表示中断发生的原因；
6. 将特权态设置为S模式；
7. 将`stvec`寄存器的值存入`pc`寄存器（`stvec`寄存器存放S模式的中断处理函数的入口地址）。

但硬件并没有完成我们所需的所有操作，还需进一步通过软件方式实现。
根据xv6-k210的[页表与内存映射](https://github.com/SKTT1Ryze/xv6-k210/blob/main/doc/xxx.md)机制，我们知道，
在用户程序虚拟地址空间的最高地址中，映射了`TRAMPOLINE`和`TRAPFRAME`两个特殊的页面，这两个页面与特权态的转换相关。
`TRAMPOLINE`对应的物理页中，存放着一些代码（见kernel/trampoline.S），包括`uservec`和`userret`两个函数。
其中，`uservec`函数的入口地址存放在`stvec`寄存器中，也就是S模式中断处理函数的入口地址寄存器。
当执行`ecall`指令后，`pc`会被设置为`stvec`中的值，系统就会进入`uservec`函数。该函数主要有以下工作：

1. 在用户进程的`TRAPFRAME`中，保存用户进程的工作空间（即各个寄存器的内容）；
2. 加载`TRAPFRAME`中保存的内核所需寄存器内容，如内核栈栈顶、内核态页表等。

随后，该函数通过`jr`指令（说明不返回）跳转到`uesrtrap`函数（定义在kernel/trap.c中）。

## 三、Syscall的执行

在`uesrtrap`函数中进行一些后续操作后，可看到以下代码片段：

```C
......
struct proc *p = myproc();
p->trapframe->epc = r_sepc();
if (r_scause() == 8) {
    if (p->killed)
        exit(-1);
    p->trapframe->epc += 4;
    intr_on();
    syscall();
}
......
```

这里首先获取该用户进程的进程控制块，该结构中也有对其`TRAPFRAME`的引用。发生中断时，寄存器`pc`的值已经保存在`sepc`中，
这里再将其保存在进程的`TRAPFRAME`中。随后读取S模式CSRs中的`scause`寄存器，其值为中断产生的原因。
当该值为8时，对应的就是syscall。由于syscall返回时是回到`ecall`指令的下一条指令，因此这里将`epc`的值加4。
至此，已经明确中断原因不是外设中断，可以通过`intr_on`将外设中断打开。
接下来进入系统调用的处理函数`syscall`（在kernel/syscall.c中定义），函数体如下：

```C
void syscall(void)
{
    int num;
    struct proc *p = myproc();
    num = p->trapframe->a7;
    if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
```

该函数首先获取当前进程的进程控制块，其`TRAPFRAME`中保存的`a7`寄存器的值就是请求的系统调用号。
检测该值的合法性后，调用相应的函数，函数返回值直接写入`TRAPFRAME`的`a0`寄存器。`a0`寄存器是RISC-V中约定的返回值寄存器，
当返回用户态时，通过该寄存器就可以获得系统调用的返回结果，就如同普通的函数调用一样。

注意到上述代码中有一个`syscalls[]`数组，这就是所谓的系统调用表，实际上就是一个函数指针数组，指向各个系统调用的功能函数。
该数组和其中的内容也是在kernel/syscall.c中声明，而系统调用号在kernel/include/syscall.h中声明，部分内容如下：

```C
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
......

static uint64 (*syscalls[])(void) = {
    [SYS_fork]    sys_fork,
    [SYS_exit]    sys_exit,
    [SYS_wait]    sys_wait,
    ......
};
```

我们注意到，这里的函数都是无参数的，而有的系统调用是需要参数的，如何解决这一问题呢？
实际上，参数的获取可以由各功能函数自行完成，kernel/syscall.c中也提供了相关的辅助函数。
在需要参数的系统调用功能函数中，通过`myproc`函数获取进程的控制块，其`TRAPFRAME`中保存中断时各通用寄存器的值，
对通过寄存器传递的参数，可直接在其中找到；若参数是指针值，还需要其指向的实际内容，则先通过进程控制块中保存的页表起始地址，
找到进程的页表，再由页表将该指针值（虚拟地址）转换成物理地址，即可找到其实际指向的内容，完成系统调用的具体功能。

## 四、返回用户模式

系统功能调用完成后，从`syscall`函数返回到`uesrtrap`函数，该函数进行其他操作后，调用`usertrapret`（也定义在kernel/trap.c中）。
这一函数则为返回U模式做准备，包括设置S模式部分CSRs寄存器、在进程`TRAPFRAME`中保存进程在S模式下所需寄存器的值、重设`epc`寄存器等，
最后，需要调用kernel/trampoline.S中的另一个函数——`userret`，其需要两个参数，一个是`TRAPFRAME`在用户空间的虚拟地址（由宏定义），
另一个是用户进程的页表在内核中的地址（已经格式化为正确形式）。

在跳转到`userret`函数后，首先将用户页表还原至页表寄存器`satp`，并刷新TLB。之后就可以使用虚拟地址，
将`TRAPFRAME`中保存的各个寄存器还原至对应寄存器，最后使用`sret`指令，返回到用户模式，一个Syscall到这里就完成了。


<br>
<br>

# 如何在xv6-k210中添加syscall

了解了Syscall在xv6-k210中的执行过程，为系统添加一个新的syscall功能就不是什么难事了。添加新系统调用的步骤如下：

1. 在xv6-uesr目录下
    + 在uesr.h文件中，添加新系统调用封装后的函数声明，假设其函数名为`mysyscall`。
    + 在usys.pl文件末尾，添加如下行：

        ```perl
        ......
        entry("mysyscall");
        ```

2. 在kernel目录下
    + 在include/syscall.h文件中，添加新系统调用号的宏定义：

        ```C
        ......
        #define SYS_mysyscall ?
        ```

        其中，“?”为新的合法系统调用号，本质上是一个数组下标，可根据需要设置，建议按顺序递增添加。

    + 在syscall.c文件中，添加功能函数的声明，并更新系统调用表：

        ```C
        ......
        extern uint64 sys_mysyscall(void);

        static uint64 (*syscalls[])(void) = {
            ......
            [SYS_mysyscall]    sys_mysyscall,
        };

        ```
3. 根据该系统调用的功能，选择一个适合的内核模块的源文件，在其中实现`sys_mysyscall`函数的功能。
    例如，与文件相关的系统调用可以在kernel/sysfile.c中添加。
    可以利用syscall.c文件中提供的相关函数，在`sys_mysyscall`中获取用户进程传递的参数。

下面给出一个更具体的例子。在我们的移植工作中，我们需要测试用户进程的运行，使其向屏幕进行输出。
但当时用户态尚未支持`printf`函数，我们便通过系统调用，在S态进行一些输出。为此，我们需要添加一个`test_proc`函数。

首先，我们在xv6-uesr/uesr.h文件中添加函数声明，为用户程序添加调用接口：
```C
int test_proc(void);
```
在xv6-uesr/usys.pl中添加行:
```perl
entry("test_proc");
```
随后修改内核，在kernel/include/syscall.h中添加调用号，调用号可以自行选择，这里我们选择22，表示第22个系统调用。
```C
#define SYS_test_proc 22
```
最后，在kernel/include/syscall.c中修改系统调用表，以及添加函数`sys_test_proc`：
```C
extern uint64 sys_test_proc(void);

static uint64 (*syscalls[])(void) = {
    ......
    [SYS_test_proc]    sys_test_proc,
};

uint64 sys_test_proc(void) {
    printf("hello world from proc %d, hart %d\n", myproc()->pid, r_tp());
    return 0;
}
```
至此，我们就已经成功添加了一个系统调用。在用户程序中可以使用这个系统调用，例如以下的用户程序实例：
```C
#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

int main()
{
    int n = 10;
    while (n--) {
        test_proc();
    }
    exit(0);
}
```
编译运行后的结果如下图所示（编译用户程序的方法详见[此处](./fs.md)）：

![](/img/syscall_test.png)


<br>
<br>

# 参考文档

+ [*xv6: a simple, Unix-like teaching operating system*](https://pdos.csail.mit.edu/6.S081/2020/xv6/book-riscv-rev1.pdf)
+ [*The RISC-V Instruction Set Manual Volume I: Unprivileged ISA*](https://github.com/riscv/riscv-isa-manual/releases/download/draft-20210212-c879d5a/riscv-spec.pdf)
+ [《RISC-V手册——一本开源指令集的指南》](http://riscvbook.com/chinese/RISC-V-Reader-Chinese-v2p1.pdf)


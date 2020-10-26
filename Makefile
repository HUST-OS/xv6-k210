# Compile xv6 for porting on k210

K=kernel
U=user
T=target

OBJS = \
  $K/entry_k210.o \
  $K/console.o \
  $K/printf.o \
  $K/uart.o \
  $K/kalloc.o \
  $K/spinlock.o \
  $K/string.o \
  $K/main.o \
  $K/vm.o \
  $K/proc.o \
  $K/swtch.o \
  $K/trampoline.o \
  $K/trap.o \
  $K/syscall.o \
  $K/sysproc.o \
  $K/bio.o \
  $K/fs.o \
  $K/log.o \
  $K/sleeplock.o \
  $K/file.o \
  $K/pipe.o \
  $K/exec.o \
  $K/sysfile.o \
  $K/kernelvec.o \
  $K/plic.o \
  $K/virtio_disk.o \
  $K/timer.o \
  $K/test.o \


QEMU = qemu-system-riscv64
RUSTSBI = ./bootloader/SBI/rustsbi-k210

TOOLPREFIX	:= riscv64-unknown-elf-
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

LDFLAGS = -z max-page-size=4096

linker = ./linker/k210.ld

# LDFLAGS = -Wl,--build-id=none -nostartfiles -nostdlib -static -fno-stack-protector
# LIBS				:= -lgcc
# LINK				:= $(CC) $(LDFLAGS)

$K/kernel: $(OBJS) $(linker)
	@$(LD) $(LDFLAGS) -T $(linker) -o $T/kernel $(OBJS)
	@$(OBJDUMP) -S $T/kernel > $T/kernel.asm
	@$(OBJDUMP) -t $T/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $T/kernel.sym
	# @rm -f $K/*.o $K/*.d
  
build: $K/kernel

ifndef CPUS
CPUS := 2
endif

QEMUOPTS = -machine virt -bios $(RUSTSBI) 
QEMUOPTS += -kernel $T/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -device loader,file=$T/kernel,addr=0x80200000

qemu: build
	@$(QEMU) $(QEMUOPTS)

image = $T/kernel.bin
k210 = $T/k210.bin
k210-serialport := /dev/ttyUSB1

k210: build
	@riscv64-unknown-elf-objcopy $T/kernel --strip-all -O binary $(image)
	@riscv64-unknown-elf-objcopy $(RUSTSBI) --strip-all -O binary $(k210)
	# @cp $(RUSTSBI) $(k210)
	@dd if=$(image) of=$(k210) bs=128k seek=1


run-k210: k210
	@sudo chmod 777 $(k210-serialport)
	python3 ./tools/kflash.py -p $(k210-serialport) -b 1500000 -t $(k210)

clean:
	rm -f $K/*.o $K/*.d
	rm -rf $T/*
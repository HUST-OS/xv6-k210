platform	:= k210
# platform	:= qemu
# mode := debug
mode := release
K=kernel
U=xv6-user
T=target

OBJS =
ifeq ($(platform), k210)
OBJS += $K/entry_k210.o
else
OBJS += $K/entry_qemu.o
endif

OBJS += \
  $K/printf.o \
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
  $K/sleeplock.o \
  $K/file.o \
  $K/pipe.o \
  $K/exec.o \
  $K/sysfile.o \
  $K/kernelvec.o \
  $K/timer.o \
  $K/logo.o \
  $K/disk.o \
  $K/fat32.o \
  $K/console.o

ifeq ($(platform), k210)
OBJS += \
  $K/spi.o \
  $K/gpiohs.o \
  $K/fpioa.o \
  $K/utils.o \
  $K/sdcard.o \

else
OBJS += \
  $K/virtio_disk.o \
  $K/plic.o \
  $K/uart.o \

endif

QEMU = qemu-system-riscv64

ifeq ($(platform), k210)
RUSTSBI = ./bootloader/SBI/sbi-k210
else
RUSTSBI = ./bootloader/SBI/sbi-qemu
endif

# TOOLPREFIX	:= riscv64-unknown-elf-
TOOLPREFIX	:= riscv64-linux-gnu-
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -g
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

ifeq ($(mode), debug) 
CFLAGS += -DDEBUG 
endif 

ifeq ($(platform), qemu)
CFLAGS += -D QEMU
endif

LDFLAGS = -z max-page-size=4096

ifeq ($(platform), k210)
linker = ./linker/k210.ld
endif

ifeq ($(platform), qemu)
linker = ./linker/qemu.ld
endif

# Compile Kernel
$T/kernel: $(OBJS) $(linker) $U/initcode
	if [ ! -d "./target" ]; then mkdir target; fi
	@$(LD) $(LDFLAGS) -T $(linker) -o $T/kernel $(OBJS)
	@$(OBJDUMP) -S $T/kernel > $T/kernel.asm
	@$(OBJDUMP) -t $T/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $T/kernel.sym
  
build: $T/kernel userprogs

# Compile RustSBI
RUSTSBI:
	@cd ./bootloader/SBI/rustsbi-k210 && cargo build && cp ./target/riscv64gc-unknown-none-elf/debug/rustsbi-k210 ../sbi-k210
	@$(OBJDUMP) -S ./bootloader/SBI/sbi-k210 > $T/rustsbi-k210.asm
	@cd ./bootloader/SBI/rustsbi-qemu && cargo build && cp ./target/riscv64gc-unknown-none-elf/debug/rustsbi-qemu ../sbi-qemu
	@$(OBJDUMP) -S ./bootloader/SBI/sbi-qemu > $T/rustsbi-qemu.asm
	
rustsbi-clean:
	@cd ./bootloader/SBI/rustsbi-k210 && cargo clean
	@cd ./bootloader/SBI/rustsbi-qemu && cargo clean

image = $T/kernel.bin
k210 = $T/k210.bin
k210-serialport := /dev/ttyUSB0

ifndef CPUS
CPUS := 2
endif

QEMUOPTS = -machine virt -bios $(RUSTSBI) -kernel $T/kernel -m 128M -smp $(CPUS) -nographic

# import virtual disk image
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0 
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

run: build fs.img
ifeq ($(platform), k210)
	@$(OBJCOPY) $T/kernel --strip-all -O binary $(image)
	@$(OBJCOPY) $(RUSTSBI) --strip-all -O binary $(k210)
	@dd if=$(image) of=$(k210) bs=128k seek=1
	@$(OBJDUMP) -D -b binary -m riscv $(k210) > $T/k210.asm
	@sudo chmod 777 $(k210-serialport)
	@python3 ./tools/kflash.py -p $(k210-serialport) -b 1500000 -t $(k210)
else
	@$(QEMU) $(QEMUOPTS)
endif

fs.img:	$(UPROGS)
	@./fs.sh

$U/initcode: $U/initcode.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/initcode.S -o $U/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/initcode.out $U/initcode.o
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode
	$(OBJDUMP) -S $U/initcode.o > $U/initcode.asm

tags: $(OBJS) _init
	@etags *.S *.c

ULIB = $U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$U/usys.S : $U/usys.pl
	@perl $U/usys.pl > $U/usys.S

$U/usys.o : $U/usys.S
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S

$U/_forktest: $U/forktest.o $(ULIB)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/_forktest $U/forktest.o $U/ulib.o $U/usys.o
	$(OBJDUMP) -S $U/_forktest > $U/forktest.asm

mkfs/mkfs: mkfs/mkfs.c $K/include/fs.h $K/include/param.h
	@gcc -Werror -Wall -I. -o mkfs/mkfs mkfs/mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

UPROGS=\
	$U/_init\
	$U/_sh\
	$U/_cat\
	$U/_echo\
	$U/_grep\
	$U/_ls\
	$U/_kill\
	$U/_mkdir\
	$U/_xargs\
	$U/_sleep\
	$U/_find\
	$U/_rm

	# $U/_forktest\
	# $U/_ln\
	# $U/_test\
	# $U/_stressfs\
	# $U/_usertests\
	# $U/_grind\
	# $U/_wc\
	# $U/_zombie\

UEXTRA = $U/xargstest.sh

userprogs: $(UEXTRA) $(UPROGS)

# Make fs image
fs: mkfs/mkfs README $(UEXTRA) $(UPROGS)
	@mkfs/mkfs fs.img README $(UEXTRA) $(UPROGS)

-include kernel/*.d user/*.d

SDCARD		?= /dev/sdb

# Write sdcard
sdcard: fs
	@echo "flashing into sd card..."
	@sudo dd if=/dev/zero of=$(SDCARD) bs=1M count=50
	@sudo dd if=fs.img of=$(SDCARD)

clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*/*.o */*.d */*.asm */*.sym \
	$T/* \
	$U/initcode $U/initcode.out \
	$K/kernel \
	mkfs/mkfs .gdbinit \
        $U/usys.S \
	$(UPROGS)

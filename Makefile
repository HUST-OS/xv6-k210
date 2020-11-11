# Compile xv6 for porting on k210

K=kernel
U=xv6-user
T=target
S=kendryte_sdk

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
  $K/spi.o \
  $K/gpiohs.o \
  $K/fpioa.o \
  $K/utils.o \
  $K/sdcard.o \
  $K/test.o \

SDK_OBJS = \
  $S/clint.o \
  $S/dmac.o \
  $S/fpioa.o \
  $S/gpio.o \
  $S/gpiohs.o \
  $S/pwm.o \
  $S/sdcard.o \
  $S/sha256.o \
  $S/spi.o \
  $S/sysctl.o \

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

kendryte_sdk_lib = ./libkendryte.a

$T/kernel: $(OBJS) $(linker) $U/initcode $U/testcode_1 $U/testcode_2 $U/testcode_3 $U/testcode_4
	@$(LD) $(LDFLAGS) -T $(linker) -o $T/kernel $(OBJS)
	@$(OBJDUMP) -S $T/kernel > $T/kernel.asm
	@$(OBJDUMP) -t $T/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $T/kernel.sym
	# @rm -f $K/*.o $K/*.d
  
build: $T/kernel

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
k210-serialport := /dev/ttyUSB0

k210: build
	@riscv64-unknown-elf-objcopy $T/kernel --strip-all -O binary $(image)
	@riscv64-unknown-elf-objcopy $(RUSTSBI) --strip-all -O binary $(k210)
	# @cp $(RUSTSBI) $(k210)
	@dd if=$(image) of=$(k210) bs=128k seek=1
	# @dd if=sdcard.bin of=$(k210) bs=128k seek=3
	@$(OBJDUMP) -D -b binary -m riscv $(k210) > $T/k210.asm

run-k210: k210
	@sudo chmod 777 $(k210-serialport)
	python3 ./tools/kflash.py -p $(k210-serialport) -b 1500000 -t $(k210)


$U/initcode: $U/initcode.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/initcode.S -o $U/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/initcode.out $U/initcode.o
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode
	$(OBJDUMP) -S $U/initcode.o > $U/initcode.asm

$U/testcode_1: $U/testcode_1.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/testcode_1.S -o $U/testcode_1.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/testcode_1.out $U/testcode_1.o
	$(OBJCOPY) -S -O binary $U/testcode_1.out $U/testcode_1
	$(OBJDUMP) -S $U/testcode_1.o > $U/testcode_1.asm

$U/testcode_2: $U/testcode_2.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/testcode_2.S -o $U/testcode_2.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/testcode_2.out $U/testcode_2.o
	$(OBJCOPY) -S -O binary $U/testcode_2.out $U/testcode_2
	$(OBJDUMP) -S $U/testcode_2.o > $U/testcode_2.asm

$U/testcode_3: $U/testcode_3.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/testcode_3.S -o $U/testcode_3.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/testcode_3.out $U/testcode_3.o
	$(OBJCOPY) -S -O binary $U/testcode_3.out $U/testcode_3
	$(OBJDUMP) -S $U/testcode_3.o > $U/testcode_3.asm

$U/testcode_4: $U/testcode_4.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/testcode_4.S -o $U/testcode_4.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/testcode_4.out $U/testcode_4.o
	$(OBJCOPY) -S -O binary $U/testcode_4.out $U/testcode_4
	$(OBJDUMP) -S $U/testcode_4.o > $U/testcode_4.asm

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
	$U/_cat\
	$U/_echo\
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_rm\
	$U/_sh\
	$U/_stressfs\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\

UEXTRA = $U/xargstest.sh

fs.img: mkfs/mkfs README $(UEXTRA) $(UPROGS)
	@mkfs/mkfs fs.img README $(UEXTRA) $(UPROGS)

-include kernel/*.d user/*.d

SDCARD		?= /dev/sdb

sdcard: fs.img
	@echo "flashing into sd card..."
	@sudo dd if=/dev/zero of=$(SDCARD) bs=1M count=50
	@sudo dd if=fs.img of=$(SDCARD)



clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*/*.o */*.d */*.asm */*.sym \
	$T/* \
	$U/initcode $U/initcode.out $K/kernel fs.img \
	mkfs/mkfs .gdbinit \
        $U/usys.S \
	$(UPROGS)

C_SOURCES = $(wildcard kernel/*.c drivers/*.c cpu/*.c libc/*.c)
HEADERS = $(wildcard kernel/*.h drivers/*.h cpu/*.h libc/*.h)
OBJ = ${C_SOURCES:.c=.o} cpu/interrupt.o

CC = /usr/local/i386elfgcc/bin/i386-elf-gcc
LD = /usr/local/i386elfgcc/bin/i386-elf-ld
CFLAGS = -ffreestanding

os-image.bin: boot/bootsect.bin kernel.bin
	cat $^ > os-image.bin
	truncate -s 1474560 os-image.bin

kernel.bin: boot/kernel_entry.o ${OBJ}
	$(LD) -o $@ -Ttext 0x10000 $^ --oformat binary

disk.img:
	truncate -s 10M disk.img

run: os-image.bin disk.img
	qemu-system-i386 -drive file=os-image.bin,format=raw,if=floppy -drive file=disk.img,format=raw,if=ide -boot a

%.o: %.c ${HEADERS}
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	nasm $< -f elf -o $@

%.bin: %.asm
	nasm $< -f bin -o $@

clean:
	rm -rf *.bin *.dis *.o os-image.bin *.elf
	rm -rf kernel/*.o boot/*.bin drivers/*.o boot/*.o cpu/*.o libc/*.o
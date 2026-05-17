# Define the target ISO name
TARGET = os.iso
ISO_DIR = isodir
GRUB_CFG = $(ISO_DIR)/boot/grub/grub.cfg

# Define compiler and linker flags
AS = as --32
CC = gcc -m32
LD = ld -m elf_i386
CFLAGS = -ffreestanding -nostdlib -nostartfiles -nodefaultlibs
LDFLAGS = -T linker.ld

# Targets
all: $(TARGET)

# Step 1: Assemble boot.o
multiboot.o: multiboot.s
	$(AS) -o $@ $<

# Step 2: Compile kernel.o
kernel.o: kernel.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Step 3: Link kernel.bin
kernel.bin: multiboot.o kernel.o
	$(LD) $(LDFLAGS) -o $@ multiboot.o kernel.o

# Step 4: Set up ISO directory structure
$(ISO_DIR)/boot/kernel.bin: kernel.bin
	mkdir -p $(ISO_DIR)/boot/grub
	cp kernel.bin $(ISO_DIR)/boot/kernel.bin

# Step 5: Create grub.cfg (FIXED FOR IMMEDIATE BOOT)
$(GRUB_CFG):
	@printf 'set timeout=0\nset default=0\nset timeout_style=hidden\nmenuentry "myos" {\n    multiboot /boot/kernel.bin\n}\n' > $(GRUB_CFG)

# Step 6: Build the ISO image
$(TARGET): $(ISO_DIR)/boot/kernel.bin $(GRUB_CFG)
	grub-mkrescue -o $(TARGET) $(ISO_DIR)

# Clean up build files
clean:
	rm -f *.o kernel.bin $(TARGET)
	rm -rf $(ISO_DIR)

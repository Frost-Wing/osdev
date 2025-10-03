IMG_SIZE = 64M
IMG_FILE = FrostWing.img
MOUNT_DIR = /mnt/fat32

SUDO_MESSAGE := " \033[1;34m========================================\033[0m\n\
\033[1;37mRequesting sudo privileges to configure drives.\033[0m\n\
\033[1;37mNo changes outside of setup will be made.\033[0m\n\
\033[1;34m========================================\033[0m"

# -----------------------------
# Build FAT32 bootable image
# -----------------------------
img:
	@echo "[*] Cleaning old mount dir and image..."
	@echo -e $(SUDO_MESSAGE)
	@sudo umount $(MOUNT_DIR) 2>/dev/null || true
	@sudo rm -rf $(MOUNT_DIR)
	@sudo rm -f $(IMG_FILE)

	@echo "[*] Creating empty image..."
	@dd if=/dev/zero of=$(IMG_FILE) bs=1M count=64 status=progress

	@echo "[*] Creating MBR partition..."
	@sudo parted $(IMG_FILE) --script mklabel msdos
	@sudo parted $(IMG_FILE) --script mkpart primary fat32 1MiB 100%

	@echo "[*] Mapping loop device..."
	@LOOP_DEV=$$(sudo losetup -Pf --show $(IMG_FILE)); \
	echo "[*] Loop device: $$LOOP_DEV"; \
	PART_DEV=$${LOOP_DEV}p1; \
	sudo mkfs.fat -F32 $$PART_DEV; \
	sudo mkdir -p $(MOUNT_DIR); \
	sudo mount $$PART_DEV $(MOUNT_DIR); \
	\
	echo "[*] Copying files..."; \
	sudo cp -v source/wing_kernel.elf source/boot/limine.cfg source/boot/libFrostedWM.so source/boot/Vera.sfn \
		limine/limine-bios.sys limine/limine-bios-pxe.bin limine/limine-bios-cd.bin limine/limine-uefi-cd.bin $(MOUNT_DIR)/; \
	sudo mkdir -p $(MOUNT_DIR)/EFI/BOOT; \
	sudo cp -v limine/BOOTX64.EFI limine/BOOTAA64.EFI limine/BOOTIA32.EFI limine/BOOTRISCV64.EFI $(MOUNT_DIR)/EFI/BOOT/; \
	\
	echo "[*] Installing Limine BIOS bootloader..."; \
	sudo ./limine/limine bios-install $(IMG_FILE); \
	\
	sudo umount $(MOUNT_DIR); \
	sudo losetup -d $$LOOP_DEV; \
	sudo rm -rf $(MOUNT_DIR)


# -----------------------------
# Tarball for distribution
# -----------------------------
tarball: img
	@tar -czvf $(IMG_FILE).tar.gz $(IMG_FILE)

# -----------------------------
# QEMU targets
# -----------------------------
QEMU_COMMON = \
	-vga std \
	-debugcon stdio \
	-serial file:serial.log \
	-audiodev pa,id=speaker \
	-device rtl8139,netdev=eth0 \
	-netdev user,hostfwd=tcp::5555-:22,id=eth0 \
	-hda FrostWing.iso \
	-drive if=none,file=disk.txt,id=disk \
	-device ahci,id=ahci \
	-device ide-hd,drive=disk,bus=ahci.0 \
	-m 512

run-x86-hdd:
	@qemu-system-x86_64 $(QEMU_COMMON)

run-x86-uefi:
	@qemu-system-x86_64 \
	-bios ./firmware/uefi/tianocore-64.uefi \
	$(QEMU_COMMON)

# -----------------------------
# Everything targets
# -----------------------------
everything:
	@make clean all -C source && make img tarball run-x86-hdd

everything-sign:
	@make clean all -C source && make sign-kernel && make img tarball run-x86-hdd

# -----------------------------
# Kernel signing
# -----------------------------
sign-kernel:
	@openssl dgst -sha256 -sign ./keys/private_key.pem -out ./keys/file.sig ./source/wing_kernel.elf
	@openssl dgst -sha256 -verify ./keys/public_key.pem -signature ./keys/file.sig ./source/wing_kernel.elf

# -----------------------------
# Fonts
# -----------------------------
fonts:
	sfnconv -U -B 40 -t b1 ~/Downloads/FiraSans-Regular.ttf ~/Desktop/FrostWing/source/boot/Vera.sfn

# -----------------------------
# Cleanup
# -----------------------------
top-clean:
	@rm -rf ./disk_root $(IMG_FILE) $(IMG_FILE).tar.gz serial.log

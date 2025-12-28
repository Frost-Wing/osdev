ISO_FILE = FrostWing.iso
ISO_ROOT = disk_root

SUDO_MESSAGE := " \033[1;34m========================================\033[0m\n\
\033[1;37mRequesting sudo privileges to configure drives.\033[0m\n\
\033[1;37mNo changes outside of setup will be made.\033[0m\n\
\033[1;34m========================================\033[0m"

# -----------------------------
# Build an bootable image (BIOS + UEFI)
# -----------------------------
iso:
	@echo "[*] Building bootable ISO..."
	@rm -rf $(ISO_ROOT) $(ISO_FILE)
	@mkdir -p $(ISO_ROOT)/EFI/BOOT

	@cp -v \
		source/wing_kernel.elf \
		source/boot/limine.cfg \
		source/boot/libFrostedWM.so \
		source/boot/font.sfn \
		limine/limine-bios.sys \
		limine/limine-bios-pxe.bin \
		limine/limine-bios-cd.bin \
		limine/limine-uefi-cd.bin \
		$(ISO_ROOT)/

	@cp -v \
		limine/BOOTX64.EFI \
		limine/BOOTAA64.EFI \
		limine/BOOTIA32.EFI \
		limine/BOOTRISCV64.EFI \
		$(ISO_ROOT)/EFI/BOOT/

	@xorriso -as mkisofs \
		-b limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label \
		$(ISO_ROOT) \
		-o $(ISO_FILE)

	@./limine/limine bios-install $(ISO_FILE)
	@echo "[+] ISO build complete: $(ISO_FILE)"


# -----------------------------
# Tarball for distribution
# -----------------------------
tarball:
	@tar -czvf $(ISO_FILE).tar.gz $(ISO_FILE)

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
	-cdrom $(ISO_FILE) \
	-drive if=none,file=disk.txtimg,id=disk \
	-device ahci,id=ahci \
	-device ide-hd,drive=disk,bus=ahci.0 \
	-rtc base=localtime,clock=host \
	-m 512

run-x86-hdd:
	@qemu-system-x86_64 $(QEMU_COMMON)

run-x86-uefi:
	@qemu-system-x86_64 \
	-bios ./firmware/uefi/tianocore-64.uefi \
	$(QEMU_COMMON)

run-x86-vnc:
	@qemu-system-x86_64 \
	-vnc :0 \
	$(QEMU_COMMON)

# -----------------------------
# Everything targets
# -----------------------------
everything:
	@make clean all -C source && make iso tarball run-x86-hdd

everything-sign:
	@make clean all -C source && make sign-kernel && make iso tarball run-x86-hdd

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
	./sfnconv -U -B 32 ./fira.ttf ./source/boot/font.sfn

# -----------------------------
# Cleanup
# -----------------------------
clean:
	@rm -rf ./disk_root $(ISO_FILE) $(ISO_FILE).tar.gz serial.log
	@cd source && make deep-clean && cd ..

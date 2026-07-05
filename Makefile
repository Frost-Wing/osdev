SHELL := /bin/bash

ISO_FILE = FrostWing.iso
ISO_ROOT = disk_root

# ================================================================
#  FrostWing colour palette
# ================================================================
BLUE    := \033[1;34m
CYAN    := \033[1;36m
WHITE   := \033[1;37m
GRAY    := \033[0;90m
BOLD    := \033[1m
RESET   := \033[0m

# Status prefixes
INFO := $(BLUE)❄$(RESET)
OK   := $(CYAN)✓$(RESET)
WARN := $(WHITE)!$(RESET)

define FROSTWING_BANNER
echo -e "$(BLUE) _____              _ __        ___             $(RESET)"; \
echo -e "$(BLUE)|  ___| __ ___  ___| |\\ \\      / (_)_ __   __ _ $(RESET)"; \
echo -e "$(CYAN)| |_ | '__/ _ \\/ __| __\\ \\ /\\ / /| | '_ \\ / _\` |$(RESET)"; \
echo -e "$(WHITE)|  _|| | | (_) \\__ \\ |_ \\ V  V / | | | | | (_| |$(RESET)"; \
echo -e "$(GRAY)|_|  |_|  \\___/|___/\\__| \\_/\\_/  |_|_| |_|\\__, |$(RESET)"; \
echo -e "$(GRAY)                                          |___/ $(RESET)"
endef

define SECTION
echo -e "$(BLUE)────────────────────────────────────────────$(RESET)"
endef

SUDO_MESSAGE := " \033[1;34m========================================\033[0m\n\
\033[1;37m❄  Requesting sudo privileges to configure drives.\033[0m\n\
\033[1;37m   No changes outside of setup will be made.\033[0m\n\
\033[1;34m========================================\033[0m"

# -----------------------------
# Build an bootable image (BIOS + UEFI)
# -----------------------------
iso:
	@$(FROSTWING_BANNER)
	@$(SECTION)
	@echo -e "$(INFO) $(WHITE)Building bootable ISO...$(RESET)"
	@rm -rf $(ISO_ROOT) $(ISO_FILE)
	@mkdir -p $(ISO_ROOT)/EFI/BOOT

	@echo -e "$(INFO) $(GRAY)Staging BIOS/kernel payload...$(RESET)"
	@cp -v \
		source/wing_kernel.elf \
		source/boot/limine.cfg \
		limine/limine-bios.sys \
		limine/limine-bios-pxe.bin \
		limine/limine-bios-cd.bin \
		limine/limine-uefi-cd.bin \
		$(ISO_ROOT)/

	@echo -e "$(INFO) $(GRAY)Staging UEFI boot loaders...$(RESET)"
	@cp -v \
		limine/BOOTX64.EFI \
		limine/BOOTAA64.EFI \
		limine/BOOTIA32.EFI \
		limine/BOOTRISCV64.EFI \
		$(ISO_ROOT)/EFI/BOOT/

	@echo -e "$(INFO) $(WHITE)Assembling hybrid BIOS/UEFI image...$(RESET)"
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
	@$(SECTION)
	@echo -e "$(OK) $(CYAN)ISO build complete:$(RESET) $(BOLD)$(ISO_FILE)$(RESET)"


# -----------------------------
# Tarball for distribution
# -----------------------------
tarball:
	@echo -e "$(INFO) $(WHITE)Packing $(ISO_FILE) into a distributable tarball...$(RESET)"
	@tar -czvf $(ISO_FILE).tar.gz $(ISO_FILE)
	@echo -e "$(OK) $(CYAN)Tarball ready:$(RESET) $(BOLD)$(ISO_FILE).tar.gz$(RESET)"

# -----------------------------
# QEMU targets
# -----------------------------
ifdef CI
AUDIO = -audiodev none,id=speaker
else
AUDIO = -audiodev pa,id=speaker
endif

ifndef CI
KVM = -enable-kvm
endif

# Note:
# - Serial is outputed to 'serial.log' file.
# - E9 Debug port messages would be displayed inside the terminal
# - NVMe disk is used for disk.img (to emulate disks)
# - Main OS is on an CDROM
#
# To use disk.img as an AHCI device:
#   -drive if=none,format=raw,file=disk.img,id=disk
#   -device ide-hd,drive=disk,bus=ahci.0
# To use disk.img as an NVMe device:
#   -drive if=none,format=raw,file=disk.img,id=nvmedisk
#   -device nvme,drive=nvmedisk,serial=FROSTNVME0

QEMU_COMMON = \
    -vga std \
    -debugcon stdio \
    -serial file:serial.log \
    $(AUDIO) \
    -device rtl8139,netdev=eth0 \
    -netdev user,hostfwd=tcp::5555-:22,id=eth0 \
    -device ahci,id=ahci \
	-drive if=none,format=raw,file=disk.img,id=disk \
    -device ide-hd,drive=disk,bus=ahci.1 \
    -drive if=none,media=cdrom,format=raw,file=$(ISO_FILE),id=cd0 \
    -device ide-cd,drive=cd0,bus=ahci.0 \
    -rtc base=localtime,clock=host \
    -boot order=d \
    $(KVM) \
	-object filter-dump,id=f1,netdev=eth0,file=/tmp/dump.pcap \
    -m 512

run-x86-bios:
	@echo -e "$(INFO) $(WHITE)Launching FrostWing (BIOS) in QEMU...$(RESET)"
	@qemu-system-x86_64 $(QEMU_COMMON)

run-x86-uefi:
	@echo -e "$(INFO) $(WHITE)Launching FrostWing (UEFI) in QEMU...$(RESET)"
	@qemu-system-x86_64 \
	-bios ./firmware/uefi/tianocore-64.uefi \
	$(QEMU_COMMON)

run-x86-vnc:
	@echo -e "$(INFO) $(WHITE)Launching FrostWing (VNC :0) in QEMU...$(RESET)"
	@qemu-system-x86_64 \
	-vnc :0 \
	-no-reboot -no-shutdown \
	$(QEMU_COMMON)

# -----------------------------
# Everything targets
# -----------------------------
everything:
	@$(FROSTWING_BANNER)
	@echo -e "$(INFO) $(WHITE)Full clean build + run...$(RESET)"
	@make clean all -C source && make iso tarball run-x86-bios

everything-sign:
	@$(FROSTWING_BANNER)
	@echo -e "$(INFO) $(WHITE)Full clean build + sign + run...$(RESET)"
	@make clean all -C source && make sign-kernel && make iso tarball run-x86-bios

# -----------------------------
# Editor support (clangd / Code - OSS)
# -----------------------------
clangd:
	@echo -e "$(INFO) $(WHITE)Generating compile_commands.json for clangd...$(RESET)"
	@$(MAKE) -C source compile-commands
	@echo -e "$(OK) $(CYAN)Done.$(RESET) Open this repo in Code - OSS and install the clangd extension."

clang-format:
	@echo -e "$(INFO) $(WHITE)Formatting source tree...$(RESET)"
	@find . -name "*.c" -o -name "*.h" | xargs clang-format -i
	@echo -e "$(OK) $(CYAN)Formatting complete.$(RESET)"

# -----------------------------
# Kernel signing
# -----------------------------
sign-kernel:
	@echo -e "$(INFO) $(WHITE)Signing wing_kernel.elf...$(RESET)"
	@openssl dgst -sha256 -sign ./keys/private_key.pem -out ./keys/file.sig ./source/wing_kernel.elf
	@openssl dgst -sha256 -verify ./keys/public_key.pem -signature ./keys/file.sig ./source/wing_kernel.elf
	@echo -e "$(OK) $(CYAN)Kernel signed and verified.$(RESET)"

# -----------------------------
# Fonts
# -----------------------------
fonts:
	@echo -e "$(INFO) $(WHITE)Converting font.sfn from fira.ttf...$(RESET)"
	./sfnconv -U -B 32 ./fira.ttf ./source/boot/font.sfn
	@echo -e "$(OK) $(CYAN)Font ready.$(RESET)"

# -----------------------------
# Cleanup
# -----------------------------
clean:
	@echo -e "$(INFO) $(GRAY)Sweeping away build artifacts...$(RESET)"
	@rm -rf ./disk_root $(ISO_FILE) $(ISO_FILE).tar.gz serial.log
	@cd source && make deep-clean && cd ..
	@echo -e "$(OK) $(CYAN)Clean.$(RESET)"

# -----------------------------
# Making root fs
# -----------------------------
root-disk:
	@echo -e $(SUDO_MESSAGE)
	@sudo -v

	@echo -e "$(INFO) $(WHITE)Creating GPT disk image...$(RESET)"
	@rm -f disk.img
	@truncate -s 80M disk.img

	@parted -s disk.img mklabel gpt
	@parted -s disk.img mkpart primary ext2 1MiB 65MiB

	@LOOP=$$(sudo losetup --find --show --partscan disk.img); \
	echo -e "$(INFO) $(GRAY)Loop device: $$LOOP$(RESET)"; \
	echo -e "$(INFO) $(GRAY)Formatting Ext2...$(RESET)"; \
	sudo mkfs.ext2 -F $${LOOP}p1 >/dev/null; \
	sudo mkdir -p /tmp/frost-root; \
	echo -e "$(INFO) $(GRAY)Mounting...$(RESET)"; \
	sudo mount $${LOOP}p1 /tmp/frost-root; \
	echo -e "$(INFO) $(GRAY)Copying fs_root...$(RESET)"; \
	sudo cp -a ./fs_root/. /tmp/frost-root/; \
	sync; \
	echo -e "$(INFO) $(GRAY)Unmounting...$(RESET)"; \
	sudo umount /tmp/frost-root; \
	sudo losetup -d $$LOOP

	@echo -e "$(OK) $(CYAN)disk.img ready.$(RESET)"
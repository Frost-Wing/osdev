all:
	@mkdir -p disk_root
	@cp -v source/wing_kernel.elf source/boot/limine.cfg source/boot/iso.f16 source/boot/bg.jpg limine/limine-bios.sys limine/limine-bios-pxe.bin limine/limine-bios-cd.bin limine/limine-uefi-cd.bin disk_root/
	
	@xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label disk_root -o FrostWing.iso

	@./limine/limine bios-install FrostWing.iso

tarball:
	@tar -czvf FrostWing.iso.tar.gz FrostWing.iso

run-x86:
	@qemu-system-x86_64 \
	-vga std \
	-debugcon stdio \
	-serial file:serial.log \
	-audiodev pa,id=speaker \
	-device rtl8139,netdev=eth0 \
	-netdev user,hostfwd=tcp::5555-:22,id=eth0 \
	-cdrom FrostWing.iso \
	-m 1024 \
	-drive id=disk,file=FrostWing.iso.tar.gz,if=none \
	-device ahci,id=ahci \
	-device ide-hd,drive=disk,bus=ahci.0

everything:
	@make clean all -C source && make all tarball run-x86
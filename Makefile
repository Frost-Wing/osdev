iso:
	@rm -rf ./disk_root
	@mkdir -p disk_root
	@cp -v \
	source/wing_kernel.elf \
	source/boot/limine.cfg \
	source/boot/libFrostedWM.so \
	source/boot/Vera.sfn \
	limine/limine-bios.sys \
	limine/limine-bios-pxe.bin \
	limine/limine-bios-cd.bin \
	limine/limine-uefi-cd.bin \
	disk_root/

	@mkdir -p disk_root/EFI/BOOT
	@cp -v \
	limine/BOOTX64.EFI \
	limine/BOOTAA64.EFI \
	limine/BOOTIA32.EFI \
	limine/BOOTRISCV64.EFI \
	disk_root/EFI/BOOT/
	
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
	-drive id=disk,file=disk.txt,if=none \
	-device ahci,id=ahci \
	-device ide-hd,drive=disk,bus=ahci.0 \
	-m 512

run-x86-hdd:
	@qemu-system-x86_64 \
	-vga std \
	-debugcon stdio \
	-serial file:serial.log \
	-audiodev pa,id=speaker \
	-device rtl8139,netdev=eth0 \
	-netdev user,hostfwd=tcp::5555-:22,id=eth0 \
	-hda FrostWing.img \
	-drive id=disk,file=disk.txt,if=none \
	-device ahci,id=ahci \
	-device ide-hd,drive=disk,bus=ahci.0 \
	-m 512

run-x86-uefi:
	@qemu-system-x86_64 \
	-bios ./firmware/uefi/tianocore-64.uefi \
	-vga std \
	-debugcon stdio \
	-serial file:serial.log \
	-audiodev pa,id=speaker \
	-device rtl8139,netdev=eth0 \
	-netdev user,hostfwd=tcp::5555-:22,id=eth0 \
	-cdrom FrostWing.iso \
	-drive id=disk,file=disk.txt,if=none \
	-device ahci,id=ahci \
	-device ide-hd,drive=disk,bus=ahci.0 \
	-m 512

run-x86-vnc:
	@qemu-system-x86_64 \
	-vga std \
	-debugcon stdio \
	-serial file:serial.log \
	-device rtl8139,netdev=eth0 \
	-netdev user,hostfwd=tcp::5555-:22,id=eth0 \
	-cdrom FrostWing.iso \
	-m 256 \
	-no-reboot \
	-vnc :1 -display none &

everything:
	@make clean all -C source && make iso tarball run-x86

everything-sign:
	@make clean all -C source && make sign-kernel && make iso tarball run-x86

doxygen:
	# I coded this for my use but you can you can use it, if you know why I have this.
	doxygen
	cd docs/html && code . && cd ../../

latest-limine:
	rm -r limine
	git clone https://github.com/limine-bootloader/limine.git --branch=v6.x-branch-binary --depth=1
	make -C limine

sign-kernel:
	@openssl dgst -sha256 -sign ./keys/private_key.pem -out ./keys/file.sig ./source/wing_kernel.elf
	@openssl dgst -sha256 -verify ./keys/public_key.pem -signature ./keys/file.sig ./source/wing_kernel.elf

fonts:
	sfnconv -U -B 40 -t b1 ~/Downloads/FiraSans-Regular.ttf ~/Desktop/FrostWing/source/boot/Vera.sfn

top-clean:
	@rm -rf ./disk_root
	@rm -rf FrostWing.iso
	@rm -rf FrostWing.iso.sha256
	@rm -rf FrostWing.iso.tar.gz
	@rm -rf FrostWing.iso.tar.gz.sha256
	@rm -rf serial.log
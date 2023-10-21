all:
	cp -v source/wing_kernel.elf source/boot/limine.cfg source/boot/iso.f16 source/boot/bg.jpg limine/limine-bios.sys limine/limine-bios-pxe.bin limine/limine-bios-cd.bin limine/limine-uefi-cd.bin disk_root/
	
	xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label disk_root -o FrostWing.iso

	./limine/limine bios-install FrostWing.iso

run-x86:
	qemu-system-x86_64 -cdrom FrostWing.iso -m 512

run-aarch:
	qemu-system-aarch64 -cdrom FrostWing.iso -m 512

run-riscv:
	qemu-system-riscv64 -cdrom FrostWing.iso -m 512
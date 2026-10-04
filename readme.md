![FrostWing Banner](https://repository-images.githubusercontent.com/708065850/eeb30fa3-303e-4ed5-89ea-0f057cf38582)
# FrostWing Operating System

FrostWing is a lightweight, flexible x86_64 operating system with a clean, aesthetic design. This README will guide you through the process of understanding the overall concept of the FrostWing Operating System.

![GitHub all releases](https://img.shields.io/github/downloads/Frost-Wing/osdev/total?style=flat-square&label=Downloads)
![GitHub](https://img.shields.io/github/license/Frost-Wing/osdev?style=flat-square&label=License)
![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/Frost-Wing/osdev?style=flat-square)
![GitHub repo size](https://img.shields.io/github/repo-size/Frost-Wing/osdev?style=flat-square&label=Repository%20Size)
![GitHub Workflow Status (with event)](https://img.shields.io/github/actions/workflow/status/Frost-Wing/osdev/build.yml?style=flat-square&label=Current%20Code%20Compiling%20(Workflows))

> [!NOTE]
> To get a really good overview of this repository please visit https://githubtracker.com/Frost-Wing/osdev

## Table of Contents

- [FrostWing Operating System](#frostwing-operating-system)
  - [Table of Contents](#table-of-contents)
  - [Gallery](#gallery)
  - [Currently working Features](#currently-working-features)
    - [Dynamically linked user programs](#dynamically-linked-user-programs)
    - [Getting started](#getting-started)
    - [Hardware/Software (Emulator) Requirements](#hardwaresoftware-emulator-requirements)
    - [Booting to real machine](#booting-to-real-machine)
  - [Contributing](#contributing)
  - [License](#license)
  - [FrostWing Team](#frostwing-team)

## Gallery
[Refer the wiki for Gallary.*](https://github.com/Frost-Wing/osdev/wiki/%E2%80%90-Gallery)

## Currently working Features
- Interrupts
- ACPI
    - Shutdown
    - Reboot
- AHCI
    - Detecting Disks
- CPU-ID
- GDT
    - BIOS
    - UEFI
- TSS
- Hardware abstraction layer (HAL)
- Memory
    - Heap memory allocator
    - Paging
- PCI
    - Probing
    - Initializing required drivers
    - Storing the devices list
- Timing
    - Real Time Clock
    - Programmable Interval Timer
- Secure Boot
    - UEFI
    - BIOS
- PS/2
    - Keyboard
    - Mouse
- Graphics
    - OpenGL Renderer
    - Terminal emulator
    - Graphics card support
- Networking
    - Ethernet
        - RTL Cards
            - RTL-8139 Networking
- Serial communications (with Arduino, NodeMCU, Sparkfun, etc.)
- Audio
    - PC Speaker

**AND MUCH MORE...**

### Dynamically linked user programs

FrostWing's ELF loader supports x86-64 ELF interpreters and starts the program's
interpreter; dynamically linked programs must also have every `DT_NEEDED`
library available in the root filesystem. Build a musl-linked program with its
normal dynamic defaults, then stage it and its dependencies before creating the
root disk:

```sh
musl-gcc input.c -o input
./scripts/stage-elf.py ./input
make root-disk
```

The staging helper inspects ELF metadata without executing the input, copies the
program to `fs_root/bin`, and places its interpreter and recursive shared-library
dependencies under `fs_root/lib` or `fs_root/lib64` according to the interpreter
path. `make root-disk` replaces the existing `disk.img`, so back up any data on
that image first. Review the staged files before distributing an image. This supports
musl-linked x86-64 programs to the extent of FrostWing's Linux syscall
compatibility; copying a glibc executable and its libraries does not make
arbitrary Linux/glibc programs compatible.

### Getting started
[*Please refer wiki for steps for compiling**](https://github.com/Frost-Wing/osdev/wiki)

### Hardware/Software (Emulator) Requirements
Works on any x86_64 processor

| | BIOS (min) | BIOS (recommended) | UEFI (min) | UEFI (recommended) |
|---|---|---|---|---|
| **RAM** | 75 MB | 128 MB | 170 MB | 256 MB |
| **Graphics** | Framebuffer | Integrated | Framebuffer | Integrated |


### Booting to real machine
This operating system is real machine **bootable** and tested under the following circumstances:

Boot Disk information:
- Using 32 GB Pendrive
- GPT Disk (MBR Also works)
- Both BIOS / UEFI supported
- Ventoy loaded
- FrostWing.iso in the root directory

> [!IMPORTANT]
> It is recommended to use Ventoy because you will not have the risk of flashing and failing of USB-Drives.

## Contributing

We welcome contributions to FrostWing! If you'd like to contribute code, report bugs, or suggest enhancements, please check our [contribution guidelines](https://github.com/Frost-Wing/osdev/blob/main/CONTRIBUTING.md).

## License

FrostWing is open-source software released under the [CC0-1.0 License](https://github.com/Frost-Wing/osdev/blob/main/LICENSE). Feel free to use, modify, and distribute it as per the terms of this license.

## FrostWing Team
- Owner, founder & "The Dev" - Pradosh ([@PradoshGame](https://twitter.com/@PradoshGame))
- OpenGL & Head Developer - GAMINGNOOB ([@GAMINGNOOBdev](https://github.com/GAMINGNOOBdev)) (inactive)
- Sources
    - [Flanterm](https://github.com/mintsuki/flanterm/tree/trunk) from Mintsuki
    - [ACPI and Shutdown](https://github.com/mintsuki/acpi-shutdown-hack) from Mintsuki
    - [Floating Point Arithmetic](https://github.com/stevej/osdev/blob/master/kernel/devices/fpu.c) from Kevin Lange
    - [UEFI Binary](https://github.com/BlankOn/ovmf-blobs/tree/master) from BlankOn
    - [Interrupts](https://github.com/RickleAndMortimer/MakenOS/tree/master/kernel) from MakenOS Kernel

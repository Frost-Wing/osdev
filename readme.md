# FrostWing Operating System

Welcome to FrostWing, a lightweight and flexible operating system designed for x86_64, ARM64 (aarch64), and RISC-V (riscv64) architectures. This README will guide you through the process of building and running FrostWing, along with an overview of its features and architecture.

![GitHub all releases](https://img.shields.io/github/downloads/Frost-Wing/osdev/total?style=flat-square&label=Downloads)
![GitHub](https://img.shields.io/github/license/Frost-Wing/osdev?style=flat-square&label=License)
![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/Frost-Wing/osdev?style=flat-square)
![GitHub repo size](https://img.shields.io/github/repo-size/Frost-Wing/osdev?style=flat-square&label=Repository%20Size)
![GitHub Workflow Status (with event)](https://img.shields.io/github/actions/workflow/status/Frost-Wing/osdev/build.yml?style=flat-square&label=Current%20Code%20Compiling%20(Workflows))

> [!NOTE]
> To get a really good overview of this repository please visit https://repo-tracker.com/r/gh/Frost-Wing/osdev

## Table of Contents

- [Images](#images)
- [Features](#currently-working-features)
- [Getting Started](#getting-started)
  - [Hardware/Software (Emulator) Requirements](#hardwaresoftware-emulator-requirements)
  - [Booting to real machine](#Booting-to-real-machine)
- [Contributing](#contributing)
- [License](#license)
- [FrostWing Team](#frostwing-team)

## Images
### Running (Early Booting)
[![https://imgur.com/xY5Qzac.png](https://imgur.com/xY5Qzac.png)](https://imgur.com/xY5Qzac.png)

### Running (Graphics Screen)
[![https://imgur.com/22Uwf8a.png](https://imgur.com/22Uwf8a.png)](https://imgur.com/22Uwf8a.png)

### Running (Initial Login Screen)
![Screenshot from 2025-01-15 08-49-29](https://github.com/user-attachments/assets/a6ceca1c-3be5-45d9-8ee1-5856619e1f8a)

### Running (Frosted Shell - fsh)
![image](https://github.com/user-attachments/assets/976facae-3bc6-4033-bead-53800af2be4d)

### Running (Exit and logging in)
![image](https://github.com/user-attachments/assets/0319336b-bee5-43f4-b89b-88a721791468)


### Meltdown (Panic) Screen
[![https://imgur.com/HqYJvMK.png](https://imgur.com/HqYJvMK.png)](https://imgur.com/HqYJvMK.png)

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
- Hardware abstraction layer
- Memory
    - Heap memory allocator
        - malloc
        - free
    - Paging
        - allocate pages
        - free pages
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

### Getting started
[*Please refer wiki for steps for compiling**](https://github.com/Frost-Wing/osdev/wiki)

### Hardware/Software (Emulator) Requirements
#### Minimum Requirements (BIOS)
- **CPU** Currently any x86_64 proccessor
- **RAM** 75 MB
- **Storage** Nothing required yet.
- **Graphics** Only Framebuffer.

#### Minimum Requirements (UEFI)
- **CPU** Currently any x86_64 proccessor
- **RAM** 170 MB
- **Storage** Nothing required yet.
- **Graphics** Only Framebuffer.

#### Recommended Requirements (BIOS)
- **CPU** Currently any x86_64 proccessor
- **RAM** 128 MB
- **Storage** Nothing required yet.
- **Graphics** Integrated Graphics

#### Recommended Requirements (UEFI)
- **CPU** Currently any x86_64 proccessor
- **RAM** 256 MB
- **Storage** Nothing required yet.
- **Graphics** Integrated Graphics

### Booting to real machine
This operating system is real machine **bootable** and tested under the following circumstances:

Boot Disk information:
- Using 32 GB Pendrive
- GPT Disk (MBR Also works)
- Both BIOS / UEFI supported
- Ventoy loaded
- FrostWing.iso in the root directory

> [!IMPORTANT]
> It is recommended to use Ventoy because you will not have the risk of flashing and failing of USB-Drives

## Contributing

We welcome contributions to FrostWing! If you'd like to contribute code, report bugs, or suggest enhancements, please check our [contribution guidelines](https://github.com/Frost-Wing/osdev/blob/main/CONTRIBUTING.md).

## License

FrostWing is open-source software released under the [CC0-1.0 License](https://github.com/Frost-Wing/osdev/blob/main/LICENSE). Feel free to use, modify, and distribute it as per the terms of this license.

## FrostWing Team
- Owner and founder - Pradosh ([@PradoshGame](https://twitter.com/@PradoshGame))
- OpenGL & Head Developer - GAMINGNOOB ([@GAMINGNOOBdev](https://github.com/GAMINGNOOBdev))
- Sources
    - [Flanterm](https://github.com/mintsuki/flanterm/tree/trunk) from Mintsuki
    - [ACPI and Shutdown](https://github.com/mintsuki/acpi-shutdown-hack) from Mintsuki
    - [Floating Point Arithmetic](https://github.com/stevej/osdev/blob/master/kernel/devices/fpu.c) from Kevin Lange
    - [UEFI Binary](https://github.com/BlankOn/ovmf-blobs/tree/master) from BlankOn
    - [Interrupts](https://github.com/RickleAndMortimer/MakenOS/tree/master/kernel) from MakenOS Kernel

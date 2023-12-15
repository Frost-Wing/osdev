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
  - [Prerequisites](#prerequisites)
  - [Building FrostWing](#building-FrostWing)
  - [Building FrostWing Documentations](#building-the-frostwing-doxygen-documentation)
  - [Hardware/Software (Emulator) Requirements](#hardwaresoftware-emulator-requirements)
  - [Running FrostWing](#running-FrostWing)
  - [Booting to real machine](#Booting-to-real-machine)
- [Features](#features)
- [Contributing](#contributing)
- [License](#license)
- [FrostWing Team](#frostwing-team)

## Images
### Running (Early Booting)
[![https://imgur.com/xY5Qzac.png](https://imgur.com/xY5Qzac.png)](https://imgur.com/xY5Qzac.png)

### Running (Graphics Screen)
[![https://imgur.com/22Uwf8a.png](https://imgur.com/22Uwf8a.png)](https://imgur.com/22Uwf8a.png)

### Meltdown (Panic) Screen
[![https://imgur.com/HqYJvMK.png](https://imgur.com/HqYJvMK.png)](https://imgur.com/HqYJvMK.png)

## Currently working Features
- ACPI
    - Shutdown
    - Reboot
- AHCI
    - Detecting Disks
- CPU-ID
- GDT
- Hardware abstraction layer
- Heap memory allocator
    - malloc
    - free
- PCI
- Real Time Clock
- Secure Boot
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
## Getting Started

### Prerequisites

Before you start building FrostWing, ensure that you have the following dependencies installed:

- Latest Version of [Limine Bootloader](https://github.com/limine-bootloader/limine)
```bash
git clone https://github.com/limine-bootloader/limine.git --branch=v5.x-branch-binary --depth=1
```
- Compile the Limine bootloader
```bash
make -C limine
```
> [!NOTE]
> A suitable cross-compiler for your target architecture (x86_64, ARM64, or RISC-V) is always recommended but the os is never tested with a cross-compiler and therefore it is optional

- QEMU System Emulator

### Building FrostWing

1. **Clone this repository to your local machine**
    ```bash
    git clone https://github.com/pradosh-arduino/FrostWing
    ```
2. **Change into the FrostWing directory:**
    ```bash
    cd FrostWing
    ```
3. **Build FrostWing for your target architecture:**
- For x86_64:
    ```bash
    make -C source
    make
    ```
- For aarch64 (ARM64):
    ```bash
    make -C source ARCH="aarch64"
    make
    ```
- For riscv64 (RISC-V):
    ```bash
    make -C source ARCH="riscv64"
    make
    ```
4. (Optional - Recommened) To use custom cross compiler:
    ```bash
    make -C source ARCH="xx" CC="cc"
    make
    ```

### Building the FrostWing Doxygen documentation
> [!NOTE]
> Slower Method below

If you want to have the doxygen documentation (for some reason), it's enough to just run
```bash
 doxygen
```
inside the repository folder. A directory called `docs` will be created which should contain the generated html documentation.
The main html file is located at: `docs/html/index.html`

> [!NOTE]
> Better Method below

Visit [Doxygen Documentation for FrostWing](https://frost-wing.github.io/doxygen-docs/) for pre-compiled documentations
### Hardware/Software (Emulator) Requirements
#### Minimum Requirements
- **CPU** Currently any x86_64 proccessor
- **RAM** 75 MB
- **Storage** Nothing required yet.
- **Graphics** Only Framebuffer.

#### Recommended Requirements
- **CPU** Currently any x86_64 proccessor
- **RAM** 128 MB
- **Storage** Nothing required yet.
- **Graphics** Integrated Graphics

### Running FrostWing

After building FrostWing, you can run it using a virtual machine or on real hardware. The specific steps for running it will depend on your chosen target architecture. Read the main Markdown to run FrostWing.

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
## Features

- Modular architecture for extensibility and maintainability.
- Support for multiple target architectures: x86_64, ARM64 (aarch64), and RISC-V (riscv64).
- Simple and clean codebase with documentation.

## Contributing

We welcome contributions to FrostWing! If you'd like to contribute code, report bugs, or suggest enhancements, please check our [contribution guidelines](https://github.com/Frost-Wing/osdev/blob/main/CONTRIBUTING.md).

## License

FrostWing is open-source software released under the [CC0-1.0 License](https://github.com/Frost-Wing/osdev/blob/main/LICENSE). Feel free to use, modify, and distribute it as per the terms of this license.

Happy coding, and let's make FrostWing even more awesome together! 😎🚀

## FrostWing Team
- Owner and founder - Pradosh ([@PradoshGame](https://twitter.com/@PradoshGame))
- Head Developer - GAMINGNOOB ([@GAMINGNOOBdev](https://github.com/GAMINGNOOBdev))
- Sources
    - [Flanterm](https://github.com/mintsuki/flanterm/tree/trunk) from Mintsuki
    - [ACPI and Shutdown](https://github.com/mintsuki/acpi-shutdown-hack) from Mintsuki
    - [Floating Point Arithmetic](https://github.com/stevej/osdev/blob/master/kernel/devices/fpu.c) from Kevin Lange
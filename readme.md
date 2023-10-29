# FrostWing Operating System

Welcome to FrostWing, a lightweight and flexible operating system designed for x86_64, ARM64 (aarch64), and RISC-V (riscv64) architectures. This README will guide you through the process of building and running FrostWing, along with an overview of its features and architecture.

## Table of Contents

- [Images](#images)
- [Getting Started](#getting-started)
  - [Directory hierarchy](#directory-hierarchy)
  - [Prerequisites](#prerequisites)
  - [Building FrostWing](#building-FrostWing)
  - [Building FrostWing Documentations](#building-the-frostwing-doxygen-documentation)
  - [Running FrostWing](#running-FrostWing)
  - [Booting to real machine](#Booting-to-real-machine)
- [Features](#features)
- [Contributing](#contributing)
- [License](#license)
- [FrostWing Team](#frostwing-team)

## Images
[![https://imgur.com/6CN4F98.png](https://imgur.com/6CN4F98.png)](https://imgur.com/6CN4F98.png)

## Getting Started

### Directory Hierarchy
```c
├── disk_root
│   └── // The files that are going to go to the ISO.
├── limine
│   └── // Bootloader files
├── source
│   ├── boot
│   │   └── // Configurations, fonts, background
│   ├── includes
│   │   └── // Header files
│   ├── kernel
│   │   └── // Main kernel code
│   ├── linker
│   │   ├── x86_64.ld
│   │   ├── aarch64.ld
│   │   └── riscv64.ld
│   ├── obj
│   │   └── // Object files
│   ├── Makefile
│   └── wing_kernel.elf
└── ...
```
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
- A suitable cross-compiler for your target architecture (x86_64, ARM64, or RISC-V).
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
    make -C source ARCH="ARCH" CC="cc"
    make
    ```

### Building the FrostWing Doxygen documentation

If you want to have the doxygen documentation (for some reason), it's enough to just run
```bash
doxygen
```
inside the repository folder. A directory called `docs` will be created which should contain the generated html documentation.
The main html file is located at: `docs/html/index.html`

### Running FrostWing

After building FrostWing, you can run it using a virtual machine or on real hardware. The specific steps for running it will depend on your chosen target architecture. Read the main Markdown to run FrostWing.

### Booting to real machine
This process is in pending and we have not tried it yet.

## Features

- Modular architecture for extensibility and maintainability.
- Support for multiple target architectures: x86_64, ARM64 (aarch64), and RISC-V (riscv64).
- Simple and clean codebase with documentation.

## Contributing

We welcome contributions to FrostWing! If you'd like to contribute code, report bugs, or suggest enhancements, please check our [contribution guidelines](CONTRIBUTING.md).

## License

FrostWing is open-source software released under the [CC0-1.0 License](LICENSE). Feel free to use, modify, and distribute it as per the terms of this license.

Happy coding, and let's make FrostWing even more awesome together! 😎🚀

## FrostWing Team
- Owner and founder - Pradosh ([@PradoshGame](https://twitter.com/@PradoshGame))
- Head Developer - GAMINGNOOB ([@GAMINGNOOBdev](https://github.com/GAMINGNOOBdev))
- Sources
    - [Flanterm](https://github.com/mintsuki/flanterm/tree/trunk) from Mintsuki
    - [ACPI and Shutdown](https://github.com/mintsuki/acpi-shutdown-hack) from Mintsuki
    - [Floating Point Arithmetic](https://github.com/stevej/osdev/blob/master/kernel/devices/fpu.c) from Kevin Lange
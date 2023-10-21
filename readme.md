# FrostWing Operating System

Welcome to FrostWing, a lightweight and flexible operating system designed for x86_64, ARM64 (aarch64), and RISC-V (riscv64) architectures. This README will guide you through the process of building and running FrostWing, along with an overview of its features and architecture.

## Table of Contents

- [Getting Started](#getting-started)
  - [Directory hierarchy](#directory-hierarchy)
  - [Prerequisites](#prerequisites)
  - [Building FrostWing](#building-FrostWing)
  - [Running FrostWing](#running-FrostWing)
  - [Booting to real machine](#Booting-to-real-machine)
- [Features](#features)
- [Contributing](#contributing)
- [License](#license)
- [FrostWing Team](#frostwing-team)

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

- Create a folder named `disk_root`
- Latest Version of [Limine Bootloader](https://github.com/limine-bootloader/limine)
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

### Running FrostWing

After building FrostWing, you can run it using a virtual machine or on real hardware. The specific steps for running it will depend on your chosen target architecture. Read the main Markdown to run FrostWing.

### Booting to real machine
This process is in pending and we have not tried it yet.

## Features

- Multiboot-compliant, making it compatible with a wide range of bootloaders, including Limine.
- Modular architecture for extensibility and maintainability.
- Support for multiple target architectures: x86_64, ARM64 (aarch64), and RISC-V (riscv64).
- Simple and clean codebase with documentation.

## Contributing

We welcome contributions to FrostWing! If you'd like to contribute code, report bugs, or suggest enhancements, please check our [contribution guidelines](CONTRIBUTING.md).

## License

FrostWing is open-source software released under the [MIT License](LICENSE). Feel free to use, modify, and distribute it as per the terms of this license.

Happy coding, and let's make FrostWing even more awesome together! 😎🚀

## FrostWing Team
- Owner and founder - Pradosh (@PradoshGame)
- Head Developer - Not here yet.
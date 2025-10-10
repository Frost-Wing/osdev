# ===================================
# FrostWing OS Makefile (x86_64 only)
# ===================================

# Nuke built-in rules and variables
override MAKEFLAGS += -rR

# Final kernel output
KERNEL := wing_kernel.elf

# Toolchain
CC := fwgcc
LD := fwld
NASM := nasm

# Directories
SRC_DIRS := kernel drivers
OBJ_DIR := obj

# Flags
CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto \
          -fPIE -Wall -O3 -pipe -w \
          -I includes -I src -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-red-zone \
          -mno-sse2 -mno-sse

CPPFLAGS := -MMD -MP
LDFLAGS := -nostdlib -static --no-dynamic-linker -z text -z max-page-size=0x1000 -T ./linker/linker.ld -m elf_x86_64
NASMFLAGS := -Wall -f elf64

# Sources
CFILES := $(shell find -L $(SRC_DIRS) -type f -name '*.c')
ASFILES := $(shell find -L $(SRC_DIRS) -type f -name '*.S')
NASMFILES := $(shell find -L $(SRC_DIRS) -type f -name '*.asm')

# Objects
OBJ := $(addprefix $(OBJ_DIR)/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o) $(NASMFILES:.asm=.asm.o))
DEPS := $(OBJ:.o=.d)

# ============================
# Targets
# ============================

.PHONY: all clean deep-clean version_info

all: version_info $(KERNEL)

# Final kernel link
$(KERNEL): $(OBJ) linker/linker.ld
	@echo -e "\033[1;32m[LD]\033[0m $@"
	@$(LD) $(OBJ) $(LDFLAGS) -o $@

# C source compile
$(OBJ_DIR)/%.c.o: %.c includes/limine.h
	@mkdir -p $(@D)
	@echo -e "\033[1;34m[CC]\033[0m $<"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Assembly source (.S) compile
$(OBJ_DIR)/%.S.o: %.S
	@mkdir -p $(@D)
	@echo -e "\033[1;34m[AS]\033[0m $<"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# NASM source (.asm) assemble
$(OBJ_DIR)/%.asm.o: %.asm
	@mkdir -p $(@D)
	@echo -e "\033[1;34m[NASM]\033[0m $<"
	@$(NASM) $(NASMFLAGS) $< -o $@

# Include dependency files
-include $(DEPS)

# Cleaning
clean:
	@echo -e "\033[1;31m[RM]\033[0m Object files"
	@rm -rf $(OBJ) $(DEPS)

deep-clean:
	@echo -e "\033[1;31m[RM]\033[0m All build outputs"
	@rm -rf $(OBJ_DIR) $(KERNEL)

# Handling Debugging Infos
version_info:
	@echo -e "\033[1;32m[SH]\033[0m Fetching the versions of toolchains..."
	@./versions.sh

# ===================================
# FrostWing OS Makefile (x86_64 only)
# ===================================

SHELL := /bin/bash

# Nuke built-in rules and variables
override MAKEFLAGS += -rR

# ================================================================
#  FrostWing colour palette (kept consistent with the top-level Makefile)
# ================================================================
BLUE    := \033[1;34m
CYAN    := \033[1;36m
WHITE   := \033[1;37m
GRAY    := \033[0;90m
RED     := \033[1;31m
BOLD    := \033[1m
RESET   := \033[0m

# Status prefixes
INFO := $(BLUE)❄$(RESET)
OK   := $(CYAN)✓$(RESET)
WARN := $(RED)!$(RESET)

# Final kernel output
KERNEL := wing_kernel.elf

# Toolchain (prefer FrostWing toolchain, else fallback to system)
CC := $(shell command -v fwgcc 2>/dev/null || echo gcc)
LD := $(shell command -v fwld 2>/dev/null || echo ld)
NASM := nasm

ifeq ($(CC),gcc)
$(warning =====================================)
$(warning ❄  FrostWing toolchain NOT found)
$(warning ❄  Using system GCC — this may break OS!)
$(warning =====================================)
endif

# Directories
SRC_DIRS := kernel drivers
OBJ_DIR := obj
CRASH_SYMBOLS_C := kernel/C/crash_symbols.generated.c

# Flags
CFLAGS := -std=gnu11 -ffreestanding \
          -w \
          -fno-common \
          -fno-stack-protector -fno-stack-check \
          -fno-lto -fno-builtin \
          -fno-omit-frame-pointer \
          -Og -g -fPIE \
          -I includes -I src \
          -m64 -march=x86-64 \
          -mno-80387 -mno-mmx -mno-red-zone \
          -mno-sse -mno-sse2 \
          -msoft-float

CPPFLAGS := -MMD -MP
LDFLAGS := -nostdlib -static --no-dynamic-linker -z text -z max-page-size=0x1000 -T ./linker/linker.ld -m elf_x86_64
NASMFLAGS := -Wall -f elf64

# Sources
CFILES := $(filter-out $(CRASH_SYMBOLS_C),$(shell find -L $(SRC_DIRS) -type f -name '*.c'))
ASFILES := $(shell find -L $(SRC_DIRS) -type f -name '*.S')
NASMFILES := $(shell find -L $(SRC_DIRS) -type f -name '*.asm')

# Objects
OBJ := $(addprefix $(OBJ_DIR)/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o) $(NASMFILES:.asm=.asm.o))
CRASH_SYMBOLS_OBJ := $(OBJ_DIR)/$(CRASH_SYMBOLS_C).o
DEPS := $(OBJ:.o=.d)

# ============================
# Targets
# ============================
.PHONY: all clean deep-clean version_info compile-commands

all: version_info compile-commands $(KERNEL)
	@echo -e "$(BLUE)────────────────────────────────────────────$(RESET)"
	@echo -e "$(OK) $(CYAN)FrostWing kernel build complete:$(RESET) $(BOLD)$(KERNEL)$(RESET)"

# Final kernel link
$(KERNEL): $(OBJ) linker/linker.ld
	@echo -e "$(CYAN)[LD]$(RESET) $(WHITE)$@ bootstrap$(RESET)"
	@$(LD) $(OBJ) $(LDFLAGS) -o $@
	@echo -e "$(CYAN)[SYM]$(RESET) $(WHITE)Generating crash diagnosis symbols$(RESET)"
	@python3 ./scripts/gen_crash_symbols.py --kernel $@ --root $(CURDIR) --out $(CRASH_SYMBOLS_C)
	@mkdir -p $(dir $(CRASH_SYMBOLS_OBJ))
	@echo -e "$(BLUE)[CC]$(RESET) $(GRAY)$(CRASH_SYMBOLS_C)$(RESET)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c $(CRASH_SYMBOLS_C) -o $(CRASH_SYMBOLS_OBJ)
	@echo -e "$(CYAN)[LD]$(RESET) $(WHITE)$@ final$(RESET)"
	@$(LD) $(OBJ) $(CRASH_SYMBOLS_OBJ) $(LDFLAGS) -o $@

# C source compile
$(OBJ_DIR)/%.c.o: %.c includes/limine.h
	@mkdir -p $(@D)
	@echo -e "$(BLUE)[CC]$(RESET) $(GRAY)$<$(RESET)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Assembly source (.S) compile
$(OBJ_DIR)/%.S.o: %.S
	@mkdir -p $(@D)
	@echo -e "$(BLUE)[AS]$(RESET) $(GRAY)$<$(RESET)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# NASM source (.asm) assemble
$(OBJ_DIR)/%.asm.o: %.asm
	@mkdir -p $(@D)
	@echo -e "$(BLUE)[NASM]$(RESET) $(GRAY)$<$(RESET)"
	@$(NASM) $(NASMFLAGS) $< -o $@

# Include dependency files
-include $(DEPS)

compile-commands:
	@echo -e "$(CYAN)[CCDB]$(RESET) $(WHITE)Generating compile_commands.json for clangd$(RESET)"
	@SRC_ROOT=$(CURDIR) CFILES='$(CFILES)' CFLAGS='$(CFLAGS)' CPPFLAGS='$(CPPFLAGS)' OUT_PATH='$(CURDIR)/compile_commands.json' \
        python3 ./scripts/gen_compile_commands.py

# Cleaning
clean:
	@echo -e "$(RED)[RM]$(RESET) $(WHITE)Object files$(RESET)"
	@rm -rf $(OBJ) $(DEPS)
	@echo -e "$(OK) $(CYAN)Objects cleaned.$(RESET)"

deep-clean:
	@echo -e "$(RED)[RM]$(RESET) $(WHITE)All build outputs$(RESET)"
	@rm -rf $(OBJ_DIR) $(KERNEL) $(CRASH_SYMBOLS_C)
	@echo -e "$(OK) $(CYAN)Deep clean complete.$(RESET)"

# Handling Debugging Infos
version_info:
	@echo -e "$(CYAN)[SH]$(RESET) $(WHITE)Fetching the versions of toolchains...$(RESET)"
	@./versions.sh
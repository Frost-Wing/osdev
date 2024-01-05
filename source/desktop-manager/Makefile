CFLAGS = \
    -w \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -pie \
    -Wno-implicit-function-declaration \
    -I ../includes \
    -O3 \
    -m64 \
    -march=x86-64 \
    -mno-80387 \
    -mno-mmx \
    -mno-red-zone \
    -mno-sse2 \
    -mno-sse

LDFLAGS = \
    -nostdlib \
    -static \
    -pie \
    --no-dynamic-linker \
    -z text \
    -z max-page-size=0x1000 \
    -T x86_64-dm-linker.ld \
    -m elf_x86_64

override CFILES := $(shell find -L * -type f -name '*.c')
override OBJ := $(addprefix obj/,$(CFILES:.c=.c.o))
override HEADER_DEPS := $(addprefix obj/,$(CFILES:.c=.c.d))

.PHONY: all

all: desktop-manager.elf

clean:
	@rm -rf obj
	@rm -rf desktop-manager.elf

desktop-manager.elf: x86_64-dm-linker.ld $(OBJ)
	@mkdir -p "$$(dirname $@)"
	@ld $(OBJ) $(LDFLAGS) -o $@
	@echo "\033[1;32mLinking desktop manager using\033[0m $<"

obj/%.c.o: %.c
	@mkdir -p "$$(dirname $@)"
	@echo "\033[1;34mCompiling\033[0m $<"
	@gcc $(CFLAGS) -c $< -o $@
CPPFLAGS = \
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
    -march=x86-64

LDFLAGS = \
    -nostdlib \
    -static \
    -pie \
    --no-dynamic-linker \
    -z text \
    -z max-page-size=0x1000 \
    -T x86_64-dm-linker.ld

override CPPFILES := $(shell find -L * -type f -name '*.cpp')
override OBJ := $(addprefix obj/,$(CPPFILES:.cpp=.cpp.o))
override HEADER_DEPS := $(addprefix obj/,$(CPPFILES:.cpp=.cpp.d))

.PHONY: all

all:
	@make clean desktop-manager.bin finalize

clean:
	@rm -rf obj
	@rm -rf desktop-manager.bin

finalize:
	@python ./tools/signature.py

desktop-manager.bin: x86_64-dm-linker.ld $(OBJ)
	@mkdir -p "$$(dirname $@)"
	@ld $(OBJ) $(LDFLAGS) -o $@
	@echo "\033[1;32mLinking desktop manager using\033[0m $<"

obj/%.cpp.o: %.cpp
	@mkdir -p "$$(dirname $@)"
	@echo "\033[1;34mCompiling desktop manager\033[0m $<"
	@gcc $(CPPFLAGS) -c $< -o $@
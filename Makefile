ARM_GCC ?= $(shell command -v arm-none-eabi-gcc 2>/dev/null || find .tools "$(HOME)/tools" -path "*/bin/arm-none-eabi-gcc" -print -quit 2>/dev/null)
CC ?= cc
PYTHON ?= python3
CFLAGS = -std=c11 -Wall -Wextra -Werror -Iinclude
ARMFLAGS = $(CFLAGS) -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Os -g3 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -ffile-prefix-map=$(CURDIR)=.
LDFLAGS = -nostdlib -Wl,--gc-sections -Tfirmware/linker.ld
.PHONY: all clean host-test integration demo probe check-tools
all: build/monitor.elf build/receiver.elf
check-tools:
	@test -n "$(ARM_GCC)" || { echo "ARM compiler missing: run tools/bootstrap.sh or set ARM_GCC" >&2; exit 1; }
build:
	mkdir -p build
build/%.elf: firmware/%.c firmware/startup.c firmware/board.c firmware/monitor_core.c firmware/linker.ld include/board.h include/monitor_core.h | build check-tools
	$(ARM_GCC) $(ARMFLAGS) $(LDFLAGS) firmware/startup.c firmware/board.c firmware/monitor_core.c $< -o $@
probe: build/probe.elf build/receiver.elf
	ARM_GCC="$(ARM_GCC)" $(PYTHON) tests/integration.py --probe
integration: all
	ARM_GCC="$(ARM_GCC)" $(PYTHON) tests/integration.py
demo: integration
host-test: | build
	$(CC) $(CFLAGS) -g -fsanitize=address,undefined firmware/monitor_core.c tests/test_core.c -o build/test_core
	ASAN_OPTIONS=detect_leaks=0 ./build/test_core
clean:
	python3 -c "import shutil; shutil.rmtree('build', ignore_errors=True)"

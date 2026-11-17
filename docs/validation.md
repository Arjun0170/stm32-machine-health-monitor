# Validation and provenance

The primary evidence is generated only after the ARM integration assertions pass:

- [`evidence/integration.json`](../evidence/integration.json): actual receiver observations, stage intervals, successful-run marker, peripheral transaction counts, compiler/emulator versions, and SHA-256 hashes of both tested ELFs.
- [`evidence/demo.txt`](../evidence/demo.txt): receiver UART output followed by the monitor UART console.
- [`evidence/probe.txt`](../evidence/probe.txt): successful early feasibility probe and independent receiving MCU output.

## Pinned tool archives

| Tool | Official release archive | SHA-256 |
| --- | --- | --- |
| Renode 1.17.0 portable (.NET 8) | [renode-1.17.0.linux-portable.tar.gz](https://github.com/renode/renode/releases/download/v1.17.0/renode-1.17.0.linux-portable.tar.gz) | `4ba7c68b59e2447f188ef4b4b112fcccd2802582c460beedceb63b84e5605a5f` |
| xPack ARM GCC 15.2.1-1.1 | [xpack-arm-none-eabi-gcc-15.2.1-1.1-linux-x64.tar.gz](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-linux-x64.tar.gz) | `da6a49ad4003944b823c6c93702a8787c922ab34bd7e918ec0eaf6933a9b1ff6` |

Both downloaded archive hashes were checked during local validation. `tools/bootstrap.sh` verifies a new archive before extraction. Existing installations are reused, so set explicit `ARM_GCC` and `RENODE` paths when exact provenance is required. Downloads were provisioned separately for the local run; the bootstrap's shell syntax and reuse path were checked without downloading a second copy. CI's remote execution is separate from local emulator validation.

## Official peripheral sources inspected

Renode v1.17.0 pins `renode-infrastructure` at **`066a7f13c052215632d469c995c89aea37c573b1`**. The platform and source inspection preceded the full application:

- [H743 platform](https://github.com/renode/renode/blob/v1.17.0/platforms/cpus/stm32h743.repl) includes the [H7 peripheral platform](https://github.com/renode/renode/blob/v1.17.0/platforms/cpus/stm32h7.repl): USART3 at `0x40004800`, I²C1 at `0x40005400`, SPI4 at `0x40013400`, MCAN/FDCAN1 at `0x4000A000`, and message RAM at `0x4000AC00`.
- [STM32F7 I²C model at the pinned revision](https://github.com/renode/renode-infrastructure/blob/066a7f13c052215632d469c995c89aea37c573b1/src/Emulator/Peripherals/Peripherals/I2C/STM32F7_I2C.cs) implements target reads and receive-ready status, but tags `NACKF` without implementing it. Its `Update()` explicitly leaves NACK signaling as a TODO. Newer upstream code adds NACK behavior; that behavior is not attributed to the tested release.
- [STM32H7 SPI model](https://github.com/renode/renode-infrastructure/blob/066a7f13c052215632d469c995c89aea37c573b1/src/Emulator/Peripherals/Peripherals/SPI/STM32H7_SPI.cs) forwards words to a registered SPI target and invokes `FinishTransmission()` at the configured transfer size. The sensor's command boundary uses that mechanism.
- [MCAN model](https://github.com/renode/renode-infrastructure/blob/066a7f13c052215632d469c995c89aea37c573b1/src/Emulator/Peripherals/Peripherals/CAN/MCAN.cs) provides message RAM, dedicated transmit buffers, receive FIFOs, and frame delivery to a CAN hub.
- [Renode's MCAN integration suite](https://github.com/renode/renode/blob/v1.17.0/tests/peripherals/MCAN.robot) uses two machines, `SetGlobalQuantum "0.000025"`, and `SetGlobalSerialExecution True`. This project uses those same synchronization settings.

The early probe passed on H743, so no fallback MCU or host-only bus replacement was needed. The sensors are simulated peripherals connected to the real emulated bus controllers. The second CAN node is another emulated Cortex-M7 executing `build/receiver.elf`.

## Register-level behavior and known warnings

The board layer initializes the STM32 registers used by the emulator, configures 64 MHz SysTick assumptions, and supplies bounded polling for all waits. An I²C transaction writes pointer zero, waits for transfer completion, issues a repeated-start two-byte read, and observes STOP. During the injected missing response, RXNE never arrives; the 10,000-iteration bound expires, STOP is requested, and the next scheduled successful read recovers.

The simulated I²C temperature target uses 7-bit address `0x48`: write register pointer `0x00`, issue a repeated-start read, then read two little-endian signed bytes in tenths of a degree. The SPI accelerometer transaction sends `0x80 | register_address` for a read, then clocks dummy bytes to fetch sequential registers; the seven-byte transaction contains one command and six data bytes. End-of-transfer closes the target's command state. The sensor register maps are summarized in the [README](../README.md#interfaces).

Some clock divisor, timing, NSS, and flag-clear fields are modeled as tags rather than operational timing. Renode logs `Unhandled bits ... Tags:` warnings for those fields. The firmware retains the appropriate register writes; warnings do not imply verified physical timing. The relevant data paths and status fields were checked through the actual ARM firmware. The code is a freestanding emulation demonstration rather than a complete board support package.

## Recoverable issues resolved

1. UART file backend contents initially appeared empty while the emulator was still running. Reading after shutdown flushes the backend and captures complete output.
2. LeakSanitizer could not run under the local tracing/sandbox environment. AddressSanitizer and UndefinedBehaviorSanitizer remained enabled; only heap leak scanning was disabled.
3. A diagnostic counter parser initially expected decimal; Renode properties use hexadecimal. Parsing was corrected and the successful demo rerun.
4. Localhost sockets require sandbox permission in restricted environments. The test itself uses no network service or external hardware.

The reproducible commands are `make all`, `make host-test`, `make probe`, and `make demo` (`make demo` runs the same receiver-asserted integration scenario as `make integration`). Python uses its standard library; no plotting package is required. Runtime assertions are in `tests/integration.py`; unit assertions are in `tests/test_core.c`. No physical hardware, bus waveforms, or unimplemented NACK behavior is claimed as tested.

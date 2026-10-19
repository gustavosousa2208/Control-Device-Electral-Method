# Build and test the STM32 firmware

Run these commands from `fw/Control_Device_Electral_Method`. Required tools:
CMake 3.21+, Ninja, Python 3, a host C compiler, and STM32CubeCLT's
`arm-none-eabi-*` tools on PATH. The validated ARM compiler was GCC 14.3.1.

## Build

```sh
cmake -S . -B Build/Debug -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build Build/Debug --parallel 4

cmake -S . -B Build/Release -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build Build/Release --parallel 4
```

Each build produces `firmware.elf`, `.bin`, `.hex`, and `.map` in its build
folder. Generated build outputs are ignored by Git. Compilation does not flash
a board. CMake uses the checked-in generated HAL, CMSIS, startup, peripheral and
FatFs sources, the existing Flash linker script, and Cortex-M4F hard-float flags.
CubeMX/CubeIDE are needed only for regeneration or IDE use, not this build.

The `.ioc` identifies STM32F407VETx. Review CPU flags, startup and linker script
if changing the MCU. CMake scans the same application directories as `.cproject`
and excludes host tests, preview assets, CMSIS templates and nested testbenches.
No submodule initialization is required for this firmware build or host suite.

## Test

```sh
python3 Application/tests/run_tests.py
```

The suite runs 28 named host regression cases, HAL adapter checks, FAT32/exFAT
file-image roundtrips, and the actual HAL polling read/write functions against
modeled peripherals. The write test uses AddressSanitizer/UndefinedBehaviorSanitizer
and requires a host compiler with these runtimes. Temporary images and previews
are kept outside the source tree. To run the write test alone:

```sh
python3 Application/tests/sd_write_test.py
```

These host tests do not operate a physical SD card or prove electrical timing.
See [SD_WRITE_FIX.md](SD_WRITE_FIX.md) for the separate hardware evidence.

## Source completeness and configuration

The `7005c06` base commit referenced application modules that were never tracked.
This branch restores their original implementations and tests from a local copy
at that same base commit: `hw_debug`, `sofia`, `sofia_io`, `thermal_tsp`,
`tc_calibration`, `tc_heater`, and FatFs `ff_unicode.c`. Existing tracked sources
matched after newline normalization. No replacement measurement/calibration
implementations were invented. `hw_debug.h` belongs in `Application/board`.

The restored host fixtures handle Clang's predefined `__weak` macro and POSIX
file-image operations while retaining the Windows implementation.

Release uses 96,672 bytes of Flash and 17,608 bytes of RAM including linker
reservations (512-byte heap and 8 KiB minimum stack). This is not a measurement
of worst-case stack consumption. Vendor warnings concern unused parameters and
switch fallthrough.

Existing `BOARD_POWER_CONTROL_ENABLED=1` is preserved. Keep load/heater power
disconnected during storage bench checks; this change does not qualify heating
or measurement safety. The historical power-control reference describes a
blocked default; `Application/board/pinout.h` is the current configuration.

## CubeMX regeneration

Keep User Code is enabled. Pull-ups are specified in the `.ioc` and the SDIO MSP
USER CODE section. The local polling HAL patch and generated FatFs diskio sync
changes can be overwritten by regeneration or HAL upgrades: review them and rerun
tests afterward. Preserve `DISABLE_SD_INIT`; `board_sd_prepare` owns SD startup.

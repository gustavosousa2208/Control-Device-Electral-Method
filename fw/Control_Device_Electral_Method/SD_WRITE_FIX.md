# SD mount and write resolution

Base: `dev-firmware` at `7005c06`. The firmware now builds from tracked sources,
mounts after power-on without an encoder press in the observed cold-boot check,
and completes thermocouple-calibration saves on the physical card.

## What failed

The encoder long press already reached the initialization/mount retry. Live
ST-Link reads showed initialization succeeded (`sd_phase=5`, `sd_init_status=0`),
but mounting failed (`sd_mount_result=1`, `FR_DISK_ERR`). Sector-zero reads had
`HAL_SD_ERROR_DATA_CRC_FAIL` (`0x02`). No writes had been attempted at that point.
GPIO PUPDR registers confirmed no configured pull-ups on SD CMD or DAT pins.

After enabling pull-ups, manual remount and calibration saving succeeded.
Startup still sometimes returned inconsistent partition data and an
address-out-of-range read (`0x02000000`), while a later remount worked. Startup
therefore needed bounded recovery as well. The exact electrical cause of those
transient data errors was not measured with an oscilloscope; do not describe this
as a fully established hardware root cause.

The original write driver also enabled the transmit DPSM before waiting for the
CMD24/CMD25 response, leaving the CPU unable to service its FIFO during that
interval. The patch corrects this ordering. An actual TX-underrun error was not
captured from the original board, so this remains a driver correction rather
than a proven explanation of the original write-only report.

## Final changes

- Enable pull-ups on CMD PD2 and DAT0..3 PC8..11; leave CK PC12 without a pull.
  Preserve the settings in the `.ioc` and MSP USER CODE section. This matches
  [ST's SDIO GPIO guidance](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Src/stm32f4xx_hal_sd.c).
- On a failed startup/explicit idle mount, unmount, wait 250 ms, reinitialize,
  and retry, with at most three mount attempts. Each initialization also has its
  existing 300 ms delay. Stop if reinitialization fails. This is read-only mount
  recovery; successful first mounts incur no extra delay. Existing HAL timeouts
  apply individually, rather than a global startup deadline.
- Send CMD24/CMD25 before enabling the polling transmit DPSM. Reject empty or
  overflowing requests, retain the correct software-timeout error, and reject
  incomplete payloads even if DATAEND is set. Leave polling reads armed before
  their read command.
- Make `CTRL_SYNC` wait for the card's transfer-ready state with a timeout.
  Retain write status/error/address/register diagnostics before later status
  queries can change `hsd.ErrorCode`. Never retry a failed file write implicitly.
- Restore omitted application modules/tests and Unicode support; add a local
  CMake ARM build. See [BUILDING.md](BUILDING.md).

Bus configuration stays one bit at 400 kHz. Relay/heater controls, encoder
mapping, measurement algorithms and calibration storage format were not changed.
No card formatting or raw-sector writes were performed.

## Verification on 2026-10-19

| Check | Result |
| --- | --- |
| Complete Debug and Release ARM builds | PASS; ELF/BIN/HEX/map produced |
| 28 host cases and HAL adapter compilation | PASS |
| FAT32/exFAT image write, sync, remount and content comparison | PASS |
| Actual polling HAL read/write modeled tests | PASS, including write sanitizers |
| Startup recovery tests | PASS: transient success, three-attempt limit, failed reinit, no file opens |
| Release flashing through ST-Link | Flash verification and reset PASS |
| Manual mount + thermocouple-calibration reset | Owner reported success; live counters: 24 writes, zero write failures/errors; last read successful |
| Automatic recovery following programmer reset | Mounted on attempt 2 without encoder input |
| Full power-off/on with card inserted | Owner reported SD OK; live counters: attempt 1, mount FR_OK, 20 reads, no read/CRC errors |

The application save path verifies calibration files by reading them back.
The observed cold boot is one power cycle, not a long-duration reliability test.
Heater behavior, electrical signal integrity, all card models and worst-case
startup latency were not qualified by these checks.

The tested final Release BIN SHA-256 is
`69771e141708d0afc84e6aedd1faca4036ed2d1f2ac48d804b91f5f67ce794d1`.
Compiler version, paths and build configuration can affect binary hashes.

## If an SD error recurs

Inspect `g_hw_debug.sd_attempts`, `sd_phase`, `sd_init_status` and
`sd_mount_result`. In `g_sd_io_debug`, inspect `read_hal_error`, `read_sector`,
`read_crc_failures`, `write_hal_status`, `write_hal_error`, `write_sector` and
`write_failures`. `last_failed_*` intentionally retains earlier failures even
when the current operation succeeds. HAL may clear flags before `write_sta` is
sampled; the retained error mask is the primary failure evidence.

Keep load/heater power disconnected for storage checks. Preserve card contents
and inspect wiring/pull-ups/supply if errors recur; a CRC failure is not a reason
to format the card. Recheck the local HAL and diskio changes after CubeMX
regeneration or a HAL update.

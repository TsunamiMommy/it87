# IT87 Gigabyte Smart Fan extraction: validation plan

This series is a source refactor. It must not change detection, fan curves,
manual PWM behavior, electrical output selection, or firmware ownership.
The test suite performs no physical hardware writes.

## Before applying

1. Record the current BIOS Smart Fan settings and whether Fan Stop is active.
2. Save a baseline: `./tests/hwmon-snapshot.sh > before.tsv` on a tree
   containing the tests, or use equivalent read-only sysfs snapshots.
3. Record `uname -r`, `modinfo it87`, `lsmod | grep it87`, and `sensors`.
4. Confirm physical fans work normally, including BIOS control on a reboot.

## Static and host-side validation

Run from the patched repository:

```
./tests/run-smartfan-tests.sh
./tests/check-integration.py
git diff --check
make TARGET="$(uname -r)" -j"$(nproc)"
```

`run-smartfan-tests.sh` uses a register-memory emulator; it checks the
G2/G3/IT57xx vector and tachometer layouts and the exact read/write order for
manual takeover and snapshot restoration. It does not require privileges.

`check-integration.py` checks that the driver still takes the snapshot before
turning on Smart Fan and writing the manual vector, and that all new objects
are linked into the module.

A full build must also be checked against the actual target kernel headers,
including the current DKMS workflow. An isolated compilation of the new
objects is not a substitute for compiling the entire module.

## Read-only physical verification

With the modified module loaded by the normal installation method:

1. Run `./tests/hwmon-snapshot.sh > after.tsv`.
2. Verify board identity, sensor availability, PWM mode visibility, tachometer
   channel count, and fan/temperature/voltage readouts against the baseline.
3. Test one G2 board, one G3 board, one G3+IT57xx board if available, and a
   conventional non-Gigabyte IT87 device where available.
4. Check that unrelated IT8688/IT8689 extra-vector support remains usable.
5. Check `dmesg` for H2RAM version-detection regressions and read failures.

Dynamic sensor values are expected to vary. Compare available sensors and
channel mapping, not the literal RPM and temperature measurements.

## Supervised functional verification

These checks require a fan-safe test system, known-good backups of original
settings, local monitoring, and a direct restoration path. Do not execute
unattended or on a machine that relies on an unverified CPU fan fallback.

1. Confirm switching each supported fan between BIOS automatic mode and
   manual mode preserves the prior firmware configuration upon release.
2. While in manual mode, confirm reported PWM duty and tachometer feedback
   match the expected channel and remain stable.
3. Repeat for G2, G3, and any IT57xx extension channels.
4. Test transition back to BIOS control, module unload, and reboot.
5. Repeat a controlled suspend/resume cycle. Verify the driver's existing
   firmware-restore behavior and ensure no PWM override remains unexpectedly.
6. Verify BIOS Fan Stop and physical PWM/DC/Auto settings continue to work
   exactly as before; this patch does not add or modify those controls.

No test should assume that a Linux mutex excludes concurrent BIOS SMM access.
Do not use `pwm=0` as a substitute for BIOS Fan Stop testing.

## Acceptance criteria

- Apply both patches cleanly against the designated baseline.
- Compile the complete `it87.ko`, and load it on the supported test machine.
- Host vector tests and integration checks pass.
- No added or missing sensors/fans or changed channel numbering.
- Identical G2/G3/IT57xx manual-register writes and restoration order.
- No new SMM, Fan Stop, GPIO, or physical electrical-mode writes.
- No change in automatic BIOS control after restore, unload, or resume.

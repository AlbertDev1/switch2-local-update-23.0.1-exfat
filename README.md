# Switch 2 Local Update — exFAT Compatibility

![Status](https://img.shields.io/badge/status-experimental-orange)
![Firmware](https://img.shields.io/badge/firmware-23.0.1-blue)
![License](https://img.shields.io/badge/license-GPL--2.0-green)
![Platform](https://img.shields.io/badge/platform-Nintendo%20Switch-red)

## Overview

An experimental continuation of JmpCallPoo's original Switch 2
offline firmware update project, focused on extending compatibility,
preserving exFAT attributes, and resolving firmware content
filename incompatibilities.

The project uses a modified Nintendo Switch 1 and Nintendo's
built-in Local Version Matching functionality.

No custom firmware is required on the receiving Switch 2.

## Acknowledgements

First and foremost, a massive thank you to **JmpCallPoo**,
the original creator of this project.

Their reverse-engineering work, technical documentation,
implementation, and original proof of concept made this
research possible.

This repository would not exist without their contributions.
All credit for the original methodology, architecture,
sysmodules, and foundational patches belongs to the original
creator and the respective upstream contributors.

This repository is a continuation of their work, not a claim
of authorship over the original implementation.

Original project and discussion:

https://gbatemp.net/threads/updating-a-switch-2-to-23-0-1-from-a-modded-switch-1.686892/

## Contributors

- **JmpCallPoo** — Original creator, architecture, implementation,
  reverse engineering, and proof of concept.
- **AlbertDev1** — exFAT compatibility research, debugging,
  content filename resolution, and additional hardware testing.
- **Atmosphere contributors** — Custom firmware and supporting libraries.
- **Switchbrew contributors** — Nintendo Switch research and documentation.
- **libnx contributors** — Nintendo Switch development libraries.
- **devkitPro contributors** — Cross-compilation toolchain.

Special thanks to the Nintendo Switch homebrew and
reverse-engineering communities for their continued research
and willingness to share knowledge.

## Research Objectives

- Investigate compatibility with earlier Switch 2 firmware.
- Investigate exFAT-enabled receiving consoles.
- Preserve legitimate firmware content integrity.
- Identify runtime content-storage failures.
- Improve reproducibility and technical documentation.
- Maintain the original project's architecture wherever possible.

## Changes Introduced

### exFAT Attribute Preservation

The original implementation suppressed the IncludesExFatDriver
attribute for BootImagePackage entries in GetAttributes.

This repository includes an experimental modification that
preserves the native attribute in that code path.

Hardware testing confirmed:

| Attribute | Original | Modified |
|-----------|----------|----------|
| FAT BootImagePackage | 0x00 | 0x00 |
| exFAT BootImagePackage | 0x00 | 0x01 |
| DeliveryInfo exFAT capability | Suppressed | 1 |

The separate HasAttributes implementation remains unchanged.

### Firmware Content Filename Compatibility

A runtime content lookup failure was identified when firmware
metadata files retained the .cnmt.nca filename suffix instead
of the normalized .nca suffix expected by the runtime.

The observed result was:

    Result: 0x202
    Horizon error: 2002-0001
    Description: PathNotFound

The original upstream firmware preparation tool already
supports filename normalization. This issue was encountered
when firmware files were extracted directly from an archive.

The workaround was validated without modifying the contents
of the firmware files.

### Runtime Validation

Additional hardware testing confirmed:

- Successful sysmodule initialization.
- Successful NCM, NS and NIM service interception.
- Native exFAT attribute preservation.
- exFAT capability propagation into delivery metadata.
- Successful local network creation.
- Successful receiver discovery.
- Successful update task creation.
- Successful asynchronous send request initiation.

## Hardware Test Environment

| Component | Configuration |
|-----------|---------------|
| Sender | Nintendo Switch 1 |
| Sender firmware | 23.0.1 |
| Atmosphere | 1.12.1 |
| Hekate | 6.5.4 |
| Environment | emuMMC |
| Carrier game | Mario Kart 8 Deluxe |
| Game version | 4.0.1 |
| Receiver | Nintendo Switch 2 |
| Receiver starting firmware | 20.1.5 |
| Target firmware | 23.0.1 |
| Development OS | Arch Linux / Omarchy |
| Build environment | Docker / devkitPro |

## Validation Results

| Test | Result |
|------|--------|
| Firmware integrity | 299/299 PASS |
| Metadata table verification | 150/150 PASS |
| Attribute mask unit tests | 256/256 PASS |
| Metadata filename aliases | 150/150 PASS |
| Runtime exFAT preservation | PASS |
| DeliveryInfo exFAT propagation | PASS |
| Sysmodule initialization | PASS |
| Receiver discovery | PASS |
| Update task creation | PASS |
| General firmware compatibility | Unverified |

Passing individual tests does not guarantee that the
complete firmware installation process is safe.

## Project Structure

    source/              Sysmodule source code
    tools/               Firmware preparation utilities
    extras/              Supporting configuration files
    SD_CARD/             SD card deployment structure
    TECHNICAL_WRITEUP.md Original technical documentation
    CHANGELOG.md         Changes and research milestones

## Building

The project uses the devkitPro AArch64 toolchain, Atmosphere
libraries, and libnx.

The experimental build was compiled in a Docker-based
devkitPro environment.

Refer to the original technical documentation for upstream
build requirements and architecture.

## Known Limitations

- Experimental implementation.
- Limited hardware testing.
- Firmware-specific dependencies.
- Compatibility with other firmware versions is unverified.
- Error 2026-0002 was observed during receiver testing.
- Further installation and cold-boot verification is required
  before declaring general compatibility.
- Rebuilt binaries are not necessarily byte-identical to
  the original distributed binaries.

## Firmware Distribution

This repository does not distribute Nintendo firmware,
game content, encryption keys, or proprietary system files.

Users are responsible for obtaining any required firmware
from legitimate sources.

Nintendo firmware and game content remain the property
of their respective copyright holders.

## Responsible Research

This project is intended for interoperability research,
technical education, preservation, and experimentation
on personally owned hardware.

It is not affiliated with, sponsored by, or endorsed
by Nintendo.

## Disclaimer

WARNING: EXPERIMENTAL SOFTWARE

Firmware modification and installation may result in:

- Permanent hardware bricking.
- Data loss.
- System instability.
- Firmware incompatibility.
- Loss of access to device functionality.
- Warranty or service restrictions.

This software is provided AS IS, without warranty of any kind.

The original author, contributors, and maintainers are not
responsible for damage, data loss, or other consequences
resulting from the use of this project.

Use entirely at your own risk.

## License

Original licensing, copyright notices, and attribution
are retained.

The original GPL-2.0 licensing terms apply to the relevant
components. Refer to the original license files and individual
source headers for component-specific licensing information.

## Final Words

Open research moves forward because people are willing to
share their discoveries, document their failures, and build
upon each other's work.

Once again, thank you to JmpCallPoo for making this research
possible and sharing the original implementation with the
community.

Respect the original creators. Share knowledge. Keep researching.

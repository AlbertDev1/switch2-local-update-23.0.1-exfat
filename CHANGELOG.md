# Changelog

## v1.0.0-exfat

### Fixed
- Preserved native BootImagePackage exFAT attributes.
- Validated exFAT capability propagation into SystemDeliveryInfo.
- Identified metadata NCA filename incompatibility.
- Resolved the filename mismatch responsible for error 0x202.

### Hardware testing
- Sender: Nintendo Switch 1
- Sender firmware: 23.0.1
- Atmosphere: 1.12.1
- Hekate: 6.5.4
- Mario Kart 8 Deluxe: 4.0.1
- Receiver: Nintendo Switch 2
- Receiver starting firmware: 20.1.5
- Target firmware: 23.0.1

### Validation
- Firmware integrity: 299/299
- Metadata entries: 150/150
- Attribute mask tests: 256/256
- Metadata filename aliases: 150/150
- Runtime exFAT attribute preservation: PASS
- DeliveryInfo exFAT propagation: PASS

### Known issues
- Error 2026-0002 was observed during hardware testing.
- Receiver compatibility across other firmware versions is unverified.
- Experimental software. Use at your own risk.

### Credits
Based on the original Switch 2 local update project.
Original authorship and licensing are retained.

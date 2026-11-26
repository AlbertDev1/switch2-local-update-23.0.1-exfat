# Updating a Switch 2 offline from a Switch 1 (Local Version Matching)

> [!CAUTION]
> **DISCLAIMER / NO WARRANTY:** This package is experimental software provided **"AS IS"**, without warranty of any kind. System firmware updates and CFW modifications always carry a risk: **the author is NOT responsible for any brick, data loss, ban, or damage to your Switch 1 or Switch 2.** You are solely responsible for everything you do with your own hardware — use at your own risk.

Update an **air-gapped, stock Nintendo Switch 2** to system firmware **23.0.1** using nothing but a
**hacked Switch 1** and Nintendo's own built-in *"Match version with local users"* feature.
No Internet connection, no CFW or exploit on the Switch 2.

> **Status:** validated end to end — Switch 2 on **22.5.0 → 23.0.1** (full 771.5 MB transfer,
> installed successfully) from a Switch 1 on **HOS 23.0.1 + Atmosphère 1.12.1**.

---

## 1. How it works (short version)

Local Version Matching lets two consoles in the same room share game updates and system updates.
The console with the higher version hosts an ad-hoc Wi-Fi group and streams the update.
A Switch 1 normally cannot host a *Switch 2* firmware, so this package makes the Switch 1 look like
a legitimate Switch 2 update source:

| Component | Where | What it does |
|---|---|---|
| `match-version-unlocker` (sysmodule `42000000162C63DA`) | Switch 1 | Intercepts `ncm`/`ns`/`nim`: announces the Switch 2 SystemUpdate `0x0400000000000816` v`0x5C01753C`, serves the 150 content-meta entries and the 299 NCAs from the SD card, fixes the delivery variation id (9 → 11). |
| `ldn_debug_logger` (sysmodule `42000000DE306106`) | Switch 1 | Intercepts `ldn` and advertises the Switch 1 as a *Switch 2 / Ounce* platform with version `0x5C01753C`. (Name is historical: it logs verbosely to `sdmc:/ldn_debug.log`.) |
| 4 ExeFS IPS patches | Switch 1 | Remove the one-shot delivery lock in `ns`, avoid two `qlaunch` aborts, avoid a `fs::MountContent` on Switch 2 (`NHR0`) NCAs in `nim`, and platform spoof in `ldn`. |
| `system_settings.ini` entries | Switch 1 | Delivery protocol / firmware group settings for the Switch 2 update. |
| Firmware NCAs in `/NX2_23_0_1/` | Switch 1 SD | The Switch 2 firmware itself (**not included**, see step 1). |

For the full reverse-engineering analysis and porting guide for future firmware versions, see `TECHNICAL_WRITEUP.md`.

## 2. Requirements

- **Switch 1** running **HOS 23.0.1** with **Atmosphère 1.12.1** (CFW already working).
  The IPS patches are tied to the exact module builds of 23.0.1 (see §8) — other firmware
  versions need new patches. If you use custom Home Menu (`qlaunch`) themes, disable them temporarily before running the update.
- **Switch 2** stock, on a firmware lower than 23.0.1 without exFAT driver pre-installed (tested: 22.5.0 → 23.0.1). Keep it offline.
- **Mario Kart 8 Deluxe** (the "carrier" game that opens the local-communication session):
  - on the **Switch 1**: the game installed **and updated to version 4.0.1**;
  - on the **Switch 2**: the game must also be **available** — simply insert the game
    cartridge (no install or update needed).
- Both consoles charged (or plugged in to AC power) and within Wi-Fi range.
  > [!WARNING]
  > **IMPORTANT — Prevent accidental Nintendo connections:** Before turning **Airplane Mode OFF** (local communication requires Wi-Fi to be enabled), **delete ALL saved Wi-Fi networks/APs on the Switch 2** (*System Settings → Internet → Internet Settings → select each network → Clear Settings*) so it cannot connect to the Internet or Nintendo's servers when Airplane Mode is turned off. On the Switch 1, keep Nintendo servers blocked as usual (DNS MITM / Exosphere).
- A PC (Windows, macOS, or Linux) with **Python 3.8+** (no third-party libraries needed) for step 1.
- The **Switch 2 firmware 23.0.1 NCA files** (299 files, 771.5 MB) — see step 1.
- ~800 MB free on the Switch 1 SD card.

## 3. Step 1 — Prepare the firmware (not included)

The Switch 2 firmware is Nintendo's copyrighted data and is **not** distributed here. You must
obtain the 23.0.1 Switch 2 system update files yourself (your own legitimate source).

You need the **149 content NCAs** and the **150 meta NCAs** (`*.cnmt.nca`) of SystemUpdate
`0x0400000000000816` v`0x5C01753C` (`0x5C01753C` = 23.0.1). Put them in any folder, then
run from your PC (Windows, macOS, or Linux — uses only the Python standard library):

```bash
python3 tools/prepare_firmware.py /path/to/your/dump NX2_23_0_1
```

The script verifies all 299 files without needing any key (a NCA's file name is the first 16 bytes of its
SHA-256), and writes them as `NX2_23_0_1/<id>.nca` (meta NCAs lose the `.cnmt` part). It stops
on any missing/corrupted file. Use `--check-only` to verify without copying.

## 4. Step 2 — Install on the Switch 1 SD card

Copy the contents of the `SD_CARD/` folder of this package **to the root of the SD card**
(merge folders, it only adds files):

```
atmosphere/contents/42000000162C63DA/{exefs.nsp, mitm.lst, flags/boot2.flag}   (match-version-unlocker)
atmosphere/contents/42000000DE306106/{exefs.nsp, mitm.lst, flags/boot2.flag}   (ldn_debug_logger)
atmosphere/exefs_patches/send_sysupdate/…3B048F33….ips                         (ns)
atmosphere/exefs_patches/qlaunch_sysupdate/…4FB7D6F3….ips                      (qlaunch)
atmosphere/exefs_patches/nim_sysupdate/…7A0ECCE9….ips                          (nim)
atmosphere/exefs_patches/ldn_platform/…8CD36A9A….ips                           (ldn)
config/ldn_debug_logger/dlog.ini                                               (ldn MITM enabled by default)
```

*(Note: `ldn_debug_logger` enables all MITM hooks by default in code (`dlog_config.cpp`: `enable_mitm = true`, `disable_* = false`), so `dlog.ini` only lists overrides; `[ldn_u] enable_mitm=1` is already enabled out of the box.)*

Then:

1. Copy the `NX2_23_0_1/` folder from step 1 to the SD root → `sdmc:/NX2_23_0_1/` (299 files).
2. Open `extras/system_settings_to_merge.ini` and **merge** its sections into
   `sdmc:/atmosphere/config/system_settings.ini` (create the file if missing; do not
   duplicate a section header that already exists — add the keys to the existing section).
3. Reboot the Switch 1 (launch Atmosphère again) so the sysmodules and patches are loaded.

Optional but handy: an FTP server on the Switch 1 (e.g. sys-ftpd-light) to read `mvu.log` /
`ldn_debug.log` from the SD card while testing.

**Sanity check after boot:** `sdmc:/mvu.log` appears and contains
`ns:su: MITM server registered` and `nim: MITM server registered`. A line
`flagset: mount … failed … (busy? ips couvre)` is harmless.

## 5. Step 3 — Prepare the Switch 2

1. **Delete every saved Wi-Fi network on the Switch 2 first** (*System Settings → Internet → Internet Settings → select each saved AP → Clear Settings*), then turn **Airplane Mode OFF** so local ad-hoc Wi-Fi (`ldn`) can run safely without connecting to Nintendo.
2. If a system update was ever started/downloaded on the Switch 2 (even a cancelled one), a
   pending `SystemUpdateTask` stays in its save data and makes every local update fail
   immediately with **2211-0525**. A normal reboot does **not** clear it. Clear it once:
   - Power the Switch 2 off.
   - Boot it into **Maintenance Mode** (hold **Vol+** and **Vol−** while pressing power; check Nintendo's documentation if your model differs).
   - **Do not reset or initialise anything** — simply exit/power off. No data is lost.

You can repeat the Maintenance Mode step any time `2211-0525` comes back immediately on the Switch 2.

## 6. Step 4 — Run the update

**The Switch 1 must create the group and the Switch 2 must join it** (the Switch 1 hosts
the files). Keep both consoles **plugged in or well charged**, with **all Wi-Fi APs removed on the Switch 2** and **Airplane Mode OFF**.

1. On the **Switch 1**: start the *local update* flow on **Mario Kart 8 Deluxe** (4.0.1)
   (Home → highlight the game → **+** → *Software Updates* → *Via local communication*) and create the
   group so it waits for the other console.
2. On the **Switch 2** (with the Mario Kart 8 Deluxe cartridge inserted): do the same and join the Switch 1's group. A popup *"A system update is
   available"* appears; accept it and press **Start communication**, then **Update the console**.
3. **Be patient — the progress bar stays at 0 % for about 65–70 seconds.** The Switch 2 first
   downloads and checks the 150 meta NCAs one by one (about 0.45 s each) and only then knows
   the total size. Do **not** cancel. After that the 771.5 MB stream starts with a moving bar.
4. Let it finish; the Switch 2 installs the update and reboots on 23.0.1.
5. **Recommended after updating:** follow **§9 Uninstall** on your Switch 1 to restore normal local multiplayer behavior and prevent `sdmc:/mvu.log` / `sdmc:/ldn_debug.log` from growing indefinitely.

## 7. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Switch 1 does not boot / freezes on the logo | Remove `atmosphere/contents/42000000162C63DA` and `…DE306106`, reboot, re-copy. Check you are on HOS 23.0.1 + AMS 1.12.1. |
| No *"system update available"* popup on the Switch 2 | Mario Kart 8 Deluxe not on 4.0.1 on the Switch 1, or cartridge not inserted in the Switch 2; otherwise `system_settings.ini` not merged (or typo), IPS patches not loaded, or the Switch 1 did not create the group. Check `mvu.log` for `[SPOOFED SystemUpdate]`. |
| **2211-0525** on the Switch 2 right after *Start communication*, ~1 s, no activity in `mvu.log` | Pending update task on the Switch 2 → Maintenance Mode (§5). |
| **2211-0525** on the Switch 1 and **2211-0517** on the Switch 2 | Serving problem on the Switch 1: `nim_sysupdate` patch missing/not loaded, or `/NX2_23_0_1/` incomplete. Check `mvu.log` for `nim::AsyncResult::Get -> rc=…`. |
| Both consoles seem idle after pressing *Update* | Normal for ~70 s (§6, step 3). Check that `mvu.log` shows `ListContentInfo: [NX2]` lines advancing. |
| **2211-0520** | Different issue (a game with Switch 2 specific content); `match-version-unlocker` already clears the offending bit. |
| `mvu.log` / `ldn_debug.log` very large | Verbose logs written at the SD root; delete them any time. |

To diagnose: copy `sdmc:/mvu.log` (and `sdmc:/ldn_debug.log`) from the SD card and look at the
last `[… ms]` session (the counter restarts at each boot).

## 8. Compatibility

- Patches are matched by **module build id** — Atmosphère silently ignores a patch whose name
  does not match the module's build id:

| Module | Program id | Build id (patch file name) | Patch |
|---|---|---|---|
| ns | `010000000000001F` | `3B048F336B810609A90F0D58605A340BA520A983` | `send_sysupdate` — NOP of the one-shot delivery lock check |
| qlaunch | `0100000000001000` | `4FB7D6F3B4BD12BD3B1AD37C75C3C2F89173171F` | `qlaunch_sysupdate` — removes 10 aborts / state asserts in the LDN update flow |
| nim | `0100000000000025` | `7A0ECCE9375DC38BA907EE7F9F4BD6046075AF96` | `nim_sysupdate` — skips `fs::MountContent` on `NHR0` NCAs (`bl` → `b`) |
| ldn | `0100000000000018` | `8CD36A9A6BB9EA92F8F7F83E5E49D251499BDDB3` | `ldn_platform` — forces the node `platform` byte (offset `0x2D`) to `1` (Switch 2 / Ounce) inside `ldn` itself |

- The embedded content table (150 `ContentMetaKey`s) and the manifest are specific to **Switch 2
  firmware 23.0.1**. Another firmware needs the table regenerated from its `.cnmt` files.
- This package forces **`variation_id = 11` (non-exFAT Switch 2)**: if a Switch 2 already has the exFAT driver update installed, delivery may be rejected or require switching to `variation_id = 12` (which also requires the 7 additional exFAT NCAs not present in the 299-file non-exFAT dump).
- Not tested: other Atmosphère / HOS versions, Switch 2 consoles other than a stock one.

## 9. Uninstall

Delete `atmosphere/contents/42000000162C63DA`, `atmosphere/contents/42000000DE306106`, the four
`atmosphere/exefs_patches/{send_sysupdate,qlaunch_sysupdate,nim_sysupdate,ldn_platform}` folders,
`config/ldn_debug_logger`, `/NX2_23_0_1`, and the keys you added to `system_settings.ini`.

## 10. Building from source

`source/match-version-unlocker` and `source/ldn_debug_logger` contain the sources.
Requirements: devkitPro (devkitA64), the `Atmosphere-libs` submodule at commit
`cf48d23375cf36742876f4b014971b2ebabc6dc8` and `libnx` at `b156b4774fb2df7b93402169509266da7547fc4c`:

```bash
cd source/match-version-unlocker
git clone https://github.com/Atmosphere-NX/Atmosphere-libs && git -C Atmosphere-libs checkout cf48d23375cf36742876f4b014971b2ebabc6dc8
git clone https://github.com/switchbrew/libnx           && git -C libnx checkout b156b4774fb2df7b93402169509266da7547fc4c
make            # artefact: dist/atmosphere/contents/42000000162C63DA/exefs.nsp

# Then build ldn_debug_logger sharing Atmosphere-libs and libnx:
cd ../ldn_debug_logger
ln -s ../match-version-unlocker/Atmosphere-libs Atmosphere-libs
make            # artefact: dist/atmosphere/contents/42000000DE306106/exefs.nsp
```

## 11. Legal, licences, credits

- **No Nintendo firmware NCAs or cryptographic keys are included.** You are responsible for sourcing the firmware legitimately.
  Use only on consoles you own.
- **No warranty / no liability:** Provided **"AS IS"**. The author is **not responsible for any brick**, boot failure, ban, data loss, or hardware damage on either console — use entirely at your own risk.
- Sources are **GPL-2.0** (see `LICENSE`).
- `match-version-unlocker` is based on [Knight1/match-version-unlocker](https://github.com/Knight1/match-version-unlocker);
  `ldn_debug_logger` on [TSRBerry/ldn_debug_logger](https://codeberg.org/TSRBerry/ldn_debug_logger)
  (itself based on ldn_mitm, MissionControl, Atmosphère). Built on
  [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere) and
  [libnx](https://github.com/switchbrew/libnx); protocol knowledge from
  [SwitchBrew](https://switchbrew.org).

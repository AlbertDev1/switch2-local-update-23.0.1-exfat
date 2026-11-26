# Offline Switch 2 System Update via Switch 1 CFW — Technical Write-Up

**Target validated:** Stock air-gapped Nintendo Switch 2 (**HOS 22.5.0 → 23.0.1**, 299 NCAs, 771.5 MB) updated from a CFW Nintendo Switch 1 (**HOS 23.0.1 + Atmosphère 1.12.1**) using Nintendo's native **Local Version Matching** (`SendSystemUpdate` / `ReceiveSystemUpdate`).

---

## 1. Security Boundary & Design Principle

Nintendo's *Local Version Matching* allows two consoles on the same local ad-hoc Wi-Fi network (`ldn`) to share system updates: whichever console holds a higher `RequiredSystemVersion` acts as an AP host and serves raw firmware archives over TCP port `55556`.

Crucially, **the receiving Switch 2 validates every single NCA cryptographically on the fly**:
- Each NCA filename `<ContentId>.nca` is the first 16 bytes of the file's SHA-256 digest.
- During delivery, the Switch 2 fetches the 56-byte `PackagedContentInfo` (`SHA-256[32] + ContentInfo[24]`) for the `SystemUpdate` meta and for each sub-title, then streams the raw `.cnmt.nca` and content `.nca` bytes and verifies their hashes, NCA signatures, and ticketless system headers natively.
- **No modified, re-signed, or padded NCA can ever pass Switch 2 verification.** Every byte served from `sdmc:/NX2_23_0_1/` must remain 100% original.

The goal is therefore purely to bridge the protocol, platform, variation, and format incompatibilities on the **Switch 1 host side** so that an unmodified Switch 1 OS stack (`ldn` + `qlaunch` + `ns` + `nim` + `ncm`) presents itself as a valid Switch 2 (`Ounce`) update host and streams the intact Switch 2 NCAs.

---

## 2. End-to-End Protocol Flow

```text
  Switch 1 (CFW 23.0.1, AP Host)                         Switch 2 (Stock 22.5.0, Station)
  ==============================                         ================================
  [1] Launch carrier game (MK8D 4.0.1) local update -> open ldn:s AP
      - MetaDb[3]::GetRequiredSystemVersion() -> 0x5C01753C (23.0.1)
      - MetaDb[3]::GetAttributes()            -> clear ProperProgramExists (bit 3)
      - ldn_platform IPS + ldn_debug_logger   -> NodeInfo.platform (+0x2D) = 1 (Ounce)
      - AdvertiseData Tag 2                   -> preserve byte[1]<=2 in lobby, inject 0x5C01753C

                                              <-------   [2] Scan / Join AP & Compare SystemDeliveryInfo[256]
                                                         - Validate peer_info in ns (0x140d58):
                                                           platform==1, fw_group mask, exfat, region, HMAC
                                                         - Prompt: "A system update is available"

  [3] qlaunch -> ns:su::RequestSendSystemUpdate (cmd 17)
      - send_sysupdate IPS bypasses flagset bit-3 lock (0x13CA60 -> NOP)
      - SystemDeliveryInfo signed natively with ns HMAC key
      - ns calls service "nim" cmd 76 CreateSendSystemUpdateTask (mvu rewrites var 9 -> 11)
      - ns calls service "nim" cmd 77 RequestSendSystemUpdate -> listen TCP :55556

                                              <-------   [4] TCP Connect -> 169.254.x.1:55556
                                                         (requires Switch 2 0x8000000000000070 clear!)

  [5] Phase 1 (~65-70s, UI progress shows tot=0):
      nim TCP cmd 1 (GetMetaPackagedContentInfo) x 150 (.cnmt.nca meta enumeration)
      - nim_sysupdate IPS (0x11A5F8 -> b +0x180) skips fs::MountContent on NHR0 NCA (avoids 0xbee99002)
      - MetaDb[3]::ListContentInfo() returns NX2 static table entry -> ContentInfo(type=Meta)
      - Storage[3]::GetSize + ReadContentIdFile streams raw .cnmt.nca from sdmc:/NX2_23_0_1/

  [6] Phase 2 (progress bar advances, ~771.5 MB):
      nim TCP cmd 2 (GetContent) x 149 content NCAs
      - Storage[3]::GetSize + ReadContentIdFile streams raw .nca bytes -> Switch 2 verifies SHA-256
      - Switch 2 commits update & reboots into 23.0.1
```

---

## 3. Component Deep Dive

### 3.1. `system_settings.ini` & `SystemDeliveryInfo` Validation

Reverse-engineering `ns` (HOS 23.0.1) around `nsSelectLatestSystemDeliveryInfo` (`0x140ac4`) and its validator (`0x140d58`) revealed the binary layout of the 256-byte `NsSystemDeliveryInfo` exchanged over LDN:

| Offset | Type | Field | Required value for NX2 23.0.1 |
|---|---|---|---|
| `0x00` | `u32` | `system_delivery_protocol_version` | `1` |
| `0x04` | `u32` | `application_delivery_protocol_version` | `1` |
| `0x08` | `u8` | `has_exfat` | `0` (non-exFAT Switch 2) |
| `0x0C` | `u32` | `system_update_version` | `0x5C01753C` (23.0.1) |
| `0x10` | `u64` | `old_system_update_id` | `0x0400000000000816` |
| `0x18` | `u8` | `firmware_variation_id` | `9` (Switch 1) → rewritten to `11` for `nim` |
| `0x19` | `u8` | `updatable_firmware_group_id` | `8` |
| `0x1A` | `u8` | `platform_region` | `0` (Global) |
| `0x1B` | `u8` | `system_delivery_info_platform` | `1` (`Ounce` / Switch 2) |
| `0x1C` | `u8` | `present` | `1` |
| `0x20` | `u64` | `system_delivery_info_id` | `0x0400000000000816` |
| `0xE0` | `u8[32]` | HMAC-SHA256 over `0x00..0xDF` | Computed and verified natively by `ns` |

Inside the validator (`0x140d58`), five checks must pass on the receiving Switch 2 (otherwise `SelectLatestSystemDeliveryInfo` returns `-1` → **`2211-0518`**):
1. **Version check:** `peer.system_update_version >= carrier_required_version && peer.system_update_version >= client_os_version`.
2. **exFAT check:** if `client.has_exfat == 1`, `peer.has_exfat` must also be `1`.
3. **Platform match:** `peer.system_delivery_info_platform == client.system_delivery_info_platform` (`1`).
4. **Hardware variation bitmask (`0x140da4`):** the client computes `(updatable_firmware_group_string[peer.group_id] >> client.variation_id) & 1`.
   - Default `group_id = 0` yields mask `1` (only variation `0`).
   - Setting `updatable_firmware_group_id = u8!0x8` selects index `8` of `"1,3,4,8,23,127,2047,2048,6144"`, which is **`6144` (`(1 << 11) | (1 << 12)`)** — matching Switch 2 variations `11` and `12`!
5. **String typing pitfall (`str!` vs `u64!`):** `ns` (`0x8e65c`, `0x8fcf0`, `0x8ffb4`) reads `system_delivery_info_id`, `old_system_update_id`, and `acceptable_system_update_ids_string` with `GetString` and parses them using `strtoull(buf, NULL, 16)`. Using `u64!` stores raw little-endian binary bytes, causing `strtoull` to return `0`. **`str!` is mandatory.**

### 3.2. `ldn_debug_logger` & `ldn:s` Protocol Quirks

`ldn_debug_logger` (historically named from its logging heritage) serves two critical MITM roles on `ldn:u` and `ldn:s`:
- **Node platform spoofing:** Forces `nodes[0].platform = 1` (offset `+0x2D` of `NodeInfo`) in `GetNetworkInfo`, `Scan`, and `ScanPrivate`, complemented by the `ldn_platform` IPS patch inside `ldn` itself.
- **Conditional `AdvertiseData` Tag 2 patching (`qlaunch` `0x422c10`):**
  - `qlaunch` reads the 256-byte `AdvertiseData` buffer: `byte[1]` is the LDN update state (`0x01`=Init, `0x02`=Lobby open, `0x07`=Update ready, `0x08`=Transferring), `byte[7]` mirrors `byte[1]`, and `bytes[8..11]` hold the big-endian host OS version (`0x5C01753C`).
  - **Critical bug avoided:** Unconditionally forcing `byte[1] = 0x07` breaks lobby discovery (the Switch 2 ignores hosts whose `byte[1] != 0x02` during scanning). The MITM always writes `bytes[8..11] = 0x5C01753C`, and only forces `byte[1] = byte[7] = 0x07` when `byte[1] > 2`.
- **HOS 18+ IPC compatibility:**
  - `ldn:s` cmds `400..403` (including `SetOperationMode(u32)` on cmd `402`, whereas `ldn:u` has `Initialize2` with 16 bytes) must be excluded from typed SF handlers and forwarded raw (`ResultShouldForwardToSession()`), otherwise deserialization fails with **`2211-0064`** (`0x3480b`).
  - `GetNetworkInfoLatestUpdate` (cmd `101`) uses a `HipcPointer` buffer in `qlaunch` rather than `HipcMapAlias` (`OutArray`), so forwarding it raw prevents **`2162-0002`**.
  - `CreateClientProcessMonitor` (cmd `1`) and `SetProtocol` (cmd `106`) must be wrapped to prevent **`2011-0301`**.

### 3.3. `match-version-unlocker` (`ncm`, `ns:su`, `nim`)

- **Three independent multi-threaded `ServerManager`s (`MaxSessions = 48`, 2 threads each):**
  - Hosting `ncm` and `ns:su` on one single-threaded `ServerManager` causes a reentrant deadlock (`qlaunch -> mvu(ns:su) -> ns -> mvu(ncm)`).
  - Registering `ns:su` or `nim` before starting `ncm` worker threads causes a boot deadlock because `nim` queries `ncm` before registering `"nim"`. Therefore, `LaunchNcmMitm()` registers `ncm` and spawns its worker threads immediately, while `ns:su` and `nim` register asynchronously inside their own dedicated threads.
  - **Note on `"nim"` vs `"nim:shp"`:** On HOS 23.0.1, `ns`'s `RequestSendSystemUpdate` opens SM service **`"nim"`** (not `"nim:shp"`) to invoke cmds `76` and `77`.
- **`IContentMetaDatabase` (`StorageId::BuiltInSystem [3]`) spoofing:**
  - **Carrier compatibility:** Clears `ProperProgramExists` (bit `3`, fixes `2211-0520`) and bit `0` (`IncludesExFatDriver` on `BootImagePackage`, avoids non-exFAT mismatch) in `GetAttributes` / `HasAttributes`, and forces `GetRequiredSystemVersion` to `0x5C01753C` for applications/patches.
  - **`SystemUpdate` declaration (`0x0400000000000816` / `0x0100000000000816`):** Spoofs `List`, `GetLatestContentMetaKey`, `Has`, `GetSize`, `Get` (patching the first 8 bytes of embedded `SystemUpdateCnmtData` to match the requested ID), `GetRequiredSystemVersion`, `GetAttributes`, and `GetPlatform` (`1` = `Ounce`).
  - **150-entry NX2 table (`include/nx2_content_meta_table.hpp`):** Because Switch 1's real `BuiltInSystem` MetaDB has neither `0x0400000000000816` nor the 149 Switch 2 sub-titles (`0x040000000000xxxx`), `nim`'s `FUN_00119e48` would fail with `0xe05` (`ncm desc 517` → **`2211-0517`**). The embedded `constexpr` table supplies exact `(meta_id, meta_size, content_id, content_size, content_type, id_offset)` tuples for `ListContentInfo` (cmd `4`), `Has` (cmd `8`), `GetContentIdByType` (cmd `3`), and `GetContentInfoByType` (cmd `24`).
- **`IContentStorage` (`StorageId::BuiltInSystem [3]` / `BuiltInUser [4]`):**
  - Serves `Has`, `GetPath` (`@Sdcard:/NX2_23_0_1/<id>.nca`), `GetSizeFromContentId`, `ReadContentIdFile`, and `GetRightsIdFromContentId` (zero rights ID) directly from `sdmc:/NX2_23_0_1/`.
- **`variation_id` rewrite (`9 -> 11`):**
  - Switch 1 `ns` passes `var=9` to `nim::CreateSendSystemUpdateTask`. NX2 23.0.1's `7a596467...cnmt` only defines `var=11` (149 base non-exFAT titles, 100% present in `/NX2_23_0_1/`) and `var=12` (156 titles = 149 + 7 extra exFAT titles). `NimShpMitmService::CreateSendSystemUpdateTask` rewrites `variation_id = 11` for `0x0400000000000816`.
- **`libstratosphere` allocator & `flagset` pitfall:**
  - `fs::SetAllocator` and `fs::SetEnabledAutoAbort(false)` must be called once before `fs::InitializeForSystem()`, otherwise `ams::Malloc` falls back to uninitialized newlib `malloc() -> NULL` and crashes on boot (`0x101`, `FAR=0x20`). `EnsureFsInitialized()` guards single-init across `CheckAndFixSystemSaveData()` and `EnsureSdMounted()`.

### 3.4. The Four ExeFS IPS Patches (HOS 23.0.1)

Remember: in Atmosphère NSO IPS patches, **`IPS_offset = real_VA + 0x100`** (the uncompressed NSO0 header is `0x100` bytes). Always verify the original 4 bytes in the decompressed `.text` before building an IPS file!

1. **`nim_sysupdate` (`nim` `0100000000000025`, build `7A0ECCE9375DC38BA907EE7F9F4BD6046075AF96`) — Core `NHR0` fix:**
   - **Root cause:** When Switch 2 connects to TCP `55556` and sends TCP cmd `1` (`GetMetaPackagedContentInfo`), `nim` (`VA_real 0x11A4C0`, Ghidra `0x11A5A8`) calls `FUN_00034be8` → `FUN_00034468` → `fs::MountContent(ContentType_Meta)` on `7a596467ac9a087c2db8a4373b2c653b.nca` solely to extract its `ContentMetaKey` whitelist. Switch 2 NCAs start at offset `0x200` with plaintext magic **`NHR0`**, whereas Switch 1's `fs` only understands encrypted `NCA3` → **`rc = 0xbee99002`** (`2211-0525` on S1 / `2211-0517` on S2). Meanwhile, downstream functions (`FUN_00119e48` cmd `1` and `FUN_001195f8` cmd `2`) read NCAs via `NCM` `ReadContentIdFile` as raw bytes and never mount any NCA!
   - **Patch (17 bytes):** NSO offset **`0x11A5F8`** (`VA_real 0x11A4F8`): `16 3f fc 97` (`bl 0x2a150` / `OpenContentMetaDatabase`) → **`60 00 00 14`** (`b +0x180`, jumping cleanly to `VA_real 0x11A678` where `x21=this`, `x20=out_56B`, `x19=req_key` are already set up and `bl FUN_00119e48` runs with `StorageId = 3`).
2. **`send_sysupdate` (`ns` `010000000000001F`, build `3B048F336B810609A90F0D58605A340BA520A983`):**
   - **Root cause:** `ns::RequestSendSystemUpdate` mounts confidential system save `0x8000000000002020`, reads `u32` at offset `0` of `/flagset`, and aborts with `0x67c10` (`2016-0830` → `2211-0525`) if bit `3` (`0x8`) is set. Because `ns` often keeps that save locked before `mvu` runs at boot, best-effort clearing isn't always sufficient.
   - **Patch (17 bytes):** NSO offset **`0x13CA60`** (`VA_real 0x13C960`): `20 08 00 36` (`tbz w0, #0, ...`) → **`1f 20 03 d5`** (`nop`).
3. **`ldn_platform` (`ldn` `0100000000000018`, build `8CD36A9A6BB9EA92F8F7F83E5E49D251499BDDB3`):**
   - Forces `platform = 1` (`strb wX, [.., #45]`) at NSO offsets `0x5647c` (branch to `0x56574`), `0x56674` (`mov w9, #1; strb w9, [sp, #45]; b 0x56380`), and `0x5e100` (`mov w1, #1; strb w1, [x0, #45]; ret`).
4. **`qlaunch_sysupdate` (`qlaunch` `0100000000001000`, build `4FB7D6F3B4BD12BD3B1AD37C75C3C2F89173171F`):**
   - 10 IPS32 records: NOPs the 3 `ResultNodeNotFound` (`0x1770d3`) cleanup abort branches (`0x40eba0`, `0x40f240`, `0x40f6c0`) in `LdnStation_CleanupOnJoinFail` (`2168-0006` / `2128-0100`), plus AP sender role / state-machine patches (`0x411ff8`, `0x4143a8`, `0x419fa4`, `0x419fbc`, `0x41a10c`, `0x41a188`, `0x41a198`).

### 3.5. Switch 2 Client-Side Gotchas

1. **Stuck `SystemUpdateTask` in Switch 2 save `0x8000000000000070`:**
   - Switch 2's `ns` checks `ListSystemUpdateTask != 0` at the very start of `ReceiveSystemUpdate` (`FUN_0013fe68`). If any previous update task was left pending in `0x8000000000000070`, it immediately aborts with `0x6e010` (`2016-0880`) → **`2211-0525` in ~1.0 s with zero TCP packets sent**. Normal reboots do **not** purge it; booting into **Maintenance Mode** (without resetting) does.
2. **Stuck `nim` receive-task slot in save `0x8000000000000077`:**
   - If a TCP `connect()` to `55556` fails inside `nim::CreateReceiveSystemUpdateTask` (`0x000b6e88`), `nim` leaks its 1-slot task list until a full Switch 2 reboot (`FUN_000ac088`).
3. **Phase 1 silent wait (~65–70 s):**
   - While Switch 2 fetches the 150 `.cnmt.nca` files one by one (~440 ms/title), `GetSendSystemUpdateProgress` reports `cur = ~575 KB, tot = 0`, so the UI progress bar stays at **0%**. Once the last `.cnmt.nca` is parsed, `tot` resolves to ~771.5 MB and the bar moves normally.

---

## 4. Porting Guide (Future NX2 Firmwares & Future Switch 1 HOS)

### 4.1. Updating for a Newer Switch 2 Firmware (Same Switch 1 HOS 23.0.1)

Assuming Switch 1 stays on HOS 23.0.1 (all 4 IPS patches remain valid):
1. Obtain the new NX2 firmware dump (`.nca` + `.cnmt.nca`). Verify every file by checking `sha256(file)[:16] == <ContentId>`. Rename `<id>.cnmt.nca` → `<id>.nca` for `sdmc:/NX2_.../`.
2. Extract the `.cnmt` files from the `SystemUpdate` meta NCA (`0x0400000000000816`) and every sub-title `.cnmt.nca`.
3. Update C++ artifacts in `source/match-version-unlocker/`:
   - Update `SystemUpdateCnmtData` / `SystemUpdateCnmtSize` in `include/system_update_cnmt.hpp` with the raw `.cnmt` bytes of the new `0x0400000000000816` meta.
   - Regenerate `include/nx2_content_meta_table.hpp` (all `Nx2ContentMetaEntry` items from each `.cnmt` + actual on-disk NCA sizes).
   - Update `kSystemUpdateMetaNcaId`, `kSystemUpdateMetaNcaSize`, and all occurrences of version `0x5C01753C` in `ncm_mitm.cpp` and `ldn_userlocalcommunication.cpp` (big-endian `bytes[8..11]`!).
   - Check the new `SystemUpdate` `.cnmt` variation IDs: verify which `variation_id` matches the non-exFAT base set vs exFAT set, and update the `CreateSendSystemUpdateTask` rewrite (`9 -> <non_exfat_var>`) and `system_settings.ini` `updatable_firmware_group_id` mask if Nintendo adds new variation IDs above `12`.
4. Regenerate `tools/firmware_manifest.json` and rebuild both sysmodules (`make`).

### 4.2. Updating the 4 ExeFS IPS Patches for a New Switch 1 HOS Version

Decompress each target module's `main` NSO0 (header bytes `0x40..0x54` = 20-byte `build_id` for the `.ips` filename). **Important:** IPS offset = `VA_real + 0x100`. Verify the original instruction bytes at `file_off` before emitting any patch.

| Module | How to re-locate the patch site in Ghidra |
|---|---|
| `ns` (`send_sysupdate`) | Search strings for `"flagset"` (`0x74657367616c66`), trace the helper returning `(flags & 8) == 0` into `RequestSendSystemUpdate`, and NOP the `tbz w0, #0, ...` immediately following it (which branches to `0x67c10`). |
| `nim` (`nim_sysupdate`) | Find the TCP `55556` server loop (`"GetMetaPackagedContentInfo"` / `0x38`-byte response), locate the virtual method calling `OpenContentMetaDatabase(3)` → `GetContentInfoByType` → `GetPath` → `.cnmt` loader → whitelist loop → tail call to `GetMetaPackagedContentInfo` helper (`MetaDb::ListContentInfo` + SHA-256). Branch directly from before the first `OpenContentMetaDatabase(3)` to the register setup right before that helper (`mov x3, #3; bl ...`). |
| `qlaunch` (`qlaunch_sysupdate`) | Search for the abort wrapper calling `svcBreak` after `LdnStation` MAC lookup (`ResultNodeNotFound` `0x1770d3`) and NOP the `b.ne` abort branches; trace `CheckSystemUpdateAvailable` (`[1]` / `[8..11]` of `AdvertiseData`) for the state/role sites. |
| `ldn` (`ldn_platform`) | Locate writes to `NodeInfo + 0x2D` (`#45` decimal) for host/node creation (`strb wX, [..., #45]`) and patch to write constant `1`. |

---

## 5. Error Code & Diagnostic Reference

| Error / Hex | Module | Root Cause | Resolution |
|---|---|---|---|
| **`2011-0301`** | `ldn` | Missing HOS 18+ cmds `1` / `106` | Wrapped in `ldn_debug_logger` |
| **`2211-0064`** | `sf` / `ldn` | `ldn:s` cmd `402` arg size mismatch | Forwarded cmds `400..403` raw |
| **`2162-0002`** | `qlaunch` | `ldn` cmd `101` HipcPointer vs MapAlias | Forwarded cmd `101` raw |
| **`2168-0006` / `2128-0100`** | `qlaunch` / `am` | Cleanup abort on `ResultNodeNotFound` | `qlaunch_sysupdate` IPS + Switch 1 creates group |
| **`2001-0103`** | `ns` | HIPC session exhaustion (`MaxSessions`) | Split `ServerManager`s (`MaxSessions = 48`) |
| **`2211-0518`** | `ns` (S2) | `SystemDeliveryInfo` rejected (`grp`, `str!`, `exfat`) | `updatable_firmware_group_id = u8!0x8` + `str!` keys |
| **`2211-0520`** | `ns` (S2) | `ProperProgramExists` bit 3 set | Cleared bit 3 in `MetaDb::GetAttributes` |
| **`2211-0525`** (pre-TCP, ~1 s) | `ns` (S2) | Pending `SystemUpdateTask` in S2 save `0x70` | **Maintenance Mode** on Switch 2 (no reset) |
| **`2211-0525`** (S1 lock) | `ns` (S1) | `flagset` bit 3 in save `0x2020` (`0x67c10`) | `send_sysupdate` IPS (`0x13CA60 -> nop`) |
| **`0xbee99002`** | `fs` / `nim` (S1) | `fs::MountContent` fails on `NHR0` Switch 2 NCA | `nim_sysupdate` IPS (`0x11A5F8 -> b +0x180`) |
| **`2211-0517`** | `ncm` / `nim` | Missing `GetContentInfoByType` / `ListContentInfo` (`0xe05`) | Spoofed `MetaDb[3]` + 150-entry NX2 table |
| **`0x272489` / `0x2a5889`** | `nim` | Normal cancellation codes (peer left / user cancel) | Wait ~70 s for Phase 1 (`tot=0`) to finish |

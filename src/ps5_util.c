#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <ps5/kernel.h>

#define KI_PID_OFFSET 72
#define KI_TDNAME_OFFSET 447

// SDK struct
typedef struct notify_request {
  char useless1[45];
  char message[3075];
} notify_request_t;

// SDK Function
int sceKernelSendNotificationRequest(int device, notify_request_t *request, size_t size, int unused);

static volatile int notify_enabled = 0;
static volatile int log_enabled = 0;

void log_enable(int enable) { log_enabled = enable; }

void log_printf(const char *fmt, ...) {
  va_list args;

  if (!log_enabled) {
    return;
  }

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  printf("\n");
}

void notify_enable(int enable) { notify_enabled = enable; }

void notify(const char *fmt, ...) {
  notify_request_t r;
  va_list args;

  if (!notify_enabled) {
    return;
  }

  memset(&r, 0, sizeof(r));

  va_start(args, fmt);
  vsnprintf(r.message, sizeof(r.message), fmt, args);
  vprintf(fmt, args);
  va_end(args);

  printf("\n");
  sceKernelSendNotificationRequest(0, &r, sizeof(r), 0);
}

void rename_service_thread(const char *name) { syscall(SYS_thr_set_name, -1, name); }

static int find_pid(const char *name) {
  int mib[4] = {1, 14, 8, 0};
  pid_t mypid = getpid();
  pid_t pid = -1;
  size_t buf_size;
  uint8_t *buf;

  if (sysctl(mib, 4, NULL, &buf_size, NULL, 0) != 0) {
    return -1;
  }

  buf = malloc(buf_size);
  if (buf == NULL) {
    return -1;
  }

  if (sysctl(mib, 4, buf, &buf_size, NULL, 0) != 0) {
    free(buf);
    return -1;
  }

  for (uint8_t *ptr = buf; ptr < (buf + buf_size);) {
    int ki_structsize = *(int *)ptr;
    pid_t ki_pid = *(pid_t *)&ptr[KI_PID_OFFSET];
    char *ki_tdname = (char *)&ptr[KI_TDNAME_OFFSET];

    ptr += ki_structsize;
    if (!strcmp(name, ki_tdname) && ki_pid != mypid) {
      pid = ki_pid;
    }
  }

  free(buf);
  return pid;
}

int terminate_instances(const char *name) {
  pid_t pid;
  while ((pid = find_pid(name)) > 0) {
    log_printf("terminating previous instance pid=%d name=%s\n", pid, name);
    if (kill(pid, SIGKILL) != 0) {
      log_printf("kill(previous instance)");
      return -1;
    }
    sleep(1);
  }
  return 0;
}

void shellui_reload(void) {
  // we don't seem to need it, writing the app.db seems to be enough
}

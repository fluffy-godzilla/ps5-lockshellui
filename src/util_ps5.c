#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <ps5/kernel.h>

// SDK struct
typedef struct notify_request {
  char useless1[45];
  char message[3075];
} notify_request_t;

// SDK Function
int sceKernelSendNotificationRequest(int device, notify_request_t *request, size_t size, int unused);

void notify(const char *fmt, ...) {
  notify_request_t r;
  va_list args;
  memset(&r, 0, sizeof(r));

  va_start(args, fmt);
  vsnprintf(r.message, sizeof(r.message), fmt, args);
  va_end(args);

  sceKernelSendNotificationRequest(0, &r, sizeof(r), 0);
}

void rename_service_thread(const char *name) {
  syscall(SYS_thr_set_name, -1, name);
}

void shellui_reload(void) {
  // TODO: cause the reload
}


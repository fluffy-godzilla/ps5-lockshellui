#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <ps5/kernel.h>
#include <sqlite3.h>

const char *elf_name = "ps5-shellui-lock.elf";

// SDK struct
typedef struct notify_request {
  char useless1[45];
  char message[3075];
} notify_request_t;

// SDK Function
int sceKernelSendNotificationRequest(int device, notify_request_t *request, size_t size, int unused);

static void notify(const char *fmt, ...) {
  notify_request_t r;
  va_list args;
  memset(&r, 0, sizeof(r));

  va_start(args, fmt);
  vsnprintf(r.message, sizeof(r.message), fmt, args);
  va_end(args);

  sceKernelSendNotificationRequest(0, &r, sizeof(r), 0);
}

int main(int argc, char *argv[]) {
  syscall(SYS_thr_set_name, -1, elf_name);

  printf("[LOCK] starting %s...\n", elf_name);
  notify("ps5-shellui-lock started...");

  // TODO: main loop

  printf("[LOCK] stopping %s...\n", elf_name);
  notify("ps5-shellui-lock stopped...");

  sleep(1);
  return 0;
}
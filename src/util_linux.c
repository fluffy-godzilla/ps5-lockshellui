#include <stdio.h>
#include <stdarg.h>

void notify(const char *fmt, ...) {
  va_list args;
  
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
}

void rename_service_thread(const char *name) {
  printf("rename_service_thread\n");
  // noop
}

void shellui_reload(void) {
  printf("shellui_reload\n");
  // noop
}

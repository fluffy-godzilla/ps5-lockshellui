#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
  va_list args;

  if (!notify_enabled) {
    return;
  }

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
}

void rename_service_thread(const char *name) {
  log_printf("rename_service_thread");
  // noop
}

static pid_t find_pid(const char *process_name, pid_t this_pid) {
  DIR *dir;
  struct dirent *entry;
  pid_t pid = -1;

  dir = opendir("/proc");
  if (dir == NULL) {
    log_printf("Failed to open /proc");
    return -1;
  }

  while ((entry = readdir(dir)) != NULL) {
    int is_numeric = 1;
    for (int i = 0; entry->d_name[i] != '\0'; i++) {
      if (!isdigit(entry->d_name[i])) {
        is_numeric = 0;
        break;
      }
    }

    if (is_numeric) {
      char comm_path[256];
      char comm_name[256];
      FILE *comm_file;

      snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
      comm_file = fopen(comm_path, "r");
      if (comm_file) {
        if (fgets(comm_name, sizeof(comm_name), comm_file) != NULL) {
          comm_name[strcspn(comm_name, "\n")] = '\0';

          if (strcmp(comm_name, process_name) == 0) {
            pid = strtol(entry->d_name, NULL, 10);
            if (pid == this_pid) {
              pid = -1;
              continue;
            }

            fclose(comm_file);
            break;
          }
        }
        fclose(comm_file);
      }
    }
  }

  closedir(dir);
  return pid;
}

int terminate_instances(const char *name) {
  pid_t pid, this_pid;

  this_pid = getpid();
  while ((pid = find_pid(name, this_pid)) > 0) {
    log_printf("terminating instance pid=%d name=%s\n", pid, name);
    if (kill(pid, SIGTERM) != 0) {
      log_printf("term(previous instance)");
      return -1;
    }
    sleep(1);
  }
  return 0;
}

void shellui_reload(void) { log_printf("shellui_reload"); }

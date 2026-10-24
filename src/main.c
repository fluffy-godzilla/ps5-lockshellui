#include "appdb.h"
#include "common.h"
#include "config.h"
#include "events.h"
#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static AppConfig config;
static int num_uids = 0;
static UserID uids[MAX_NUM_USER_PROFILES];

#define TIMER_PERIODIC_TICK 10
#define TIMER_CONFIG_DEBOUNCE 20
#define TIMER_APPDB_DEBOUNCE 30

// rethrow the SIGTERM signal for kqueue to catch it
static void on_signal(int signo) {
  log_printf("on_signal: %d", signo);
  raise(SIGTERM);
}

static void init_config() {
  struct stat st;

  // no logs just yet
  if (stat(CONFIG_DIR, &st) != 0) {
    if (mkdir(CONFIG_DIR, 0777) != 0) {
      return;
    }
  }

  if (load_config(CONFIG_PATH, &config) == 0) {
    return;
  }

  log_printf("Failed to load configuration. Setting defaults...");
  memset(&config, 0, sizeof(AppConfig));

  insert_or_update_app(&config.visibility, "NPXS40037", 0);
  insert_or_update_app(&config.visibility, "NPXS40047", 0);
  insert_or_update_app(&config.visibility, "NPXS40056", 0);
  insert_or_update_app(&config.visibility, "NPXS40075", 0);
  insert_or_update_app(&config.visibility, "NPXS40063", 1);
  insert_or_update_app(&config.visibility, "NPXS40071", 1);
  insert_or_update_app(&config.visibility, "NPXS40139", 1);

  insert_or_update_app(&config.sort, "NPXS40071", 10);
  insert_or_update_app(&config.sort, "NPXS40063", 12);
  insert_or_update_app(&config.sort, "NPXS40139", 13);
  insert_or_update_app(&config.sort, "PPSA01650", 15);
  sort_app_list(&config.sort);

  config.general.oneshot = 0;
  config.general.debug = 0;
  config.general.quiet = 1;

  save_config(CONFIG_PATH, &config);
}

static int file_changed(const char *file_path, struct timespec *mtime, off_t *size) {
  struct stat now;

  if (stat(file_path, &now) == 0) {
    if (mtime->tv_sec == 0 && *size == 0) {
      *mtime = now.st_mtim;
      *size = now.st_size;
    } else if (mtime->tv_sec != now.st_mtim.tv_sec || *size != now.st_size) {
      *mtime = now.st_mtim;
      *size = now.st_size;
      return 1;
    }
  }
  return 0;
}

static int validate_and_apply_override(void) {
  if (!db_config_mismatch(&config, num_uids, uids)) {
    log_printf("Database is consistent with our configuration");
    return 0;
  }

  notify("Database is inconsistent with our configuration. Applying override...");
  return apply_config_to_db(&config, num_uids, uids);
}

static int handle_sigterm(int kq, EventContext *ctx, struct kevent *e, struct kevent *chlist, int *nchanges) {
  log_printf("Caught SIGTERM. Cleaning up and exiting...");
  return -1;
}

static int handle_config_update(int kq, EventContext *ctx, struct kevent *e, struct kevent *chlist, int *nchanges) {
  static struct timespec config_mtime = {};
  static off_t config_size = 0;

  if (e->filter == EVFILT_VNODE) {
    if (!file_changed(CONFIG_PATH, &config_mtime, &config_size)) {
      return 0;
    }

    log_printf("config file changed, letting fs settle...");
    extern EventContext config_ctx;

    // debounce the actual change to let the file writes settle
    EV_SET(&chlist[(*nchanges)++], TIMER_CONFIG_DEBOUNCE, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, 0,
           TIMER_COOLDOWN_MS, &config_ctx);
    return 0;
  }

  notify("Configuration file changed, reloading config...");
  if (load_config(CONFIG_PATH, &config) != 0) {
    free_config(&config);
    return -1;
  }

  log_enable(config.general.debug);
  notify_enable(!config.general.quiet);

  notify("Applying override due to configuration file change...");
  apply_config_to_db(&config, num_uids, uids);

  // kill thyself
  if (config.general.oneshot) {
    log_printf("mode changed to oneshot, exiting...");
    raise(SIGTERM);
  }
  return 0;
}

static int handle_appdb_update(int kq, EventContext *ctx, struct kevent *e, struct kevent *chlist, int *nchanges) {
  static struct timespec appdb_mtime = {};
  static off_t appdb_size = 0;

  if (e->filter == EVFILT_VNODE) {
    if (!file_changed(DB_PATH, &appdb_mtime, &appdb_size)) {
      return 0;
    }

    log_printf("%s changed, letting fs settle...", DB_PATH);
    extern EventContext appdb_ctx;

    // debounce the actual change to let the file writes settle
    EV_SET(&chlist[(*nchanges)++], TIMER_APPDB_DEBOUNCE, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, 0,
           TIMER_COOLDOWN_MS, &appdb_ctx);
    return 0;
  }

  log_printf("%s changed, validating...", DB_PATH);
  return validate_and_apply_override();
}

// this exists due to EVFILT_VNODE quirks in libkqueue implementation in linux.
// handle_config_update and handle_file_mod internally track the modtime/size changes in db and config files and
// only act whenever the changes do occur.
static int handle_file_timer_tick(int kq, EventContext *ctx, struct kevent *e, struct kevent *chlist, int *nchanges) {
  // pretend to be a fs change event for the sake of handlers' logic
  e->filter = EVFILT_VNODE;

  int rc1 = handle_config_update(kq, ctx, e, chlist, nchanges);
  int rc2 = handle_appdb_update(kq, ctx, e, chlist, nchanges);
  return rc1 ? rc1 : rc2;
}

EventContext sigterm_ctx = {.id = SIGTERM, .filter = EVFILT_SIGNAL, .handle_event = handle_sigterm};
EventContext timer_ctx = {.id = TIMER_PERIODIC_TICK, .filter = EVFILT_TIMER, .handle_event = handle_file_timer_tick};
EventContext config_ctx = {.id = 0, .handle_event = handle_config_update};
EventContext appdb_ctx = {.id = 0, .handle_event = handle_appdb_update};

// returns 0 upon success (when terminating), anything else as error
static int monitor_events(void) {
  int config_fd = -1;
  int appdb_fd = -1;
  int rc = 1;

  int kq = kqueue();
  if (kq == -1) {
    log_printf("kqueue failed");
    return rc;
  }

  // configure the event loop
  int nchanges = 0;
  struct kevent change_list[MAX_KQUEUE_EVENTS];
  struct kevent event_list[MAX_KQUEUE_EVENTS];

  EV_SET(&change_list[nchanges++], SIGTERM, EVFILT_SIGNAL, EV_ADD | EV_ENABLE, 0, 0, &sigterm_ctx);

#if defined(PS5_PAYLOAD_SDK)
  appdb_fd = open(DB_DIR, O_RDONLY);
  if (appdb_fd == -1) {
    log_printf("Error opening database dir file descriptor");
    goto cleanup;
  }

  config_fd = open(CONFIG_DIR, O_RDONLY);
  if (config_fd == -1) {
    log_printf("Error opening config dir file descriptor");
    goto cleanup;
  }

  appdb_ctx.id = appdb_fd;
  config_ctx.id = config_fd;

  // file-dir-level edge-triggers
  EV_SET(&change_list[nchanges++], appdb_fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR, NOTE_WRITE, 0, &appdb_ctx);
  EV_SET(&change_list[nchanges++], config_fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR, NOTE_WRITE, 0, &config_ctx);
#else
  // timer poll to bypass libkqueue vnode quirks
  EV_SET(&change_list[nchanges++], 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, TIMER_POLL_MS, &timer_ctx);
#endif

  if (kevent(kq, change_list, nchanges, NULL, 0, NULL) == -1) {
    log_printf("Error registering kqueue event");
    rc = 1;
    goto cleanup;
  }
  nchanges = 0;

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  log_printf("Listening for term signal and file events...");
  while (1) {
    int new_events;

    do {
      new_events = kevent(kq, change_list, nchanges, event_list, MAX_KQUEUE_EVENTS, NULL);
    } while (new_events == -1 && errno == EINTR); // Retry if interrupted

    if (new_events == -1) {
      log_printf("kevent loop error: %s (errno: %d)", strerror(errno), errno);
      rc = 1;
      goto cleanup;
    }

    nchanges = 0;

    // loop and process events
    for (int i = 0; i < new_events; i++) {
      struct kevent event = event_list[i];
      EventContext *ctx = (EventContext *)event.udata;

      // log_printf("EVENT: %d:%d:%p %p:%p:%d", event.filter, event.flags, event.udata, ctx,
      //            ctx ? ctx->handle_event : NULL, ctx ? ctx->filter : 0);
      if (ctx && ctx->handle_event && ((!ctx->filter) || (ctx->filter && ctx->filter == event.filter))) {
        // if a handler needs to queue up a modification, it can do that
        // via &change_list[(*nchanges)++]
        int err = ctx->handle_event(kq, ctx, &event, change_list, &nchanges);
        if (err) {
          rc = (err > 0) ? err : 0;
          goto cleanup;
        }
      }
    }
  }

cleanup:
  if (kq == -1)
    close(kq);
  if (config_fd == -1)
    close(config_fd);
  if (appdb_fd == -1)
    close(appdb_fd);
  return rc;
}

int main(int argc, char *argv[]) {
  int rc = 0;

  rename_service_thread(ELF_NAME);
  init_config();
  log_enable(config.general.debug);
  notify_enable(!config.general.quiet);

  notify("Locking ShellUI configuration...");
  notify("Running in '%s' mode", config.general.oneshot ? "set and forget" : "daemon");

  if (!config.general.oneshot && terminate_instances(ELF_NAME) != 0) {
    log_printf("Failed to terminate old instances!");
    rc = 1;
    goto cleanup;
  }

  // get the initial user profile ids
  // TODO: add update of user ids IF they change (should do this when we detect app.db change)
  if (populate_user_ids(&num_uids, uids) != 0) {
    log_printf("Failed to get the user profiles!");
    rc = 1;
    goto cleanup;
  }

  // Enforce initial configuration state on bootup
  if (validate_and_apply_override()) {
    log_printf("Failed to apply override!");
    rc = 1;
    goto cleanup;
  }

  if (!config.general.oneshot) {
    log_printf("[Listening] Actively monitoring for updates to %s and SIGTERM...", DB_PATH);
    rc = monitor_events();
    if (rc) {
      log_printf("Error in monitor_events: %d!", rc);
    }
  }

  notify("Unlocking ShellUI configuration...");

cleanup:
  free_config(&config);
  sleep(1);
  return rc;
}

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sqlite3.h>
#include "util.h"

#define OS_WRITE_COOLDOWN_US 500000

const char *elf_name = "ps5-shellui-lock.elf";

#if defined(PS5_SDK)
const char *DB_PATH = "/system_data/priv/mms/app.db";
#else
const char *DB_PATH = "/tmp/app.db";
#endif

static volatile int ignore_next_write = 0;

// Callback for our validation pre-check query
static int check_callback(void *data, int argc, char **argv, char **azColName)
{
  int *needs_override = (int *)data;
  // If our validation query returns ANY rows, it means something is wrong
  // (e.g., we query for rows where visible != 1 or sortOrder != expected)
  if (argc > 0)
  {
    *needs_override = 1;
  }
  return 0;
}

// Checks if the database is already matching our rules, and if not, applies the override script
int validate_and_enforce_override(const char *db_path)
{
  sqlite3 *db;
  char *err_msg = 0;
  int needs_override = 0;

  // 1. PRE-CHECK PHASE (Read-Only)
  // Open as Read-Only first to avoid fighting the OS for an exclusive write lock if we don't need to.
  int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL);
  if (rc == SQLITE_OK)
  {
    sqlite3_busy_timeout(db, 2000);

    // If this query returns rows, it means the OS reverted our changes.
    // TODO: find at least one user in the app.db and use that table
    const char *validation_query = "SELECT 1 FROM tbl_iconinfo_0350443091 WHERE titleId == 'NPXS40047' AND visible == 1 LIMIT 1;";

    rc = sqlite3_exec(db, validation_query, check_callback, &needs_override, &err_msg);
    if (rc != SQLITE_OK)
    {
      // If the schema changed or table doesn't exist yet, default to attempting the override
      needs_override = 1;
      sqlite3_free(err_msg);
    }
    sqlite3_close(db);
  }
  else
  {
    // If we can't even open read-only, assume we should try the write override
    needs_override = 1;
  }

  // Fallback optimization: If the presence values are already correct, bail out safely!
  if (!needs_override)
  {
    notify("[Optimization] Presence fields are already correct. Skipping write and UI reload.\n");
    return 0;
  }

  // 2. OVERRIDE PHASE (Read-Write)
  notify("[State Drift] Presence modification detected. Opening read-write channel...\n");
  rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
  if (rc != SQLITE_OK)
  {
    notify("[Error] Cannot open database for writing: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  sqlite3_busy_timeout(db, 5000);

  rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", 0, 0, &err_msg);
  if (rc != SQLITE_OK)
  {
    notify("[Lock Failed] Could not lock DB: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return -1;
  }

  const char *sql =
      "UPDATE tbl_iconinfo_0350443091 SET visible = 0 "
      "WHERE titleId IN ('NPXS40037', 'NPXS40047', 'NPXS40056', 'NPXS40075');"
      "UPDATE tbl_iconinfo_0350443091 SET visible = 1 "
      "WHERE titleId IN ('NPXS40063', 'NPXS40071');"
      "UPDATE tbl_iconinfo_0350443091 SET visible = 1 "
      "WHERE titleId IN ('NPXS40139');"
      "UPDATE tbl_contentInfo SET sortPriority = 10 "
      "WHERE titleId == 'NPXS40071';"
      "UPDATE tbl_contentInfo SET sortPriority = 12 "
      "WHERE titleId == 'NPXS40063';"
      "UPDATE tbl_contentInfo SET sortPriority = 13 "
      "WHERE titleId == 'NPXS40139';"
      "UPDATE tbl_contentInfo SET sortPriority = 15 "
      "WHERE titleId == 'PPSA01650';";

  // Block our kqueue loop from responding to our upcoming write action
  ignore_next_write = 1;

  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK)
  {
    notify("[SQL Error] Override failed: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_exec(db, "ROLLBACK;", 0, 0, NULL);
    ignore_next_write = 0;
    // free(sql);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_exec(db, "COMMIT;", 0, 0, &err_msg);
  if (rc != SQLITE_OK)
  {
    notify("[Commit Failed] Save failed: %s\n", err_msg);
    sqlite3_free(err_msg);
    ignore_next_write = 0;
    sqlite3_close(db);
    // free(sql);
    return -1;
  }

  notify("[Success] Custom presence fields successfully restored.\n");
  // free(sql);
  sqlite3_close(db);

  // 3. UI RELOAD PHASE
  // Triggered strictly when changes were physically written out to disk
  notify("[UI Sync] Executing system call to reload PS5 shell interface...\n");
  shellui_reload();
  return 0;
}

int main_loop() {

  int file_fd = open(DB_PATH, O_RDONLY);
  if (file_fd == -1)
  {
    notify("Error opening database file descriptor");
    return 1;
  }

  int kq = kqueue();
  if (kq == -1)
  {
    notify("Error creating kqueue");
    close(file_fd);
    return 2;
  }

  struct kevent change_event;
  struct kevent trigger_event;

  EV_SET(&change_event, file_fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR, NOTE_WRITE | NOTE_DELETE | NOTE_RENAME, 0, NULL);

  if (kevent(kq, &change_event, 1, NULL, 0, NULL) == -1)
  {
    notify("Error registering kqueue event");
    close(file_fd);
    close(kq);
    return 3;
  }

  printf("[Listening] Actively monitoring for PS5 updates to %s...\n", DB_PATH);

  while (1)
  {
    int new_events = kevent(kq, NULL, 0, &trigger_event, 1, NULL);
    if (new_events == -1)
    {
      notify("kevent exception encountered");
      break;
    }
    else if (new_events > 0)
    {
      if (trigger_event.fflags & NOTE_WRITE)
      {

        if (ignore_next_write)
        {
          notify("[Self Loop Blocked] Safely ignoring our own write event.\n");
          ignore_next_write = 0;
          continue;
        }

        notify("\n[OS Trigger] System software modified app.db.\n");
        notify("[Cooldown] Pausing for %dms...\n", OS_WRITE_COOLDOWN_US / 1000);
        usleep(OS_WRITE_COOLDOWN_US);

        // Evaluate if the system software broke our fields, override if necessary, and reload UI
        validate_and_enforce_override(DB_PATH);
      }
      else if (trigger_event.fflags & NOTE_DELETE || trigger_event.fflags & NOTE_RENAME)
      {
        notify("File was replaced or deleted! Need to re-open file_fd and re-register kqueue.\n");
        close(file_fd);
        close(kq);

        // Sleep a tiny bit to let the writing application finish the rename
        usleep(100000);

        // we will try again
        return 0;
      }
      else
      {
        notify("!!!!");
      }
    }
    else
    {
      notify("????");
    }
  }

  close(file_fd);
  close(kq);
  return -1;
}

int main(int argc, char *argv[])
{
  rename_service_thread(elf_name);

  notify("[LOCK] starting %s...\n", elf_name);

  // Enforce initial configuration states on bootup
  validate_and_enforce_override(DB_PATH);

  while(1) {
    if (main_loop()) {
      break;
    }
  }

  notify("[LOCK] stopping %s...\n", elf_name);
  sleep(1);
  return 0;
}

#include "appdb.h"
#include "common.h"
#include "config.h"
#include "util.h"
#include <sqlite3.h>
#include <string.h>

static int apply_priorities(sqlite3 *db, sqlite3_stmt *s, AppList *list) {
  AppNode *current = list->head;
  int rc = 0;
  int updated = 0;

  while (current != NULL) {
    // bind parameters (1-based index)
    // UPDATE tbl_contentInfo SET sortPriority = ? WHERE titleId == ?;
    sqlite3_bind_int(s, 1, current->value);
    sqlite3_bind_text(s, 2, current->app_id, -1, SQLITE_STATIC);

    rc = sqlite3_step(s);
    if (rc == SQLITE_DONE) {
      if (sqlite3_changes(db) > 0) {
        updated++;
      }
    } else {
      log_printf("failed to updated titleId %d: %s", current->app_id, sqlite3_errmsg(db));
    }

    // reuse in the next iteration
    sqlite3_reset(s);

    current = current->next;
  }

  log_printf("Priorities: updated %d rows", updated);
  return 0;
}

static int apply_visibility(sqlite3 *db, sqlite3_stmt *s, AppList *list) {
  AppNode *current = list->head;
  int rc = 0;
  int updated = 0;

  while (current != NULL) {
    // bind parameters (1-based index)
    // UPDATE tbl_iconinfo_XXX SET visible = ? WHERE titleId == ?;
    sqlite3_bind_int(s, 1, current->value);
    sqlite3_bind_text(s, 2, current->app_id, -1, SQLITE_STATIC);

    rc = sqlite3_step(s);
    if (rc == SQLITE_DONE) {
      if (sqlite3_changes(db) > 0) {
        updated++;
      }
    } else {
      log_printf("failed to updated titleId %d: %s", current->app_id, sqlite3_errmsg(db));
    }

    // reuse in the next iteration
    sqlite3_reset(s);

    current = current->next;
  }

  log_printf("Visibility: updated %d rows", updated);
  return 0;
}

int db_config_mismatch(AppConfig *config, int num_uids, UserID *uids) {
  sqlite3_stmt *sprio = NULL, *svis = NULL;
  char *err_msg = NULL;
  sqlite3 *db;
  int rc = 1;

  rc = sqlite3_open_v2(DB_PATH, &db, SQLITE_OPEN_READONLY, NULL);
  if (rc != SQLITE_OK) {
    log_printf("Cannot open database for reading");
    return rc;
  }

  sqlite3_busy_timeout(db, 2000);

  // it was observed that if there are db modifications, they usually apply
  // to all our overrides, so it's poitless checking all of the rows.

  // sortPriority
  const char *sql1 = "SELECT 1 FROM tbl_contentInfo WHERE titleId == ? AND sortPriority != ? LIMIT 1;";
  rc = sqlite3_prepare_v2(db, sql1, -1, &sprio, NULL);
  if (rc != SQLITE_OK) {
    log_printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    goto cleanup;
  }

  AppNode *current = config->sort.head;
  while (current != NULL) {
    // bind parameters (1-based index)
    sqlite3_bind_text(sprio, 1, current->app_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(sprio, 2, current->value);

    rc = sqlite3_step(sprio);
    if (rc == SQLITE_ROW) {
      // found our mismatch, quit out
      log_printf("sortPriority mismatch for app %s", current->app_id);
      goto cleanup;
    }

    // reuse in the next iteration
    sqlite3_reset(sprio);

    current = current->next;
  }

  // visible
  char *sql2 = sqlite3_mprintf("SELECT 1 FROM \"tbl_iconinfo_%w\" WHERE titleId == ? AND visible != ?;", uids[0]);
  if (sql2 == NULL) {
    log_printf("out of memory");
    goto cleanup;
  }
  rc = sqlite3_prepare_v2(db, sql2, -1, &svis, NULL);
  sqlite3_free(sql2);
  if (rc != SQLITE_OK) {
    log_printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    goto cleanup;
  }

  current = config->visibility.head;
  while (current != NULL) {
    // bind parameters (1-based index)
    sqlite3_bind_text(svis, 1, current->app_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(svis, 2, current->value);

    rc = sqlite3_step(svis);
    if (rc == SQLITE_ROW) {
      // found our mismatch, quit out
      log_printf("visibility mismatch for app %s", current->app_id);
      goto cleanup;
    }

    // reuse in the next iteration
    sqlite3_reset(svis);

    current = current->next;
  }

  // nothing found
  rc = 0;

cleanup:
  if (svis)
    sqlite3_finalize(svis);
  if (sprio)
    sqlite3_finalize(sprio);
  if (err_msg)
    sqlite3_free(err_msg);
  if (db)
    sqlite3_close(db);
  return rc;
}

int apply_config_to_db(AppConfig *config, int num_uids, UserID *uids) {
  sqlite3_stmt *sprio = NULL;
  char *err_msg = 0;
  sqlite3 *db = NULL;
  int rc = 0;

  rc = sqlite3_open_v2(DB_PATH, &db, SQLITE_OPEN_READWRITE, NULL);
  if (rc != SQLITE_OK) {
    log_printf("Cannot open database for writing");
    goto cleanup;
  }

  sqlite3_busy_timeout(db, 2000);

  // sortPriority
  const char *sql1 = "UPDATE tbl_contentInfo SET sortPriority = ? WHERE titleId = ?;";
  rc = sqlite3_prepare_v2(db, sql1, -1, &sprio, NULL);
  if (rc != SQLITE_OK) {
    log_printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    goto cleanup;
  }

  rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);
  if (rc != SQLITE_OK) {
    log_printf("Could not lock db: %s", err_msg);
    goto cleanup;
  }

  // apply visibility to all users
  for (int i = 0; i < num_uids; i++) {
    sqlite3_stmt *svis = NULL;

    // visible
    char *sql2 = sqlite3_mprintf("UPDATE \"tbl_iconinfo_%w\" SET visible = ? WHERE titleId == ?;", uids[i]);
    if (sql2 == NULL) {
      log_printf("out of memory");
      goto cleanup;
    }

    rc = sqlite3_prepare_v2(db, sql2, -1, &svis, NULL);
    sqlite3_free(sql2);

    if (rc != SQLITE_OK) {
      log_printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
      goto cleanup;
    }

    rc = apply_visibility(db, svis, &config->visibility);
    sqlite3_finalize(svis);
    if (rc != SQLITE_OK) {
      sqlite3_close(db);
      return rc;
    }
  }

  rc = apply_priorities(db, sprio, &config->sort);
  if (rc != SQLITE_OK) {
    sqlite3_close(db);
    return rc;
  }

  rc = sqlite3_exec(db, "COMMIT;", 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    log_printf("Commit failed: %s", err_msg);
    goto cleanup;
  }

cleanup:
  if (sprio)
    sqlite3_finalize(sprio);
  if (err_msg)
    sqlite3_free(err_msg);
  if (db)
    sqlite3_close(db);
  return rc;
}

// Populates the uids and num_uids with user profile ids for all existing users (max 16).
// Returns num_uids or 0 as error;
int populate_user_ids(int *num_uids, UserID *uids) {
  sqlite3 *db;
  sqlite3_stmt *s;
  int count = 0;

  int rc = sqlite3_open_v2(DB_PATH, &db, SQLITE_OPEN_READONLY, NULL);
  if (rc == SQLITE_OK) {
    const char *sql = "SELECT name FROM sqlite_schema WHERE type='table' AND name LIKE 'tbl_iconinfo_%';";
    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) == SQLITE_OK) {
      const char *prefix = "tbl_iconinfo_";
      size_t prefix_len = strlen(prefix);
      while (sqlite3_step(s) == SQLITE_ROW && count < MAX_NUM_USER_PROFILES) {
        const char *table = (const char *)sqlite3_column_text(s, 0);
        if (table && strncmp(table, prefix, prefix_len) == 0) {
          strncpy(uids[count], table + prefix_len, MAX_USER_PROFILE_LEN - 1);
          uids[count][MAX_USER_PROFILE_LEN - 1] = '\0';
          count++;
        }
      }
      sqlite3_finalize(s);
    }
    sqlite3_close(db);
  }

  *num_uids = count;
  return 0;
}

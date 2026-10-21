#include <stdio.h>
#include <unistd.h>
#include <sqlite3.h>

const char *DB_PATH = "/tmp/app.db";

int main(int argc, char *argv[])
{
  sqlite3 *db;
  char *err_msg = 0;
  
  const char *sql =
      "UPDATE tbl_iconinfo_0350443091 SET visible = 1 "
      "WHERE titleId IN ('NPXS40047', 'NPXS40056', 'NPXS40075');";

  int rc = sqlite3_open_v2(DB_PATH, &db, SQLITE_OPEN_READWRITE, NULL);
  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "[Error] Cannot open database for writing: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  sqlite3_busy_timeout(db, 5000);

  rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", 0, 0, &err_msg);
  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "[Lock Failed] Could not lock DB: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "[SQL Error] updated failed: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_exec(db, "ROLLBACK;", 0, 0, NULL);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_exec(db, "COMMIT;", 0, 0, &err_msg);
  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "[Commit Failed] Save failed: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return -1;
  }

  sqlite3_close(db);
  return 0;
}

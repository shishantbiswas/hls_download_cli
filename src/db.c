#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int init_db() {
  char *home = getenv("HOME");
  char result[256];

  if (home != NULL) {
    snprintf(result, sizeof(result), "%s/.local/share/spd", home);
  } else {
    printf(
        "%s",
        "HOME env is missing\n Try adding `export HOME='/home/you_username'`");
    return 1;
  }
  struct stat st = {0};

  if (stat(result, &st) == -1) {
    mkdir(result, 0700);
  }
  snprintf(result, sizeof(result), "%s/.local/share/spd/cache.db", home);

  sqlite3 *db;
  char *err_msg = NULL;

  int rc = sqlite3_open(result, &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  const char *sql = "\
    CREATE TABLE IF NOT EXISTS videos (\
        name TEXT NOT NULL UNIQUE,\
        value TEXT NULL,\
        uri TEXT NOT NULL UNIQUE PRIMARY KEY);\
    CREATE TABLE IF NOT EXISTS segments (\
        pending BOOLEAN NOT NULL DEFAULT 0,\
        name TEXT NOT NULL,\
        segment_uri TEXT NOT NULL,\
        video_uri TEXT NOT NULL,\
        PRIMARY KEY (video_uri, name),\
        FOREIGN KEY (video_uri) REFERENCES videos(uri) ON DELETE CASCADE\
    );";

  sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, &err_msg);
  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return 1;
  }

  sqlite3_close(db);
  return 0;
}

int rm_db() {
  char *home = getenv("HOME");
  char db_path[256];

  struct stat st = {0};

  if (home != NULL) {
    snprintf(db_path, sizeof(db_path), "%s/.local/share/spd", home);
  } else {
    printf("%s", "HOME env is missing\n Try adding `export "
                 "HOME='/home/you_username' to your .bashrc/.zshrc`");
    return 1;
  }

  snprintf(db_path, sizeof(db_path), "%s/.local/share/spd/cache.db", home);

  if (stat(db_path, &st) == 0) {
    if (remove(db_path) == 0) {
      printf("File '%s' deleted successfully.\n", db_path);
    } else {
      perror("Error deleting file");
    }
  }
  init_db();

  return 0;
}

int add_video(char *name, char *uri, char *video_uri) {
  // printf("%s\n%s\n%s", name, uri, video_uri);
  char *home = getenv("HOME");
  char result[256];
  char sql[1000];

  sqlite3 *db;
  char *err_msg = NULL;

  snprintf(result, sizeof(result), "%s/.local/share/spd/cache.db", home);
  int rc = sqlite3_open(result, &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }
  sqlite3_busy_timeout(db, 5000);

  snprintf(sql, sizeof(sql),
           "INSERT INTO videos (name,uri,value) VALUES ('%s','%s','%s');", name,
           uri, video_uri);

  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return 1;
  }

  sqlite3_close(db);
  return 0;
}

char *get_video_by_uri(char *uri) {
  char *home = getenv("HOME");
  char path_to_db[256];
  char *result = malloc(128);
  if (result == NULL) {
    printf("Memory allocation failed");
    return NULL;
  }

  result[0] = '\0';

  snprintf(path_to_db, sizeof(path_to_db), "%s/.local/share/spd/cache.db",
           home);

  sqlite3 *db;
  char *err_msg = NULL;

  int rc = sqlite3_open(path_to_db, &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return NULL;
  }

  const char *sql = "SELECT name FROM videos WHERE uri = ?;";
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return NULL;
  }
  sqlite3_busy_timeout(db, 5000);

  sqlite3_bind_text(stmt, 1, uri, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    const unsigned char *name = sqlite3_column_text(stmt, 0);
    strcpy(result, (char *)name);
  } else if (rc == SQLITE_DONE) {
    result = NULL;
  } else {
    fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return result;
}

char *get_video_value_by_uri(char *uri) {
  char *home = getenv("HOME");
  char path_to_db[256];
  char *result = malloc(128);
  if (result == NULL) {
    printf("Memory allocation failed");
    return NULL;
  }

  result[0] = '\0';

  snprintf(path_to_db, sizeof(path_to_db), "%s/.local/share/spd/cache.db",
           home);

  sqlite3 *db;
  char *err_msg = NULL;

  int rc = sqlite3_open(path_to_db, &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return NULL;
  }
  sqlite3_busy_timeout(db, 5000);

  const char *sql = "SELECT value FROM videos WHERE uri = ?;";
  sqlite3_stmt *stmt;

  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  sqlite3_bind_text(stmt, 1, uri, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    const unsigned char *name = sqlite3_column_text(stmt, 0);
    strcpy(result, (char *)name);
  } else if (rc == SQLITE_DONE) {
    result = NULL;
  } else {
    fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return result;
}

int rm_video(char *uri) {
  char *home = getenv("HOME");
  char path_to_db[256];

  snprintf(path_to_db, sizeof(path_to_db), "%s/.local/share/spd/cache.db",
           home);

  sqlite3 *db;
  char *err_msg = NULL;

  int rc = sqlite3_open(path_to_db, &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }
  sqlite3_busy_timeout(db, 5000);

  rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to enable foreign keys: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  if (db == NULL) {
    fprintf(stderr, "Database connection is NULL\n");
    return 1;
  }

  if (uri == NULL) {
    fprintf(stderr, "Video URI is NULL\n");
    return 1;
  }

  const char *sql = "DELETE FROM videos WHERE uri = ?;";

  sqlite3_stmt *stmt;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  rc = sqlite3_bind_text(stmt, 1, uri, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind video URI: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }

  sqlite3_finalize(stmt);
  return 0;
}

int add_segment_to_video(sqlite3 *db, char *uri, char *segment_name,
                         char *og_uri) {
  char sql[1000];

  char *err_msg = NULL;

  snprintf(sql, sizeof(sql),
           "INSERT INTO segments (name,pending,video_uri,segment_uri) VALUES "
           "('%s',1,'%s','%s');",
           segment_name, og_uri, uri);

  int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return 1;
  }

  return 0;
}

int get_segment_status(sqlite3 *db, char *name) {
  char *err_msg = NULL;

  const char *sql = "SELECT pending FROM segments WHERE name = ?;";
  sqlite3_stmt *stmt;
  int pending = -1;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return 1;
  }
  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    pending = sqlite3_column_int(stmt, 0);
  } else if (rc == SQLITE_DONE) {
    pending = -1;
  } else {
    fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
  }
  sqlite3_finalize(stmt);

  return pending;
}

int complete_segment_status(sqlite3 *db, char *name) {
  char *home = getenv("HOME");
  char result[256];
  char sql[1000] = "UPDATE segments SET pending = 0 WHERE name = ?;";

  // sqlite3_busy_timeout(db, 5000);

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  rc = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind name: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }

  sqlite3_finalize(stmt);
  return 0;
}
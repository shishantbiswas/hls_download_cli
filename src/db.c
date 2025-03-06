#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern sqlite3 *db;

int open_db(void) {
  char *home = getenv("HOME");
  char result[256];

  char *err_msg = NULL;

  if (home != NULL) {
    snprintf(result, sizeof(result), "%s/.local/share/spd",
             home); // db folder path
  } else {
    printf("HOME env is missing\n"
           "Try adding `export HOME='/home/you_username'`");
    return 1;
  }
  struct stat st = {0};

  // create db folder path if it doesn't exists
  if (stat(result, &st) == -1) {
    mkdir(result, 0700);
  }

  snprintf(result, sizeof(result), "%s/.local/share/spd/cache.db",
           home); // db file

  int rc = sqlite3_open(result, &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  sqlite3_busy_timeout(db, 10000);
  rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "foreign key can not be enabled: %s\n", sqlite3_errmsg(db));
    return 1;
  }
  rc = sqlite3_exec(db, "PRAGMA journal_mode = WAL;", 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "journal_modecan not be enabled: %s\n", sqlite3_errmsg(db));
    return 1;
  }
  return 0;
}

/*
    Initialize the database
*/
int init_db(void) {

  char *err_msg = NULL;

  const char *sql = "\
    CREATE TABLE IF NOT EXISTS videos (\
        name TEXT NOT NULL UNIQUE,\
        value TEXT NULL,\
        uri TEXT NOT NULL UNIQUE PRIMARY KEY);\
    CREATE TABLE IF NOT EXISTS segments (\
        pending BOOLEAN NOT NULL DEFAULT 0,\
        name TEXT NOT NULL,\
        video_name TEXT NOT NULL,\
        segment_uri TEXT NOT NULL,\
        video_uri TEXT NOT NULL,\
        PRIMARY KEY (video_uri, name),\
        FOREIGN KEY (video_uri) REFERENCES videos(uri) ON DELETE CASCADE\
    );\
    CREATE TABLE IF NOT EXISTS videos_pending (\
        uri TEXT NOT NULL UNIQUE PRIMARY KEY,\
        type TEXT NOT NULL,\
        arg TEXT\
    );";

  int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return 1;
  }

  return 0;
}

/*
    Remove the database
*/
int rm_db(void) {
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
  return 0;
}

/*
    Add a video to the video table
*/
int add_video(char *name, char *uri, char *video_uri) {
  char *sql = "INSERT INTO videos (name,uri,value) VALUES (?, ?, ?);";

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

  rc = sqlite3_bind_text(stmt, 2, uri, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind uri: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }

  rc = sqlite3_bind_text(stmt, 3, video_uri, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind video_uri: %s\n", sqlite3_errmsg(db));
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

/*
    Add a video to the pending table
*/
int add_video_to_pending(char *uri, char *type, char *arg) {
  char *sql = "INSERT INTO videos_pending (uri,type,arg) VALUES (?, ?, ?);";
  if (uri == NULL) {
    printf("%s", "Example: spd add https://www.youtube.com/watch?v=id yt\n");
    printf("%s", "URL is missing!\n");
    return 1;
  }
  
  if (strcmp(type, "yt") != 0 && strcmp(type, "vd") != 0) {
    printf("Type can only be `vd` or `yt` \n");
    return 1;
  }

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  rc = sqlite3_bind_text(stmt, 1, uri, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind name: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }

  rc = sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind uri: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }

  rc = sqlite3_bind_text(stmt, 3, arg, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind video_uri: %s\n", sqlite3_errmsg(db));
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

/*
    Get the video name/random name by the uri
*/
void get_video_by_uri(char *uri, char *result) {

  const char *sql = "SELECT name FROM videos WHERE uri = ?;";
  sqlite3_stmt *stmt;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return;
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
}

/*
    Get the video value/cached video name by the uri
*/
char *get_video_value_by_uri(char *uri) {
  char *result = malloc(128);
  if (result == NULL) {
    printf("Memory allocation failed");
    return NULL;
  }
  result[0] = '\0';

  const char *sql = "SELECT value FROM videos WHERE uri = ?;";
  sqlite3_stmt *stmt;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
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
  return result;
}

/*
    Remove the video from the database
*/
int rm_video(char *uri) {

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
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
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

/*
    Add a segment to the video
*/
int add_segment_to_video(char *uri, char *name, char *video_name,
                         char *og_uri) {
  const char *sql =
      "INSERT INTO segments (name,pending,video_uri,video_name,segment_uri) "
      "VALUES (?,?,?,?,?);";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 1;
  }
  rc = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind segment name: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }
  rc = sqlite3_bind_int(stmt, 2, 1);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind pending: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }
  rc = sqlite3_bind_text(stmt, 3, og_uri, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind og_uri: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }
  rc = sqlite3_bind_text(stmt, 4, video_name, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind uri: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }
  rc = sqlite3_bind_text(stmt, 5, uri, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind uri: %s\n", sqlite3_errmsg(db));
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

/*
    Get the segment status
*/
int get_segment_status(char *name, char *video_uri, char *segment_uri,
                       char *video_name) {

  // Add error checking for NULL parameters
  if (!db || !name || !video_uri || !segment_uri || !video_name) {
    fprintf(stderr, "Invalid parameters passed to get_segment_status\n");
    return -1;
  }

  const char *sql = "SELECT pending FROM segments WHERE segment_uri = ? AND "
                    "video_uri = ? AND name = ? AND video_name = ?;";
  sqlite3_stmt *stmt;
  int pending = -1;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL prepare error: %s\n",
            sqlite3_errmsg(db)); // Use sqlite3_errmsg instead
    return -1;                   // Return -1 for error instead of 1
  }

  // Add error checking for bindings
  if (sqlite3_bind_text(stmt, 1, segment_uri, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(stmt, 2, video_uri, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(stmt, 4, video_name, -1, SQLITE_STATIC) != SQLITE_OK) {
    fprintf(stderr, "Binding error: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    pending = sqlite3_column_int(stmt, 0);
  } else if (rc == SQLITE_DONE) {
    pending = -1; // No row found
  } else {
    fprintf(stderr, "Query execution error: %s\n", sqlite3_errmsg(db));
    pending = -1;
  }

  sqlite3_finalize(stmt);
  return pending;
}

/*
    Complete the segment status
*/
int complete_segment_status(char *segment_uri, char *name) {
  char *sql =
      "UPDATE segments SET pending = 0 WHERE segment_uri = ? AND name = ?;";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  rc = sqlite3_bind_text(stmt, 1, segment_uri, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind segment_uri: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 1;
  }
  rc = sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
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

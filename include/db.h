#include <sqlite3.h>

#ifndef DB_H
#define DB_H

int init_db();
int rm_db();
int add_video(char *name, char *uri, char *video_uri);
char *get_video_value_by_uri(char *uri);
int rm_video(char *uri);
char *get_video_by_uri(char *uri);
int add_segment_to_video(sqlite3 *db, char *uri, char *name, char *video_name, char *og_uri);
int get_segment_status(sqlite3 *db, char *name,char *video_uri,char *segment_uri, char *video_name);
int complete_segment_status(sqlite3 *db, char *segment_uri,char *name);

#endif

#include <sqlite3.h>

#ifndef DB_H
#define DB_H

int open_db();
int init_db();
int rm_db();
int add_video(char *name, char *uri, char *video_uri);
char *get_video_value_by_uri(char *uri);
int rm_video(char *uri);
void get_video_by_uri(char *uri,char *result) ;
int add_segment_to_video(char *uri, char *name, char *video_name, char *og_uri);
int get_segment_status(char *name,char *video_uri,char *segment_uri, char *video_name);
int complete_segment_status(char *segment_uri,char *name);

#endif

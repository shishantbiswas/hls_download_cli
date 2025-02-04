#include "db.h"
#include "helpers.h"
#include "hls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void show_help() {
  printf("Usage: main [options] <command>\n");
  printf("Options:\n");
  printf("  -h/-help  Show help\n");
  printf("  -v/-version  Show version information\n");
  printf("Commands:\n");
  printf("  init  Intialize the Cache Database\n");
  printf("  rm  Clears the Datebase\n");
  printf("  vd  Download hls video\n");
  printf("  yt  Download YouTube video\n");
}

void show_version() { printf("Version 0.1.5\n"); }

void yt(char *uri, char *resolution) {
  if (uri == NULL) {
    printf("%s", "Example: spd yt https://www.youtube.com/watch?v=id\n");
    printf("%s", "URL is missing!\n");
    return;
  } else if (resolution != NULL) {
    if (strcmp(resolution, "360") == 1 || strcmp(resolution, "720") == 1 ||
        strcmp(resolution, "1080") == 1) {
      printf("%s\n", "Resolution can only be 360, 720 or 1080\nDefault is 480");
      return;
    }
  }
  char *home = getenv("HOME");
  char result[256];

  if (home != NULL) {
    snprintf(result, sizeof(result), "%s/Music", home);
  }

  char command[1000];
  snprintf(command, sizeof(command),
           "yt-dlp -S \"res:%s\" -f mp4 -N 4 -o \"%s/%%(title)s.%%(ext)s\" %s",
           resolution != NULL ? resolution : "480", home != NULL ? result : "",
           uri);

  int command_result = system(command);

  if (command_result == -1) {
    perror("system");
    return;
  }
}

void dl(char *uri, char *addition_args) {
  // printf("%s%s", uri, addition_args);
  // printf("\033[30mThis is black text\033[0m\n");
  // printf("\033[31mThis is red text\033[0m\n");
  // printf("\033[32mThis is green text\033[0m\n");
  // printf("\033[33mThis is yellow text\033[0m\n");
  // printf("\033[34mThis is blue text\033[0m\n");
  // printf("\033[35mThis is magenta text\033[0m\n");
  // printf("\033[36mThis is cyan text\033[0m\n");
  // printf("\033[37mThis is white text\033[0m\n");

  // printf("\033[90mThis is black text\033[0m\n");
  // printf("\033[91mThis is red text\033[0m\n");
  // printf("\033[92mThis is green text\033[0m\n");
  // printf("\033[93mThis is yellow text\033[0m\n");
  // printf("\033[94mThis is blue text\033[0m\n");
  // printf("\033[95mThis is magenta text\033[0m\n");
  // printf("\033[96mThis is cyan text\033[0m\n");
  // printf("\033[97mThis is white text\033[0m\n");

  // printf("\033[34;43mBlue text on yellow background\033[0m\n");
  // add_video("somename","https://","uri_of_videp");
  // add_segment_to_video("uri_of_videp1","segf_name","video","https://");
  // add_segment_to_video("uri_of_videp2","sesg_name","video","https://");
  // add_segment_to_video("uri_of_videp5","seg_nayme","video","https://");
  // add_segment_to_video("uri_of_videp3","seg_vname","video","https://");
  // add_segment_to_video("uri_of_videp4","seg_nhame","video","https://");
  rm_video("https://");
  return;
}

sqlite3 *db;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    show_help();
    return 0;
  }
  char *command = argv[1];
  char *uri = argv[2];
  char *addition_args = argv[3];

  if (strcmp(command, "-h") == 0 || strcmp(command, "-help") == 0) {
    show_help();
  } else if (strcmp(command, "-v") == 0 || strcmp(command, "-version") == 0) {
    show_version();
  } else if (strcmp(command, "vd") == 0) {
    open_db();
    hls_command(uri, addition_args);
  } else if (strcmp(command, "yt") == 0) {
    yt(uri, addition_args);
  } else if (strcmp(command, "dl") == 0) {
    open_db();
    dl(uri, addition_args);
  } else if (strcmp(command, "init") == 0) {
    open_db();
    init_db();
  } else if (strcmp(command, "rm") == 0) {
    rm_db();
    open_db();
    init_db();
  } else {
    printf("Unknown command: %s\n", command);
    show_help();
  }

  if (db != NULL) {
    sqlite3_close(db);
  }

  return 0;
}

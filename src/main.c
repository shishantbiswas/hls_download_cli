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

void show_version() { printf("Version 0.1.4\n"); }

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
  printf("%s%s",uri,addition_args);
  return;
}

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
    hls_command(uri, addition_args);
  } else if (strcmp(command, "yt") == 0) {
    yt(uri, addition_args);
  } else if (strcmp(command, "dl") == 0) {
    dl(uri, addition_args);
  } else if (strcmp(command, "init") == 0) {
    init_db();
  } else if (strcmp(command, "rm") == 0) {
    rm_db();
  } else {
    printf("Unknown command: %s\n", command);
    show_help();
    return 0;
  }

  return 0;
}

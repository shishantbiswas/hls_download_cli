#include "helpers.h"
#include "hls.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool isMulti;

void show_help(void) {

  printf("\033[33mUsage:\033[0m\n  spd [options] <command>\n"
         "\033[33mOptions:\033[0m\n"
         "  -h/--help     Show help\n"
         "  -v/--version  Show version information\n\n"
         "\033[33mCommands:\033[0m\n"
         "  \033[32mvd\033[0m      Download hls video\n"
         "  \033[32mmd\033[0m      Download hls video (multithreaded)\n"
         "  \033[32myt\033[0m      Download YouTube video\n"
         "  \033[32mrm\033[0m      Delete Datebase\n"
         "  \033[32mfile\033[0m    Download using file\n");
}

void show_version(void) { printf("version 0.1.5\n"); }

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

  char command[512];
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

void download_from_text_file(char *file) {
  FILE *fp;
  fp = fopen(file, "r");
  if (!fp) {
    perror("Failed to create file");
    return;
  }

  fseek(fp, 0, SEEK_END);
  long file_size = ftell(fp);
  rewind(fp);

  char *buffer = (char *)malloc(file_size + 1);
  if (buffer == NULL) {
    perror("Memory allocation failed");
    fclose(fp);
    return;
  }

  fread(buffer, 1, file_size, fp);
  buffer[file_size] = '\0';

  char **lines = split(buffer, "\n");

  for (int i = 0; lines[i] != NULL; i++) {
    if (start_with("#", lines[i]) || start_with("//", lines[i])) {
      printf("Ingoring line ");
      printf("%s \n", lines[i]);
      continue;
    } else if (start_with("exit", lines[i])) {
      printf("exiting...");
      break;
    } else {

      char **args = split(lines[i], " ");

      char *command = args[0];
      char *uri = args[1];
      char *addition_args = args[2] ? args[2] : NULL;
      if (strcmp(command, "yt") == 0) {
        yt(uri, addition_args);
      } else if (strcmp(command, "md") == 0) {
        isMulti = true;
        hls_command(uri, addition_args);
      } else {
        isMulti = false;
        hls_command(uri, addition_args);
      }
      free_split(args);
    }
  }

  free_split(lines);
  free(buffer);
  fclose(fp);
}


int main(int argc, char *argv[]) {
  if (argc < 2) {
    show_help();
    return 0;
  }
  char *cmd = argv[1];
  char *uri = argv[2];
  char *addition_args = (argc > 3) ? argv[3] : NULL;

  if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
    show_help();
  } else if (strcmp(cmd, "-v") == 0 || strcmp(cmd, "--version") == 0) {
    show_version();
  } else if (strcmp(cmd, "vd") == 0) {
    isMulti = false;
    hls_command(uri, addition_args);
  } else if (strcmp(cmd, "md") == 0) {
    isMulti = true;
    hls_command(uri, addition_args);
  } else if (strcmp(cmd, "yt") == 0) {
    yt(uri, addition_args);
  } else if (strcmp(cmd, "file") == 0) {
    download_from_text_file(uri);
  } else {
    printf("Unknown command: %s\n", cmd);
    show_help();
  }

  return 0;
}

#include "hls.h"
#include "helpers.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TOTAL_CONCURRENT_CONNECTION 5

extern bool isMulti;

char *parse_hls_manifest(char *manifest) {
  char *copy = strdup(manifest);
  if (copy == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }

  int userInput;
  int num_of_resolutions = 0;
  int resolutions[10];

  char *result = NULL;

  char **seperated = split(copy, "\n");
  if (seperated == NULL) {
    fprintf(stderr, "Failed to split string\n");
    return NULL;
  }
  for (int i = 0; seperated[i] != NULL; i++) {
    remove_invisible_chars(seperated[i]);
    if (!start_with("#", seperated[i])) {
      char **lo = split(seperated[i - 1], ",");
      for (int j = 0; lo[j] != NULL; j++) {
        if (start_with("RESOLUTION=", lo[j])) {
          printf("%d) %s\n", num_of_resolutions + 1, lo[j]);
          resolutions[num_of_resolutions] = i;
          num_of_resolutions++;
        }
      }
      free_split(lo);
    }
  }

  if (num_of_resolutions == 0) {
    printf("%s", "No Resolutions Found!");
    return NULL;
  }

  do {
    printf("Pick A Resolution : ");
    scanf("%d", &userInput);
    result = strdup(seperated[resolutions[(userInput - 1)]]);

    if (userInput < 1 || userInput > num_of_resolutions) {
      printf("Invalid input. Try again.\n");
    }
  } while (userInput < 1 || userInput > num_of_resolutions);
  free_split(seperated);
  free(copy);
  return result;
}

int is_video_manifest(char *manifest) {
  char *copy = strdup(manifest);
  if (copy == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return -1;
  }

  int num_of_resolutions = 0;
  char **seperated = split(copy, "\n");
  if (seperated == NULL) {
    fprintf(stderr, "Failed to split string\n");
    return -1;
  }
  for (int i = 0; seperated[i] != NULL; i++) {
    remove_invisible_chars(seperated[i]);
    if (strstr(seperated[i], ".m3u8") != NULL) {
      num_of_resolutions++;
    }
  }
  free_split(seperated);
  free(copy);
  return num_of_resolutions > 0 ? 0 : 1;
}

int ffmpeg_merge(char *folder_name, char *output_file) {
  int ffmpeg_installed = system("ffmpeg -version");
  if (ffmpeg_installed != 0) {
    printf("%s", "Error: ffmpeg is not installed on the system\n");
    return -1;
  }
  char command[1024];
  snprintf(command, sizeof(command),
           "ffmpeg -allowed_extensions ALL -i ./%s/merge_file.m3u8 -c copy %s",
           folder_name, output_file);

  int result = system(command);
  if (result != 0) {
    printf("Error: Failed to merge video segments\n");
    return -1;
  }
  return 0;
}

void hls_command(char *uri, char *video_name) {
  if (uri == NULL) {
    printf("%s",
           "\033[33mExample:\033[0m\n  spd vd http://localhost/master.m3u8\n");
    printf("%s", "  URL is missing!\n");
    return;
  }

  char result[100];
  if (validate_hls_url(uri, result) == -1) {
    printf("[error] Check your Internet connection\n");
    return;
  } else if (validate_hls_url(uri, result) == 1) {
    printf("[error] No HLS Stream Found\n");
    return;
  }

  MemoryStruct chunk = {
      .memory = malloc(1),
      .size = 0,
  };
  char *response = fetch(uri, &chunk);
  if (response == NULL) {
    if (chunk.memory != NULL) {
      free(chunk.memory);
    }
    return;
  }
  printf("[video] Checking for video resolutions\n");
  int is_video = is_video_manifest(response);
  if (is_video == 1) {
    printf("[video] Video found stating download\n");
    prepare_download(uri, uri, video_name);
  } else {
    char *video_resolution = parse_hls_manifest(response);

    char *video_uri = make_complete_url(uri, video_resolution);
    prepare_download(video_uri, uri, video_name);
    free(video_uri);
    free(video_resolution);
  }
  free(chunk.memory);
  return;
}

volatile int completed = 0;
volatile int failed = 0;
int total = 0;
sem_t semaphore;

void *loading_indicator(void *arg) {
  (void)arg;
  while (completed < total) {
    int percentage = (int)(((float)completed / total) * 100);
    printf("\r[%s] Progress: %d%% %d/%d", completed % 2 == 0 ? "/" : "\\",
           percentage, completed, total);
    fflush(stdout);
    usleep(200000);
  }
  printf("\nAll tasks completed!\n");
  return NULL;
}

void *thread_function(void *arg) {
  Info *info = (Info *)arg;
  sem_wait(&semaphore);
  char *segment_name = info->segment_name;

  int success = 0;
  int retries = 5;
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", info->folder_name, segment_name);
  while (retries > 0) {
    printf("uri: %s\n", info->segment_uri);
    int result = download_file(info->segment_uri, path);
    if (result != 0) {
      retries--;
      printf("\nRetrying... (%d retries left)\n", retries);
      continue;
    }
    success = 1;
    break;
  }

  if (!success) {
    __sync_fetch_and_add(&failed, 1);
    fprintf(stderr, "Failed to download file after multiple attempts: %s\n",
            info->segment_name);
  }
  sem_post(&semaphore);
  free(info);
  __sync_fetch_and_add(&completed, 1);
  return NULL;
}

int prepare_download(char *uri, char *og_uri, char *video_name) {
  MemoryStruct chunk = {
      .memory = malloc(1),
      .size = 0,
  };
  char *response = fetch(uri, &chunk);
  if (response == NULL) {
    if (chunk.memory != NULL) {
      free(chunk.memory);
    }
    return -1;
  }

  char *response_copy = strdup(response);
  if (response_copy == NULL) {
    printf("strdup failed");
    return -1;
  }

  char **seperated = split(response, "\n");
  if (seperated == NULL) {
    free(response);
    free_split(seperated);
    return -1;
  }

  struct stat st = {0};
  char path[256];
  char folder_name[128];
  folder_name[0] = '\0';

  char *r_str = random_string(15);
  char *lol = video_name == NULL ? r_str : video_name;
  strcpy(folder_name, lol);
  free(r_str);

  snprintf(path, sizeof(path), "%s/merge_file.m3u8", folder_name);
  if (stat(path, &st) == 0) {
    remove(path);
  }
  snprintf(path, sizeof(path), "%s", folder_name);
  if (stat(path, &st) != 0) {
    mkdir(path, 0700);
  }

  char **merge_file_temp = split(response_copy, "\n");

  int merge_file_errors = 0;
  for (int i = 0; merge_file_temp[i] != NULL; i++) {
    remove_invisible_chars(merge_file_temp[i]);
    char *temp = make_segment_from_url(merge_file_temp[i]);
    remove_invisible_chars(temp);
    // remove_query_params(temp);
    merge_file_manifest(path, temp);
    free(temp);
    if (!start_with("#EXT", seperated[i]) && strlen(seperated[i]) > 0) {
      total++;
    }
  }
  free_split(merge_file_temp);
  free(response_copy);

  if (merge_file_errors > 0) {
    printf("\033[31m[Error] There were %d many errors\nVideo merge will "
           "probably fail\033[0m\n",
           merge_file_errors);
  }

  pthread_t threads[total];

  int no_threads = isMulti ? TOTAL_CONCURRENT_CONNECTION : 1 ;

  sem_init(&semaphore, 0, no_threads);
  pthread_t loader_thread;
  pthread_create(&loader_thread, NULL, loading_indicator, NULL);

  int added = 0;

  printf("\n\033[34m[Download] Starting download with %d "
         "concurrent threads\033[0m\n",
         no_threads);

  for (int i = 0; seperated[i] != NULL; i++) {
    remove_invisible_chars(seperated[i]);
    if (!start_with("#EXT", seperated[i]) && strlen(seperated[i]) > 0) {
      char *complete_seg_uri = make_complete_url(uri, seperated[i]);
      char *seg_name = make_segment_from_url(seperated[i]);
      char *seg_uri = make_complete_url(og_uri, seperated[i]);
      // remove_query_params(seg_name);
      Info *info = malloc(sizeof(Info));
      if (info == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
      }
      strncpy(info->uri, uri, sizeof(info->uri) - 1);
      strncpy(info->segment_uri, complete_seg_uri,
              sizeof(info->segment_uri) - 1);
      strncpy(info->segment_name, seg_name, sizeof(info->segment_name) - 1);
      strncpy(info->folder_name, folder_name, sizeof(info->folder_name) - 1);
      info->uri[sizeof(info->uri) - 1] = '\0';
      info->folder_name[sizeof(info->folder_name) - 1] = '\0';
      info->segment_uri[sizeof(info->segment_uri) - 1] = '\0';
      info->segment_name[sizeof(info->segment_name) - 1] = '\0';

      pthread_create(&threads[added], NULL, thread_function, info);
      added++;

      free(complete_seg_uri);
      free(seg_uri);
      free(seg_name);
    }
  }

  for (int i = 0; i < total; i++) {
    pthread_join(threads[i], NULL);
  }
  pthread_join(loader_thread, NULL);

  printf("\n\033[34m[Download] Download Complete\033[0m");

  sem_destroy(&semaphore);
  free_split(seperated);
  free(response);

  char output_file[256];
  char *home = getenv("HOME");

  snprintf(output_file, sizeof(output_file), "%s/Downloads/%s.mp4", home,
           folder_name);

  if (failed > 0) {
    printf("\nFailed to download all segment\n  Rerun the command to download "
           "missing segment");
    return 1;
  }

  // make video
  printf("\n\033[34m[Merge] Starting Merge\033[0m\n");
  int success = ffmpeg_merge(folder_name, output_file);
  if (success == 0) {
    printf("\n\033[34m[Clean] Running after merge cleanup...\033[0m");
    delete_directory(folder_name);
    printf("\n\033[32m[Merge] Video Saved Successfully at %s \033[0m\n",
           output_file);
  }
  return 0;
}

char *make_complete_url(char *original_url, char *segment) {
  char *copy = strdup(original_url);
  if (copy == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  char *complete_url = malloc(1024);
  if (complete_url == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  complete_url[0] = '\0';

  if (start_with("http", segment)) {
    strcat(complete_url, segment);
    free(copy);
    return complete_url;
  }

  char **seperated = split(copy, "/");
  for (int i = 0; seperated[i] != NULL; i++) {
    if (seperated[i + 1] == NULL) {
      continue;
    }
    if (start_with("http", seperated[i])) {
      strcat(complete_url, seperated[i]);
      strcat(complete_url, "//");
    } else {
      strcat(complete_url, seperated[i]);
      strcat(complete_url, "/");
    }
  };
  strcat(complete_url, segment);
  free_split(seperated);
  free(copy);
  return complete_url;
}

char *make_segment_from_url(char *original_segment) {

  char *copy = strdup(original_segment);
  if (copy == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  char *segment = malloc(1024);
  if (segment == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  segment[0] = '\0';

  if (!(start_with(original_segment, "http") == 0)) {
    strcat(segment, original_segment);
    free(copy);
    return segment;
  }

  char **seperated = split(copy, "/");
  for (int i = 0; seperated[i] != NULL; i++) {
    if (seperated[i + 1] == NULL) {
      strcat(segment, seperated[i]);
      break;
    }
  };
  free_split(seperated);
  free(copy);
  return segment;
}

/* AVOID: legacy text based merge file  */
int merge_file(char *path, char *segment_name) {
  FILE *fp;
  char complete_path[256];
  char content[256];
  snprintf(complete_path, sizeof(complete_path), "%s/merge_file.txt", path);
  fp = fopen(complete_path, "a");
  if (fp == NULL) {
    printf("failed to open file");
    return 1;
  }
  snprintf(content, sizeof(content), "file '%s'\n", segment_name);
  fprintf(fp, "%s", content);
  fclose(fp);
  return 0;
}

int merge_file_manifest(char *path, char *segment_name) {
  FILE *fp;
  char complete_path[256];
  char content[256];
  snprintf(complete_path, sizeof(complete_path), "%s/merge_file.m3u8", path);
  fp = fopen(complete_path, "a");
  if (fp == NULL) {
    printf("failed to open file");
    return 1;
  }
  snprintf(content, sizeof(content), "%s\n", segment_name);
  fprintf(fp, "%s", content);
  fclose(fp);
  return 0;
}

int validate_hls_url(char *url, char *output) {

  MemoryStruct chunk = {
      .memory = malloc(1),
      .size = 0,
  };
  const char *response = fetch(url, &chunk);
  if (response == NULL) {
    if (chunk.memory != NULL) {
      free(chunk.memory);
    }
    return -1;
  }

  char *newline = strchr(response, '\n');
  size_t len = newline - response;
  int result = 1;
  if (newline) {
    strncpy(output, response, len);
    if (start_with(output, "#EXTM3U")) {
      result = 0;
    } else {
      result = 1;
    }
  }

  free(chunk.memory);
  return result;
}

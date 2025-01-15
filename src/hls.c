#include "db.h"
#include "helpers.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int prepare_download(char *uri, char *og_uri, char *video_name);
char *make_complete_url(char *original_url, char *segment);
char *make_segment_from_url(char *original_url);
int merge_file(char *path, char *segment_name);

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
           "ffmpeg -f concat -safe 0 -i ./%s/merge_file.txt -c copy %s",
           folder_name, output_file);
  int result = system(command);
  if (result != 0) {
    printf("Error: Failed to merge video segments\n");
    return -1;
  }
  return 0;
}

void run_hls_command(char *uri, char *video_name) {
  if (uri == NULL) {
    printf("%s", "Example: main vd http://localhost/master.m3u8\n");
    printf("%s", "URL is missing!\n");
    return;
  }
  MemoryStruct chunk = {
      .memory = malloc(0),
      .size = 0,
  };
  char *response = fetch(uri, &chunk);
  if (response == NULL) {
    if (chunk.memory != NULL) {
      free(chunk.memory);
    }
    return;
  }

  int is_video = is_video_manifest(response);
  if (is_video == 1) {
    prepare_download(uri, uri, video_name);
  } else {
    char *cached = get_video_value_by_uri(uri);
    if (cached != NULL) {
      prepare_download(cached, uri, video_name);
    } else {

      char *video_resolution = parse_hls_manifest(response);

      char *video_uri = make_complete_url(uri, video_resolution);
      prepare_download(video_uri, uri, video_name);
      free(video_uri);
      free(video_resolution);
    }
  }
  free(chunk.memory);
  return;
}

volatile int completed = 0;
volatile int failed = 0;
int total = 0;
sem_t semaphore;
sqlite3 *db;

void *loading_indicator(void *arg) {

  while (completed < total) {
    int percentage = (int)(((float)completed / total) * 100);
    printf("\r[%s] Progress: %d %% %d/%d completed",
    percentage % 2 == 0 ? "/" : "\\"
    , percentage, completed, total);
    fflush(stdout);
    usleep(20000);
  }
  printf("\nAll tasks completed!\n");
  return NULL;
}

typedef struct Info{
  char uri[1000];
  char segment_uri[1000];
  char segment_name[512];
  char folder_name[512];
} Info;

void *thread_function(void *arg) {
  Info *info = (Info *)arg;
  sem_wait(&semaphore);
  // if in local fs and db shows pending false increment completed and skip
  // downloading
  if (get_segment_status(db, info->segment_name) == 0) {
    __sync_fetch_and_add(&completed, 1);
    sem_post(&semaphore);
    free(info);
    printf("\n\e[1;34mFound Cache %s\e[0m", info->segment_name);
    return NULL;
  }

  int success = 0;
  int retries = 10;
  char path[1000];
  snprintf(path, sizeof(path), "%s/%s", info->folder_name, info->segment_name);
  while (retries > 0) {
    int result = download_file(info->segment_uri, path);
    if (result != 0) {
      retries--;
      printf("Retrying... (%d retries left)\n", retries);
      continue;
    }
    success = 1;
    int lol = complete_segment_status(db, info->segment_name);

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
      .memory = malloc(0),
      .size = 0,
  };
  char *response = fetch(uri, &chunk);
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

  char *cached_video = get_video_by_uri(og_uri);

  if (cached_video != NULL) {
    strcpy(folder_name, cached_video);
  } else {
    char *lol = video_name == NULL ? random_string(15) : video_name;
    strcpy(folder_name, lol);
    int av = add_video(folder_name, og_uri, uri);
    if (av != 0) {
      printf("Failed to save video");
      return -1;
    }
  }

  free(cached_video);

  snprintf(path, sizeof(path), "%s/merge_file.txt", folder_name);
  if (stat(path, &st) == 0) {
    remove(path);
  }
  snprintf(path, sizeof(path), "%s", folder_name);
  if (stat(path, &st) != 0) {
    mkdir(path, 0700);
  }

  int merge_file_errors = 0;
  for (int i = 0; seperated[i] != NULL; i++) {
    remove_invisible_chars(seperated[i]);
    if (!start_with("#EXT", seperated[i]) && strlen(seperated[i]) > 0) {
      char *temp = make_segment_from_url(seperated[i]);
      remove_invisible_chars(temp);
      if (merge_file(path, temp) != 0) {
        merge_file_errors++;
      }
      free(temp);
      total++;
    }
  }
  if (merge_file_errors > 0) {
    printf("There were %d many errors\nVideo merge will probably fail",
           merge_file_errors);
  }

  pthread_t threads[total];
  sem_init(&semaphore, 0, 3);
  pthread_t loader_thread;
  pthread_create(&loader_thread, NULL, loading_indicator, NULL);

  int added = 0;

  char path_to_db[256];
  char *home = getenv("HOME");

  snprintf(path_to_db, sizeof(path_to_db), "%s/.local/share/spd/cache.db",
           home);

  int rc = sqlite3_open(path_to_db, &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }
  sqlite3_busy_timeout(db, 5000);

  for (int i = 0; seperated[i] != NULL; i++) {
    remove_invisible_chars(seperated[i]);
    if (!start_with("#EXT", seperated[i]) && strlen(seperated[i]) > 0) {
      char *complete_seg_uri = make_complete_url(uri, seperated[i]);
      char *seg_name = make_segment_from_url(seperated[i]);
      char *seg_uri = make_complete_url(og_uri, seperated[i]);

      Info *info = malloc(sizeof(Info));
      if (info == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
      }

      if (get_segment_status(db, seg_name) == -1) { // doesn't exists in cache

        add_segment_to_video(db, seg_uri, seg_name, og_uri);
        strncpy(info->uri, uri, sizeof(info->uri) - 1);
        strncpy(info->segment_uri, complete_seg_uri,
                sizeof(info->segment_uri) - 1);
        char *temp = make_segment_from_url(seperated[i]);
        strncpy(info->segment_name, temp, sizeof(info->segment_name) - 1);
        free(temp);
        strncpy(info->folder_name, folder_name, sizeof(info->folder_name) - 1);

        info->uri[sizeof(info->uri) - 1] = '\0';
        info->folder_name[sizeof(info->folder_name) - 1] = '\0';
        info->segment_uri[sizeof(info->segment_uri) - 1] = '\0';
        info->segment_name[sizeof(info->segment_name) - 1] = '\0';
        free(complete_seg_uri);
        pthread_create(&threads[added], NULL, thread_function, info);
        added++;
      } else
      // if (get_segment_status(db, seg_name) == 1)
      { // exists but pending

        strncpy(info->uri, uri, sizeof(info->uri) - 1);
        strncpy(info->segment_uri, complete_seg_uri,
                sizeof(info->segment_uri) - 1);
        char *temp = make_segment_from_url(seperated[i]);
        strncpy(info->segment_name, temp, sizeof(info->segment_name) - 1);
        free(temp);
        strncpy(info->folder_name, folder_name, sizeof(info->folder_name) - 1);

        info->uri[sizeof(info->uri) - 1] = '\0';
        info->folder_name[sizeof(info->folder_name) - 1] = '\0';
        info->segment_uri[sizeof(info->segment_uri) - 1] = '\0';
        info->segment_name[sizeof(info->segment_name) - 1] = '\0';
        free(complete_seg_uri);
        pthread_create(&threads[added], NULL, thread_function, info);
        added++;
      }
      // else if (get_segment_status(db, seperated[i]) == 0) { // exists and
      // complete
      //   total--;
      //   continue;
      // }
      free(seg_uri);
      free(seg_name);
    }
  }

  pthread_join(loader_thread, NULL);

  for (int i = 0; i < total; i++) {
    pthread_join(threads[i], NULL);
  }

  sqlite3_close(db);
  sem_destroy(&semaphore);
  free_split(seperated);
  free(response);

  char output_file[256];

  snprintf(output_file, sizeof(output_file), "%s/Downloads/%s.mp4", home,
           folder_name);

  if (failed > 0) {
    printf("\nFailed to download all segment\nRerun the command to download "
           "missing segment",
           "");
    return 1;
  }

  int success = ffmpeg_merge(folder_name, output_file);
  if (success == 0) {
    rm_video(uri);
    printf("%s", "Merge Successful\n");
    delete_directory(folder_name);
  }
  return 0;
}

char *make_complete_url(char *original_url, char *segment) {
  char *copy = strdup(original_url);
  if (copy == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  char *complete_url = malloc(1000);
  if (complete_url == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  complete_url[0] = '\0';

  if (start_with("http", segment)) {
    // printf("%s", segment);
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
  char *segment = malloc(1000);
  if (segment == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  segment[0] = '\0';

  if (!start_with(original_segment, "http") == 0) {
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
  fprintf(fp, content);
  fclose(fp);
  return 0;
}
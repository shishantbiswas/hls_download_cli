#include "helpers.h"
#include <ctype.h>
#include <curl/curl.h>
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
/*
    Removes unprintinable symbols
    and ascii char using isprint
*/
void remove_invisible_chars(char *str) {
  char *src = str;
  char *dst = str;

  while (*src) {
    if (isprint((unsigned char)*src)) {
      *dst++ = *src;
    }
    src++;
  }
  *dst = '\0';
}

/*
    High level split function similar to JavaScript,python,etc.
    Needs to be freed using free_split
*/
char **split(char *str, char *delim) {
  char *savep = str;
  char *token;
  char **arr;
  int capacity = 10;
  int length = 0;

  arr = (char **)malloc(capacity * sizeof(char *));

  if (arr == NULL) {
    perror("malloc failed");
    return NULL;
  };

  while ((token = strtok_r(savep, delim, &savep))) {
    if (length >= capacity) {
      capacity *= 2;
      char **temp = (char **)realloc(arr, capacity * sizeof(char *));
      if (temp == NULL) {
        perror("realloc failed");
        for (int i = 0; i < length; i++)
          free(arr[i]);
        free(arr);
        return NULL;
      }
      arr = temp;
    }

    arr[length] = strdup(token);
    if (arr[length] == NULL) {
      perror("strdup failed");
      for (int i = 0; i < length; i++)
        free(arr[i]);
      free(arr);
      return NULL;
    }

    length++;
  }
  arr[length] = NULL;
  return arr;
}

/*
    free data made by split
*/
void free_split(char **arr) {
  if (arr == NULL)
    return;
  for (int i = 0; arr[i] != NULL; i++)
    free(arr[i]);
  free(arr);
}

/*
    checks if PRE starts_with STR
*/
int start_with(const char *pre, const char *str) {
  return strncmp(pre, str, strlen(pre)) == 0;
}

/*
    The memory callback function used by libcurl
*/
size_t mem_cb(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  MemoryStruct *mem = (MemoryStruct *)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory == NULL) {
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

/*
    A generic fetch function which return the body
    Needs to be Freed manually or free the MemoryStruct
*/
char *fetch(const char *url, MemoryStruct *chunk) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Failed to initialize CURL\n");
    return NULL;
  }
  CURLcode response;

  curl_global_init(CURL_GLOBAL_ALL);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  response = curl_easy_perform(curl);
  if (response != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(response));
    return NULL;
  }
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return chunk->memory;
}

/*
    Return random string of specified LIMIT
    Needs to be Freed manually
*/
char *random_string(int limit) {
  char *result = malloc(limit + 1);
  if (result == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }

  char char_set[] = "abcdefghijklmnopqrstuvwxyz";
  int char_set_size = sizeof(char_set) - 1;

  srand((unsigned int)time(0));
  for (int i = 0; i < limit; i++) {
    result[i] = char_set[rand() % char_set_size];
  }
  result[limit] = '\0';

  return result;
}

int download_file(char *uri, char *filename) {
  if (!uri || !filename) {
    fprintf(stderr, "Invalid parameters\n");
    return 1;
  }

  if (strlen(filename) >= 512) {
    fprintf(stderr, "Filename too long\n");
    return 1;
  }

  if (strstr(filename, "..") != NULL) {
    fprintf(stderr, "Invalid filename\n");
    return 1;
  }

  FILE *fp;
  CURLcode res;

  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Failed to initialize CURL\n");
    return 1;
  }

  fp = fopen(filename, "wb");
  if (!fp) {
    perror("Failed to create file");
    curl_easy_cleanup(curl);
    return 1;
  }
  remove_invisible_chars(uri);

  curl_easy_setopt(curl, CURLOPT_URL, uri);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  // curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 25L);
  // curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 50L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10L);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "\n\033[31mDownload failed: %s\033[0m",
            curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    fclose(fp);
    return 1;
  }

  long response_code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  if (response_code != 200) {
    fprintf(stderr, "HTTP error: %ld\n", response_code);
    curl_easy_cleanup(curl);
    fclose(fp);
    return 1;
  }

  curl_easy_cleanup(curl);
  fclose(fp);

  return 0;
}

void delete_directory(const char *path) {
  struct dirent *entry;
  char full_path[1024];
  struct stat path_stat;

  DIR *dir = opendir(path);
  if (!dir) {
    perror("opendir");
    return;
  }

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
        strcmp(entry->d_name, ".DS_Store") == 0)
      continue;

    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

    if (stat(full_path, &path_stat) == 0) {
      if (S_ISDIR(path_stat.st_mode)) {
        delete_directory(full_path); // Recursive call for subdirectory
      } else {
        if (unlink(full_path) != 0) {
          perror("unlink");
        }
      }
    }
  }

  closedir(dir);

  if (rmdir(path) != 0) {
    perror("rmdir");
  }
}

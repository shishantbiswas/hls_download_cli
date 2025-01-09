#include <stddef.h>
#include <curl/curl.h>

#ifndef HELPERS_H
#define HELPERS_H

typedef struct {
  char *memory;
  size_t size;
} MemoryStruct;

void remove_invisible_chars(char *str);
char *fetch(const char *url, MemoryStruct *chunk);
char **split(char *str, char *delim);
void free_split(char **arr);
int start_with(const char *pre, const char *str);
static size_t mem_cb(void *contents, size_t size, size_t nmemb, void *userp);
char *random_string(int limit);
int download_file(char *uri, char *filename);
void delete_directory(const char *path);
#endif
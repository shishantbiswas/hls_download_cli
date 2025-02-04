#ifndef HLS_H
#define HLS_H

typedef struct Info {
  char uri[1024];
  char segment_uri[1024];
  char segment_name[512];
  char folder_name[512];
} Info;

char *parse_hls_manifest(char *manifest);
void hls_command(char *uri, char *video_name);
int is_video_manifest(char *manifest);
int download_file(char *uri, char *filename);
char *make_segment_from_url(char *segment);
char *make_complete_url(char *original_url, char *segment);
int ffmpeg_merge(char *folder_name, char *output_file);

int prepare_download(char *uri, char *og_uri, char *video_name);
int merge_file(char *path, char *segment_name);
int merge_file_manifest(char *path, char *segment_name);
int validate_hls_url(char *url, char *output);

#endif

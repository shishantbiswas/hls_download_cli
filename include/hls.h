#ifndef HLS_H
#define HLS_H

char *parse_hls_manifest(char *manifest);
void run_hls_command(char *uri, char *video_name);
int is_video_manifest(char *manifest);
int download_file(char *uri, char *filename);
char *make_segment_from_url(char *segment);
char *make_complete_url(char *original_url, char *segment);
int ffmpeg_merge(char *folder_name, char *output_file);

#endif

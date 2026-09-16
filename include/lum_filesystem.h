#ifndef Lum_Filesystem_H
#define Lum_Filesystem_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct lum_Binary_file {
  unsigned char* data;
  size_t size;
} lum_Binary_file;

char* lum_fs_read(const char* filename);
lum_Binary_file lum_fs_read_binary(const char* filename);
int lum_fs_write(const char* filename, const char* text);
int lum_fs_write_binary(const char* filename, const unsigned char* data, size_t size);

bool lum_fs_exists(const char* path);
bool lum_fs_is_file(const char* path);
bool lum_fs_is_directory(const char* path);

uint64_t lum_fs_file_size(const char* path);
uint64_t lum_fs_file_mtime(const char* path);

#endif //Lum_Filesystem_H

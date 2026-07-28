#include "lum_filesystem.h"
#include "lum_log.h"
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

static unsigned char* lum__read(const char* filename, size_t* size, bool null_terminate) {
  assert(filename);
  
  FILE* fp = fopen(filename, "rb");
  if (!fp) {
    Lum_Log_Error("Failed to open file: %s", filename);
    return NULL;
  }

  if (fseek(fp, 0, SEEK_END) != 0) goto failure;
  long file_length = ftell(fp);
  if (file_length < 0) goto failure;
  if (fseek(fp, 0, SEEK_SET) != 0) goto failure;

  unsigned char* buffer = (unsigned char*)malloc(null_terminate ? file_length + 1 : file_length);
  if (!buffer) {
    *size = 0;
    fclose(fp);
    return NULL;
  }

  fread(buffer, 1, file_length, fp);
  if(null_terminate) buffer[file_length] = '\0';
  fclose(fp);
  *size = file_length;

  return buffer;
failure:
  fclose(fp);
  *size = 0;
  return NULL;
}

static int lum__stat(const char* path, struct stat* st) {
  assert(path && st);
  return stat(path, st) == 0;
}

char* lum_fs_read(const char* filename) {
  size_t sz = 0;
  return lum__read(filename, &sz, true);
}

lum_Binary_file lum_fs_read_binary(const char* filename) {
  size_t sz = 0;
  lum_Binary_file file = {NULL, 0};
  unsigned char* data = lum__read(filename, &sz, false);
  if (data) {
    file.size = sz;
    file.data = data;
  }
  
  return file;
}

int lum_fs_write_binary(const char* filename, const unsigned char* data, size_t size) {
  assert(filename);
  if (size > 0 && !data) return 0;

  FILE* fp = fopen(filename, "wb");
  if (!fp) {
    Lum_Log_Error("Failed to open file: %s", filename);
    return 0;
  }

  if (size > 0) {
    size_t written = fwrite(data, 1, size, fp);
    if (written != size) {
      Lum_Log_Error("Failed to write file: %s", filename);
      fclose(fp);
      return 0;
    }
  }

  if (fclose(fp) != 0) {
    Lum_Log_Error("Failed to flush file: %s", filename);
    return 0;
  }
  return 1;
}

int lum_fs_write(const char* filename, const char* text) {
  assert(text);
  return lum_fs_write_binary(filename, text, strlen(text));
}

bool lum_fs_exists(const char* path) {
  struct stat st;
  return lum__stat(path, &st);
}

bool lum_fs_is_file(const char* path) {
  struct stat st;
  return lum__stat(path, &st) && (st.st_mode & S_IFMT) == S_IFREG;
}

bool lum_fs_is_directory(const char* path) {
  struct stat st;
  return lum__stat(path, &st) && (st.st_mode & S_IFMT) == S_IFDIR;
}

uint64_t lum_fs_file_size(const char* path) {
  struct stat st;
  return lum__stat(path, &st) ? st.st_size : 0;
}

uint64_t lum_fs_file_mtime(const char* path) {
  struct stat st;
  return lum__stat(path, &st) ? st.st_mtime : 0;
}
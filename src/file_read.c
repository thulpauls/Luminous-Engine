#include "file_read.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

char* lum_file_read(const char* filename) {
  assert(filename);
  
  FILE* fp = fopen(filename, "rb");
  if (!fp) {
    printf("Failed to open file: %s\n", filename);
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  long file_length = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char* buffer = (char*)malloc(file_length + 1);
  if (!buffer) {
    fclose(fp);
    return NULL;
  }

  fread(buffer, 1, file_length, fp);
  buffer[file_length] = '\0';
  fclose(fp);

  return buffer;
}

lum_Binary_file lum_file_read_binary(const char* filename) {
  assert(filename);
  
  lum_Binary_file file = {NULL, 0};
  FILE* fp = fopen(filename, "rb");
  if (!fp) {
    printf("Failed to open file: %s\n", filename);
    return file;
  }

  fseek(fp, 0, SEEK_END);
  long file_length = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  unsigned char* buffer = (unsigned char*)malloc(file_length);
  if (!buffer) {
    fclose(fp);
    return file;;
  }

  fread(buffer, 1, file_length, fp);
  fclose(fp);

  file.data = buffer;
  file.size = file_length;

  return file;
}

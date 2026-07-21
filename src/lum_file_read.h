#ifndef Lum_File_Read_H
#define Lum_File_Read_H

#include <stddef.h>

typedef struct lum_Binary_file {
  unsigned char* data;
  size_t size;
} lum_Binary_file;

char* lum_file_read(const char* filename);
lum_Binary_file lum_file_read_binary(const char* filename);

#endif //Lum_File_Read_H

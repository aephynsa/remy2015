#ifndef UTILITY_H
#define UTILITY_H

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

// print an error message and quit
void error(char const* message) {
  fprintf(stderr, "error: %s\n", message);
  exit(-1);
}

// basic self-resizing buffer with utility functions
typedef struct buffer_t {
  int size, len;
  unsigned char* data;
} buffer;

buffer* create_buffer(int size) {
  buffer* b = (buffer*)malloc(sizeof(buffer));
  b->size = size;
  b->len = 0;
  b->data = (unsigned char*)malloc(size * sizeof(unsigned char));
  return b;
}
void delete_buffer(buffer* b) {
  free(b->data);
  free(b);
}
void clear_buffer(buffer* b) {
  b->len = 0;
}
void append_buffer(buffer* b, unsigned char const* str, int len) {
  if (b->len + len > b->size) {
    unsigned char* temp = (unsigned char*)malloc(b->size * 2 * sizeof(unsigned char));
    memcpy(temp, b->data, b->len * sizeof(unsigned char));
    free(b->data);
    b->data = temp;
    b->size *= 2;
  }
  memcpy(b->data + b->len, str, len * sizeof(unsigned char));
  b->len += len;
}

#endif
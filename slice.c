/*
 * Author: Axel Viala
 * In 3I010 Systeme course at Université Pierre et Marie Curie
 * Year 2016 - 2017
 *
 * slice.c
 */

#define _POSIX_C_SOURCE 200809L


#include <stdint.h>
#include <stdbool.h>
#include <string.h> // strchr

#include "slice.h"

extern
inline
uint32_t slice_len(const str_slice slice) {
  return slice.len;
}

extern
inline
const char* slice_data(const str_slice s) {
  return s.data;
}

/*
 * Caller keeps responsiblity for lifetime of data.
 */
extern
inline
str_slice slice_new(const char* data, uint32_t len) {
  return (str_slice) { data, len };
}

extern
inline
str_slice slice_at(const char* data, const char c) {
  const char* end = strchr(data, c);
  uint32_t len = end - data;
  return slice_new(data, len);
}

extern
inline
str_slice slice_next(const str_slice s, const char c) {
  const char* data = slice_data(s) + slice_len(s) + 1; // skip c
  const char* end = strchr(data, c);
  if (end != NULL) {
    return slice_new(data, end - data);
  } else {
    return slice_new(NULL, 0);
  }
}

extern
inline
bool slice_empty(const str_slice s) {
  return s.len == 0;
}

#define _POSIX_C_SOURCE 200809L
#ifndef __SLICE_H
#define __SLICE_H

/*
 * Author: Axel Viala
 * In 3I010 Systeme course at Université Pierre et Marie Curie
 * Year 2016 - 2017
 *
 * slice.h
 */

/*
 * Little slice struct for char* null terminated.
 */
typedef struct _str_slice {
  const char* data;
  uint32_t len; // end - data len of slice.
} str_slice;

const char* slice_data(const str_slice s);
uint32_t slice_len(const str_slice slice);
str_slice slice_new(const char* data, const uint32_t len);
str_slice slice_at(const char* data, const char c);
str_slice slice_next(const str_slice slice, const char c);
bool slice_empty(const str_slice s);

#endif // __SLICE_H

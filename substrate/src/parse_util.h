/* The flat `key: value` grammar both tracks' workload files use: one pair a
 * line, `#` starts a comment, surrounding whitespace ignored. Internal to the
 * substrate's parsers. */
#ifndef GDS_PARSE_UTIL_H
#define GDS_PARSE_UTIL_H

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Trims in place; returns the start of the trimmed text. */
static inline char* gds_trim(char* s) {
  while (*s && isspace((unsigned char)*s)) ++s;
  char* e = s + strlen(s);
  while (e > s && isspace((unsigned char)e[-1])) --e;
  *e = '\0';
  return s;
}

static inline bool gds_parse_u64(const char* v, uint64_t* out) {
  char* end = NULL;
  errno = 0;
  const unsigned long long x = strtoull(v, &end, 10);
  if (end == v || *end != '\0') return false;
  *out = x;
  return true;
}

static inline bool gds_parse_double(const char* v, double* out) {
  char* end = NULL;
  const double x = strtod(v, &end);
  if (end == v || *end != '\0') return false;
  *out = x;
  return true;
}

/* Reads a whole file into a fresh NUL-terminated buffer, or NULL. */
static inline char* gds_read_file(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* buf = malloc((size_t)(n < 0 ? 0 : n) + 1);
  const size_t got = n > 0 ? fread(buf, 1, (size_t)n, f) : 0;
  buf[got] = '\0';
  fclose(f);
  return buf;
}

/* Splits the next line off *cursor (destructively), strips its comment and
 * surrounding whitespace, and splits it at the first colon. Returns 0 at the
 * end of the text, 1 for a blank line, 2 for a key and value, and -1 for a
 * non-blank line with no colon. */
static inline int gds_next_pair(char** cursor, char** key, char** val) {
  char* line = *cursor;
  if (!line || !*line) {
    if (line && !*line) *cursor = NULL;
    return 0;
  }
  char* nl = strchr(line, '\n');
  if (nl) {
    *nl = '\0';
    *cursor = nl + 1;
  } else {
    *cursor = line + strlen(line);
    if (!**cursor) *cursor = NULL;
  }
  char* hash = strchr(line, '#');
  if (hash) *hash = '\0';
  line = gds_trim(line);
  if (!*line) return 1;
  char* colon = strchr(line, ':');
  if (!colon) return -1;
  *colon = '\0';
  *key = gds_trim(line);
  *val = gds_trim(colon + 1);
  return 2;
}

static inline void gds_copy_str(char* dst, size_t cap, const char* src) {
  snprintf(dst, cap, "%s", src);
}

#endif

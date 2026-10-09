#ifndef GDS_JSON_H
#define GDS_JSON_H

#include <stdio.h>

/* Writes s to out as the inside of a JSON string literal. */
void gds_json_write_escaped(FILE* out, const char* s);

#endif

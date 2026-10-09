#include "gds/json.h"

void gds_json_write_escaped(FILE* out, const char* s) {
  for (; *s; ++s) {
    const char c = *s;
    switch (c) {
      case '"': fputs("\\\"", out); break;
      case '\\': fputs("\\\\", out); break;
      case '\n': fputs("\\n", out); break;
      case '\r': fputs("\\r", out); break;
      case '\t': fputs("\\t", out); break;
      default:
        if ((unsigned char)c < 0x20) fprintf(out, "\\u%04x", c);
        else fputc(c, out);
    }
  }
}

#include "build_defs.h"

void termprint(const char* s)
{
  uint8_t len = strlen_P(s);
  char c;
  for (uint8_t i = 0; i < len; ++i) {
    c = pgm_read_byte_near(s + i);
    Serial.write(c);
  }
}


#include "typecast.h"
#include <string>

bool isNum(char c) {
  return c >= '0' && c <= '9';
}
float string2float(const char *s) {
  if (!s) {
    return 0;
  }
  if (!isNum(s[0]) && s[0] != '.') {
    return 0;
  }

  return std::stof(s);
}
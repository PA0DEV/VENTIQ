#pragma once
#include <stdint.h>

struct SemVer {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
};

// <0 wenn a<b, 0 wenn gleich, >0 wenn a>b
inline int compareSemVer(const SemVer &a, const SemVer &b) {
  if (a.major != b.major) return a.major < b.major ? -1 : 1;
  if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
  if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
  return 0;
}

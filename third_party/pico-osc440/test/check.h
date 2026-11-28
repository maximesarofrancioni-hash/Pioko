#pragma once

// Minimal test helper shared by the host test programs.

#include <cstdio>

namespace test {

inline int& failures() {
  static int n = 0;
  return n;
}

inline void check(bool ok, const char* what, double value) {
  std::printf("%-60s %12.6g  %s\n", what, value, ok ? "OK" : "FAIL");
  if (!ok) ++failures();
}

// For checks whose result is a message rather than a number
inline void check_msg(bool ok, const char* what, const char* detail) {
  std::printf("%-60s %s  %s\n", what, detail, ok ? "OK" : "FAIL");
  if (!ok) ++failures();
}

inline int finish(const char* suite) {
  std::printf("%s: %s\n\n", suite, failures() ? "SOME TESTS FAILED" : "all passed");
  return failures() ? 1 : 0;
}

}  // namespace test

#include "dsp/buffer.h"

#include "config/platform.h"

namespace dsp {

// GCC would turn this loop into a memset() call: the attribute keeps it a loop
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("no-tree-loop-distribute-patterns")))
#endif
void AUDIO_HOT(clear)(float* dst, size_t n) {
  size_t i = 0;
  for (; i + 4 <= n; i += 4) {   // four stores per loop pass
    dst[i] = 0.0f;
    dst[i + 1] = 0.0f;
    dst[i + 2] = 0.0f;
    dst[i + 3] = 0.0f;
  }
  for (; i < n; ++i) dst[i] = 0.0f;
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("no-tree-loop-distribute-patterns")))
#endif
void AUDIO_HOT(copy)(float* dst, const float* src, size_t n) {
  size_t i = 0;
  for (; i + 4 <= n; i += 4) {
    dst[i] = src[i];
    dst[i + 1] = src[i + 1];
    dst[i + 2] = src[i + 2];
    dst[i + 3] = src[i + 3];
  }
  for (; i < n; ++i) dst[i] = src[i];
}

}  // namespace dsp

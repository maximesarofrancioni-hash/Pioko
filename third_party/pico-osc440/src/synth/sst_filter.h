#pragma once

#include <cstddef>

namespace synth {

// One filter of the Surge Synth Team's sst-filters for a voice: any of the models in sst_filter.cpp
// (the curated list filter-lab toured), at the voice's sample rate. Builds only with OSC440_SST_FILTERS,
// with sst-filters, sst-basic-blocks and simde on the include path; the sst headers stay in the .cpp.
class SstFilter {
 public:
  static int models();
  static const char* model_name(int model);

  SstFilter();
  void set_model(int model);                 // resets the state when it changes
  // cutoff as a MIDI note (69 = 440 Hz), resonance 0..1; ramped over the next block
  void set(float note, float resonance);
  void process_block(float* x, size_t n);    // in place
  void reset();

 private:
  int unit_ = -1;     // the sst state, in sst_filter.cpp's pool
  int model_ = -1;
};

}  // namespace synth

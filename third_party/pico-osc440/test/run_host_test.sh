#!/bin/sh
# Builds the synth for the host, runs the unit tests, renders test notes, checks them.
set -e
cd "$(dirname "$0")/.."
mkdir -p build-host
SYNTH="src/dsp/buffer.cpp src/dsp/interpolate.cpp src/synth/saw_osc.cpp src/synth/saw_tables.cpp src/synth/adsr.cpp
       src/synth/wavetable_osc.cpp src/synth/wavetable_data.cpp src/synth/wavetable_plaits.cpp
       src/synth/oscillator.cpp src/synth/dadsr.cpp src/synth/poly_group.cpp
       src/synth/mseg.cpp src/synth/lfo.cpp src/synth/triple_wave_group.cpp
       src/engine/voice_allocator.cpp src/engine/voice_engine.cpp"
FLAGS="-std=c++17 -O2 -Wall -Wextra -Isrc -Itest -pthread"

for t in unit_tests voice_tests osc_tests wavetable_tests engine_tests preset_tests; do
  g++ $FLAGS test/$t.cpp $SYNTH src/preset/preset_json.cpp -o build-host/$t
  build-host/$t
done

g++ $FLAGS test/host_render.cpp $SYNTH -o build-host/host_render
build-host/host_render build-host > /dev/null
python3 test/check_wav.py build-host

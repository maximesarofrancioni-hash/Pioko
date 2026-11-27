#!/bin/sh
# Browser build of filter-lab: one self-contained web/filter-lab.html with
#   - the engine (filter-lab + sst-filters, and osc440's TripleWaveGroup on the wave lane) -> filterlab.wasm
#   - the open-mpk4 display (firmware gfx.c + boot, filter-lab screens in screen/fl_ui.c) -> screen.wasm
#   - the SP-303 Vinyl Sim JS engine (vinyl_engine.js, vinyl_data.json)
#   - osc440's presets (../osc440bridge/presets.json, made by ../osc440bridge/dump_presets.cpp)
#   - Mutable Instruments Beads, Marbles, Tides 2 (pichenettes/eurorack + stmlib, MIT) and the shift register (mods/)
# LIBS_DIR: sst-filters, sst-basic-blocks, simde    MPK4: open-mpk4 checkout    OSC440: pico-osc440 checkout
# MI: pichenettes/eurorack checkout with stmlib cloned into it
set -e
L=${LIBS_DIR:-../libs}
F=${MPK4:-../../open-mpk4}/firmware/src
O=${OSC440:-../../pico-osc440}/src
E=${MI:-../../eurorack}
MSRC="$E/marbles/random/t_generator.cc $E/marbles/random/x_y_generator.cc $E/marbles/random/output_channel.cc
      $E/marbles/random/lag_processor.cc $E/marbles/random/quantizer.cc $E/marbles/random/discrete_distribution_quantizer.cc
      $E/marbles/ramp/ramp_extractor.cc $E/marbles/resources.cc $E/tides2/poly_slope_generator.cc $E/tides2/resources.cc
      $E/beads/dsp/processor.cc $E/beads/dsp/delay_clock.cc $E/beads/dsp/delay_controller.cc $E/beads/dsp/mu_law.cc
      $E/beads/dsp/trigger_generator.cc $E/beads/resources.cc $E/stmlib/dsp/units.cc $E/stmlib/dsp/atan.cc $E/stmlib/utils/random.cc"
OSRC="$O/dsp/buffer.cpp $O/dsp/interpolate.cpp $O/synth/saw_osc.cpp $O/synth/saw_tables.cpp $O/synth/adsr.cpp
      $O/synth/wavetable_osc.cpp $O/synth/wavetable_data.cpp $O/synth/wavetable_plaits.cpp $O/synth/oscillator.cpp
      $O/synth/dadsr.cpp $O/synth/mseg.cpp $O/synth/lfo.cpp $O/synth/triple_wave_group.cpp
      $O/engine/voice_allocator.cpp $O/engine/voice_engine.cpp"
# -Iwave first: wave/config/audio_config.h runs osc440 at 44.1 kHz instead of 48 kHz
emcc -std=c++20 -O3 -msimd128 -msse4.1 -fno-exceptions -Wno-deprecated-enum-float-conversion \
  -DTEST -Iwave -I$O -I$E -Imods -I$L/sst-filters/include -I$L/sst-basic-blocks/include -I$L/simde \
  wasm_api.cpp wave/wave_lane.cpp mods/mods.cpp ../src/filterlab.cpp $OSRC $MSRC -o filterlab.wasm \
  --no-entry -sSTANDALONE_WASM -sTOTAL_STACK=262144 -sINITIAL_MEMORY=33554432
# the display: 150 x 112, shown at 4x on the 600 x 450 AMOLED (600 x 448). A copy of board.h and gfx.c with that size.
mkdir -p screen/gen
sed -e 's/^#define LCD_W 160/#define LCD_W 150/' -e 's/^#define LCD_H 80/#define LCD_H 112/' $F/board.h > screen/gen/board.h
cp $F/gfx.c screen/gen/gfx.c
emcc -O2 -std=gnu11 -Iscreen/gen -I$F -I$F/ui screen/fl_ui.c screen/gen/gfx.c $F/ui/ui_boot.c -o screen.wasm \
  --no-entry -sSTANDALONE_WASM -sINITIAL_MEMORY=1048576 -sTOTAL_STACK=65536
python3 - <<'PY'
import base64, gzip
z = lambda f: base64.b64encode(gzip.compress(open(f, 'rb').read(), 9)).decode()
s = open('page.html').read().replace('__WASM__', z('filterlab.wasm')).replace('__SCR__', z('screen.wasm'))
s = s.replace('__VINYL_SRC__', open('vinyl_engine.js').read()).replace('__VINYL_DATA__', open('vinyl_data.json').read())
s = s.replace('__WPRESETS__', open('../osc440bridge/presets.json').read().strip())
open('filter-lab.html', 'w').write(s)
PY
echo "wrote web/filter-lab.html"

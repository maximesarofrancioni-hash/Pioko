# Host build of Pyoko's engine (core/), at 48 kHz like the Pi 4 kernel: render a WAV to check a change before
# flashing.  make -C host && ./host/render out.wav 16 lofi
# LIBS_DIR: sst-filters, sst-basic-blocks, simde    MI: pichenettes/eurorack with stmlib cloned into it
LIBS_DIR ?= ../libs
MI ?= ../libs/eurorack
O = ../third_party/pico-osc440/src
CXXFLAGS = -std=c++20 -O2 -fno-exceptions -DTEST -DOSC440_SST_FILTERS -Wno-deprecated-enum-float-conversion \
	-I../core -I$(O) -I$(MI) -I$(LIBS_DIR)/sst-filters/include -I$(LIBS_DIR)/sst-basic-blocks/include -I$(LIBS_DIR)/simde
ifeq ($(shell uname -m),x86_64)
CXXFLAGS += -msse4.1
endif

CORE = ../core/engine.cpp ../core/scenes.cpp ../core/wave_lane.cpp ../core/mods/mods.cpp ../core/sampler.cpp
OSC = $(O)/synth/sst_filter.cpp $(O)/dsp/buffer.cpp $(O)/dsp/interpolate.cpp $(O)/synth/saw_osc.cpp \
	$(O)/synth/saw_tables.cpp $(O)/synth/adsr.cpp $(O)/synth/wavetable_osc.cpp $(O)/synth/wavetable_data.cpp \
	$(O)/synth/wavetable_plaits.cpp $(O)/synth/oscillator.cpp $(O)/synth/dadsr.cpp $(O)/synth/mseg.cpp \
	$(O)/synth/lfo.cpp $(O)/synth/triple_wave_group.cpp $(O)/engine/voice_allocator.cpp $(O)/engine/voice_engine.cpp
MIS = $(MI)/marbles/random/t_generator.cc $(MI)/marbles/random/x_y_generator.cc $(MI)/marbles/random/output_channel.cc \
	$(MI)/marbles/random/lag_processor.cc $(MI)/marbles/random/quantizer.cc \
	$(MI)/marbles/random/discrete_distribution_quantizer.cc $(MI)/marbles/ramp/ramp_extractor.cc \
	$(MI)/marbles/resources.cc $(MI)/tides2/poly_slope_generator.cc $(MI)/tides2/resources.cc \
	$(MI)/beads/dsp/processor.cc $(MI)/beads/dsp/delay_clock.cc $(MI)/beads/dsp/delay_controller.cc \
	$(MI)/beads/dsp/mu_law.cc $(MI)/beads/dsp/trigger_generator.cc $(MI)/beads/resources.cc \
	$(MI)/stmlib/dsp/units.cc $(MI)/stmlib/dsp/atan.cc $(MI)/stmlib/utils/random.cc

render: render.cpp $(CORE) $(wildcard ../core/*.h ../core/mods/*.h)
	$(CXX) $(CXXFLAGS) render.cpp $(CORE) $(OSC) $(MIS) -o $@

clean:
	rm -f render

#
# Pyoko's Pi 4 kernel (Circle, AArch64): the engine in ../core, osc440 (../third_party/pico-osc440, with
# sst-filters) and Mutable Instruments Marbles / Tides 2 / Beads (eurorack), played from kernel.cpp.
#

CIRCLEHOME ?= ../circle
LIBS_DIR   ?= ../libs
MI         ?= $(LIBS_DIR)/eurorack
O          := ../third_party/pico-osc440/src

OBJS	= main.o kernel.o $(ENGINE_OBJS)

LIBS	= $(CIRCLEHOME)/lib/sound/libsound.a \
	  $(CIRCLEHOME)/lib/libcircle.a

# kernel.cpp only sees core/engine.h and core/scenes.h, plain C / C++ declarations
EXTRAINCLUDE = -I ../core

include $(CIRCLEHOME)/Rules.mk

# The engine uses header-only parts of the C++ standard library (<cmath>, <algorithm>, <array>...), so it is built
# hosted, with the toolchain's libstdc++ headers. Nothing from libstdc++ is linked: only libm, which Circle links
# by default (STDLIB_SUPPORT=1). Own flag set: Circle's include dirs are left out (addon/linux shadows errno.h)
# and -ffreestanding / -nostdinc++ are dropped. No exceptions or RTTI. TEST: stmlib's portable code paths.
ENGINE_FLAGS = $(ARCH) $(WARNINGS) -fsigned-char -g $(OPTIMIZE) \
	-D__circle__=$(CIRCLEVER) -DRASPPI=$(RASPPI) -DTEST -DOSC440_SST_FILTERS \
	-fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-math-errno \
	-fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	-std=gnu++20 -I ../core -I $(O) -I $(MI) \
	-I $(LIBS_DIR)/sst-filters/include -I $(LIBS_DIR)/sst-basic-blocks/include -I $(LIBS_DIR)/simde \
	-Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-psabi \
	-Wno-deprecated-enum-float-conversion -Wno-volatile

CORE_SRC = engine.cpp scenes.cpp wave_lane.cpp mods/mods.cpp sampler.cpp
OSC_SRC  = synth/sst_filter.cpp dsp/buffer.cpp dsp/interpolate.cpp synth/saw_osc.cpp synth/saw_tables.cpp \
	   synth/adsr.cpp synth/wavetable_osc.cpp synth/wavetable_data.cpp synth/wavetable_plaits.cpp \
	   synth/oscillator.cpp synth/dadsr.cpp synth/mseg.cpp synth/lfo.cpp synth/triple_wave_group.cpp \
	   engine/voice_allocator.cpp engine/voice_engine.cpp
MI_SRC   = marbles/random/t_generator.cc marbles/random/x_y_generator.cc marbles/random/output_channel.cc \
	   marbles/random/lag_processor.cc marbles/random/quantizer.cc \
	   marbles/random/discrete_distribution_quantizer.cc marbles/ramp/ramp_extractor.cc marbles/resources.cc \
	   tides2/poly_slope_generator.cc tides2/resources.cc beads/dsp/processor.cc beads/dsp/delay_clock.cc \
	   beads/dsp/delay_controller.cc beads/dsp/mu_law.cc beads/dsp/trigger_generator.cc beads/resources.cc \
	   stmlib/dsp/units.cc stmlib/dsp/atan.cc stmlib/utils/random.cc

# objects under build/, mirroring their sources (eurorack has several resources.cc)
ENGINE_OBJS = $(CORE_SRC:%.cpp=build/core/%.o) $(OSC_SRC:%.cpp=build/osc440/%.o) $(MI_SRC:%.cc=build/mi/%.o)

build/core/%.o: ../core/%.cpp
	@echo "  CPP   $@"
	@mkdir -p $(@D)
	@$(CPP) $(ENGINE_FLAGS) -c -o $@ $<

build/osc440/%.o: $(O)/%.cpp
	@echo "  CPP   $@"
	@mkdir -p $(@D)
	@$(CPP) $(ENGINE_FLAGS) -c -o $@ $<

build/mi/%.o: $(MI)/%.cc
	@echo "  CPP   $@"
	@mkdir -p $(@D)
	@$(CPP) $(ENGINE_FLAGS) -c -o $@ $<

-include $(DEPS)

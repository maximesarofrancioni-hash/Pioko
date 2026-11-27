#
# filter-lab Makefile (Circle, Raspberry Pi 4, AArch64)
#

CIRCLEHOME ?= ../circle
LIBS_DIR   ?= ../libs

OBJS	= main.o kernel.o filterlab.o

LIBS	= $(CIRCLEHOME)/lib/sound/libsound.a \
	  $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

# filterlab.cpp is the only file that sees sst-filters. It uses header-only
# parts of the C++ standard library (<cmath>, <algorithm>, <array>...), so it
# is built hosted, with the toolchain's libstdc++ headers. Nothing from libstdc++ is
# linked: only libm, which Circle links by default (STDLIB_SUPPORT=1).
SST_INC = -I $(LIBS_DIR)/sst-filters/include \
	  -I $(LIBS_DIR)/sst-basic-blocks/include \
	  -I $(LIBS_DIR)/simde

# Own flag set: Circle's include dirs are left out (addon/linux shadows errno.h)
# and -ffreestanding / -nostdinc++ are dropped. No exceptions or RTTI.
FILTERLAB_FLAGS = $(ARCH) $(WARNINGS) -fsigned-char -g $(OPTIMIZE) \
	-D__circle__=$(CIRCLEVER) -DRASPPI=$(RASPPI) \
	-fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-math-errno \
	-fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	-std=c++20 $(SST_INC) \
	-Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-psabi

filterlab.o filterlab.d: CPPFLAGS := $(FILTERLAB_FLAGS)

-include $(DEPS)

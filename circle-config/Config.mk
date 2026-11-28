# Copied to circle/Config.mk by the build workflow
AARCH = 64
RASPPI = 4
PREFIX64 = aarch64-none-elf-
STDLIB_SUPPORT = 1
OPTIMIZE = -O3
# the engine's data (osc440's wavetables, the sst filter pool, Beads' buffers) does not fit Circle's default 2 MB
# kernel image + BSS; Circle halts at boot if it overflows, so the build prints the size (.github/workflows/build.yml)
DEFINE += -DKERNEL_MAX_SIZE=0x2000000

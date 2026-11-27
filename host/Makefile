LIBS_DIR ?= ../libs
CXXFLAGS = -std=c++20 -O2 -I$(LIBS_DIR)/sst-filters/include -I$(LIBS_DIR)/sst-basic-blocks/include -I$(LIBS_DIR)/simde
ifeq ($(shell uname -m),x86_64)
CXXFLAGS += -msse4.1
endif

render: render.cpp ../src/filterlab.cpp ../src/filterlab.h ../src/euclid.h
	$(CXX) $(CXXFLAGS) render.cpp ../src/filterlab.cpp -o $@

clean:
	rm -f render

CXX       ?= $(which gcc)
CXXVER    ?= 17
CXXDBG    ?= 0
CXXOPT    ?= 2
CXXFLAGS   = -c -Wall -Wextra -Wpedantic -std=c++$(CXXVER) -O$(CXXOPT) -g$(CXXDBG)
LDFLAGS   ?=
SOURCES    = $(wildcard *.cpp)
OBJECTS    = $(SOURCES:.cpp=.o)
OUTPUT     = $(shell basename $(CURDIR))

all: $(SOURCES) $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $@

.PHONY: clean

clean:
	rm -f $(OBJECTS) $(OUTPUT)


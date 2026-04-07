CXX ?= g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Isrc
LDFLAGS =
EXT =

ifdef OS
  ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32
    EXT = .exe
  endif
endif

sl$(EXT): src/main.cpp src/types.h src/lexer.h src/parser.h src/interp.h
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp $(LDFLAGS)

clean:
	rm -f sl sl.exe

.PHONY: clean

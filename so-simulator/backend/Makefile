CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread
INCLUDES = -Iinclude -Isrc
SRCS = $(wildcard src/*.cpp)
OUTDIR = build
BIN = $(OUTDIR)/sosim

.PHONY: all clean

all: $(BIN)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(BIN): $(SRCS) | $(OUTDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRCS) -o $(BIN)

clean:
	rm -rf $(OUTDIR)

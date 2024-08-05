IDIR=src
CC=g++
CC_C=gcc
WCC=i686-w64-mingw32-g++
WCC_C=i686-w64-mingw32-gcc
CFLAGS=-I$(IDIR) -Wall -fmax-errors=1 -std=c++11
CFLAGS_C=-I$(IDIR) -Wall -fmax-errors=1 `pkg-config --cflags libpipewire-0.3`
WCFLAGS=-D_GLIBCXX_USE_NANOSLEEP -static-libgcc -static-libstdc++ -static -lpthread
WCFLAGS_C=-static-libgcc

BDIR=bin
SDIR=src
ODIR=obj
LDIR=lib

LIBS_L=-lpthread `pkg-config --libs libpipewire-0.3`
LIBS_W=-lwinmm

_DEPS = log.hpp
_DEPS_C = audio.h dsp.h helpers.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))
DEPS_C = $(patsubst %,$(IDIR)/%,$(_DEPS_C))

_OBJ = main.o log.o
_OBJ_C = audio.o dsp.o helpers.o
OBJ_L = $(patsubst %,$(ODIR)/Linux/cpp/%,$(_OBJ))
OBJ_W = $(patsubst %,$(ODIR)/Windows/cpp/%,$(_OBJ))
OBJ_L_C = $(patsubst %,$(ODIR)/Linux/c/%,$(_OBJ_C))
OBJ_W_C = $(patsubst %,$(ODIR)/Windows/c/%,$(_OBJ_C))


$(ODIR)/Linux/cpp/%.o: $(SDIR)/%.cpp $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< -g -rdynamic $(CFLAGS)

$(ODIR)/Windows/cpp/%.o: $(SDIR)/%.cpp $(DEPS)
	@mkdir -p $(@D)
	$(WCC) -c -o $@ $< -g $(CFLAGS)

$(ODIR)/Linux/c/%.o: $(SDIR)/%.c $(DEPS_C)
	@mkdir -p $(@D)
	$(CC_C) -c -o $@ $< -g -rdynamic $(CFLAGS_C)

$(ODIR)/Windows/c/%.o: $(SDIR)/%.c $(DEPS_C)
	@mkdir -p $(@D)
	$(WCC_C) -c -o $@ $< -g $(CFLAGS_C)

linux: $(OBJ_L_C) $(OBJ_L)
	@mkdir -p $(BDIR)/Linux
	g++ -o $(BDIR)/Linux/Test $^ -g -rdynamic $(CFLAGS) $(LIBS_L)

windows: $(OBJ_W_C) $(OBJ_W)
	@mkdir -p $(BDIR)/Linux
	i686-w64-mingw32-g++ -o $(BDIR)/Windows/Test.exe $^ -g $(CFLAGS) $(WCFLAGS) $(LIBS_W)

all: linux windows

.PHONY: clean runl runw rundl rundw

clean:
	rm -f $(ODIR)/Linux/c/*.o $(ODIR)/Windows/c/*.o $(ODIR)/Linux/cpp/*.o $(ODIR)/Windows/cpp/*.o *~ core $(INCDIR)/*~

runl:
	./bin/Linux/Test

runw:
	./bin/Windows/Test.exe

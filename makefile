IDIR=include
CC=g++
WCC=i686-w64-mingw32-g++
WRC=i686-w64-mingw32-windres $(SDIR)/resources.rc -O coff
RCFLAGS=-I $(IDIR)
CFLAGS=-I$(IDIR) -Wall -std=c++17
WCFLAGS=-D_GLIBCXX_USE_NANOSLEEP -static-libgcc -static-libstdc++ -static -lpthread

BDIR=bin
SDIR=src
ODIR=obj
LDIR=lib

LIBS_L=-lpthread
LIBS_W=

_DEPS =
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o log.o
OBJ_LD = $(patsubst %,$(ODIR)/Linux/Debug/%,$(_OBJ))
OBJ_WD = $(patsubst %,$(ODIR)/Windows/Debug/%,$(_OBJ))
OBJ_L = $(patsubst %,$(ODIR)/Linux/Release/%,$(_OBJ))
OBJ_W = $(patsubst %,$(ODIR)/Windows/Release/%,$(_OBJ))


$(ODIR)/Linux/Debug/%.o: $(SDIR)/%.cpp $(DEPS)
	$(CC) -c -o $@ $< -g -rdynamic $(CFLAGS)

$(ODIR)/Windows/Debug/%.o: $(SDIR)/%.cpp $(DEPS)
	$(WCC) -c -o $@ $< -g $(CFLAGS)

$(ODIR)/Linux/Release/%.o: $(SDIR)/%.cpp $(DEPS)
	$(CC) -c -o $@ $< -O3 $(CFLAGS) -DNDEBUG

$(ODIR)/Windows/Release/%.o: $(SDIR)/%.cpp $(DEPS)
	$(WCC) -c -o $@ $< -O3 $(CFLAGS) -DNDEBUG

debugl: $(OBJ_LD)
	g++ -o $(BDIR)/Linux/Debug/Test $^ -g -rdynamic $(CFLAGS) $(LIBS_L)

debugw: $(OBJ_WD)
	i686-w64-mingw32-g++ -o $(BDIR)/Windows/Debug/Test.exe $^ -g $(CFLAGS) $(WCFLAGS) $(LIBS_W)

debug: debugl debugw

releasel: $(OBJ_L)
	g++ -o $(BDIR)/Linux/Release/Arcade $^ $(CFLAGS) $(LIBS_L)

releasew: $(OBJ_W) $(ODIR)/Windows/resources.o
	i686-w64-mingw32-g++ -o $(BDIR)/Windows/Release/Arcade.exe $^ $(CFLAGS) $(WCFLAGS) $(LIBS_W)

release: releasel releasew


.PHONY: clean runl runw rundl rundw

clean:
	rm -f $(ODIR)/Linux/Debug/*.o $(ODIR)/Windows/Debug/*.o $(ODIR)/Linux/Release/*.o $(ODIR)/Windows/Release/*.o *~ core $(INCDIR)/*~

runl:
	./bin/Linux/Release/Test

runw:
	./bin/Windows/Release/Test.exe

rundl:
	./bin/Linux/Debug/Test

rundw:
	./bin/Windows/Debug/Test.exe

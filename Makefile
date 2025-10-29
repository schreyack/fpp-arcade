SRCDIR ?= /opt/fpp/src
include $(SRCDIR)/makefiles/common/setup.mk
include $(SRCDIR)/makefiles/platform/*.mk

all: libfpp-arcade2.$(SHLIB_EXT)
debug: all

CFLAGS+=-I.
OBJECTS_fpp_arcade_so += src/FPPArcade.o src/FPPTetris.o src/FPPPong.o src/FPPSnake.o src/FPPBreakout.o src/FPPPacman.o
LIBS_fpp_arcade_so += -L$(SRCDIR) -lfpp -ljsoncpp -lhttpserver -lSDL2
CXXFLAGS_src/FPPArcade.o += -I$(SRCDIR)


%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-arcade2.$(SHLIB_EXT): $(OBJECTS_fpp_arcade_so) $(SRCDIR)/libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_arcade_so) $(LIBS_fpp_arcade_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-arcade2.so $(OBJECTS_fpp_arcade_so)


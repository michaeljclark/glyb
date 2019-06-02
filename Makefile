CFLAGS      = -Wall -g -pg -O2 -Isrc

test_srcs   = $(wildcard tests/*.cc)
test_objs   = $(patsubst tests/%.cc,build/obj/%.o,$(test_srcs))
test_progs  = $(patsubst tests/%.cc,build/bin/%,$(test_srcs))

FT_LIBS     = $(shell pkg-config --libs freetype2)
FT_INCS     = $(shell pkg-config --cflags freetype2)

HB_LIBS     = $(shell pkg-config --libs harfbuzz)
HB_INCS     = $(shell pkg-config --cflags harfbuzz)

CFLAGS      += $(FT_INCS) $(HB_INCS)
LIBS        += $(FT_LIBS) $(HB_LIBS)

COMMON_HDRS = $(wildcard src/*.h)

COMMON_OBJS = build/obj/binpack.o \
              build/obj/font.o \
              build/obj/glyph.o \
              build/obj/text.o \
              build/obj/utf8.o \
              build/obj/util.o

all: programs tests

programs: build/bin/glbinpack build/bin/glfont build/bin/glsimple build/bin/gllayout build/bin/ftrender build/bin/fontdb

tests: $(test_progs)

backup: clean
	tar czf ../$(shell basename $(shell pwd))-$(shell date '+%Y%m%d').tar.gz .

build/obj/%.o: src/%.cc $(COMMON_HDRS)
	@echo CXX $@ ; mkdir -p $(@D) ; $(CXX) $(CFLAGS) -c $< -o $@

build/obj/%.o: tests/%.cc $(COMMON_HDRS)
	@echo CXX $@ ; mkdir -p $(@D) ; $(CXX) $(CFLAGS) -c $< -o $@

build/obj/%.o: examples/%.cc $(COMMON_HDRS)
	@echo CXX $@ ; mkdir -p $(@D) ; $(CXX) $(CFLAGS) -c $< -o $@

build/bin/%: build/obj/%.o $(COMMON_OBJS)
	@echo LD $@ ; mkdir -p $(@D) ; $(CXX) $(CFLAGS) $^ -o $@ ${LIBS}

clean:
	rm -fr build gmon.out

build/bin/glfont: LIBS += -lGL -lglfw
build/bin/glsimple: LIBS += -lGL -lglfw
build/bin/gllayout: LIBS += -lGL -lglfw
build/bin/glbinpack: LIBS += -lGL -lglfw
build/obj/glfont.o: examples/glfont.cc examples/glcommon.h src/binpack.h
build/obj/glbinpack.o: examples/glbinpack.cc examples/glcommon.h src/binpack.h
build/obj/binpack.o: src/binpack.cc src/binpack.h
build/obj/glyph.o: src/glyph.cc src/glyph.h
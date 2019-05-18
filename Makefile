CFLAGS       = -Wall -g -O2 -Iglm -Isrc -pg

glbinpack_srcs    = src/glbinpack.cc src/binpack.cc
glbinpack_objs    = $(patsubst src/%.cc,build/obj/%.o,$(glbinpack_srcs))
glbinpack_prog    = build/bin/glbinpack

test_srcs         = $(wildcard tests/*.cc)
test_objs         = $(patsubst tests/%.cc,build/obj/%.o,$(test_srcs))
test_progs        = $(patsubst tests/%.cc,build/bin/%,$(test_srcs))

all: programs tests

programs: $(glbinpack_prog)

tests: $(test_progs)

backup: clean
	tar czf ../$(shell basename $(shell pwd))-$(shell date '+%Y%m%d').tar.gz .

build/obj/%.o: src/%.cc src/binpack.h
	@echo CXX $@ ; mkdir -p $(@D) ; $(CXX) $(CFLAGS) -c $< -o $@

build/obj/%.o: tests/%.cc src/binpack.h
	@echo CXX $@ ; mkdir -p $(@D) ; $(CXX) $(CFLAGS) -c $< -o $@

build/bin/%: build/obj/%.o build/obj/binpack.o
	@echo LD $@ ; mkdir -p $(@D) ; $(CXX) $(CFLAGS) $^ -o $@ ${LIBS}

clean:
	rm -fr build gmon.out

build/bin/glbinpack: LIBS = -lGL -lglfw
build/bin/pngbinpack: LIBS = -lGL -lglfw
build/obj/glbinpack.o: src/glbinpack.cc src/glbinpack.h src/binpack.h
build/obj/binpack.o: src/binpack.cc src/binpack.h
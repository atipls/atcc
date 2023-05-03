MODE ?= release
COMPILER := ../build/$(MODE)/atcc

SOURCES := source/editor.aa source/highlight.aa source/interop.aa source/main.aa source/util.aa

CFLAGS := -O0 -g -Wno-incompatible-library-redeclaration -Wno-incompatible-pointer-types

all: sample

run: sample
	./sample

sample: generated.c source/interop.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

generated.c: $(SOURCES)
	$(COMPILER) --config build.cnf

source/interop.o: source/interop.c
	$(CC) $(CFLAGS) -c -o $@ source/interop.c

clean:
	rm -f sample generated.c source/interop.o

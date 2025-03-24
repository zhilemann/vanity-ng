all: build/vanity-ng

build/vanity-ng: core/*.c *.c | core/*.h build/kernel.cl
	$(CC) $^ -Wall -Wno-missing-braces -lOpenCL -lm -O2 -march=native -o $@

build/kernel.cl: core/*.h core/*.c kernel.cl | build
	cat $^ | sed /#include/d | tr '\t' ' ' > $@

build:
	mkdir -p $@

all: build/vanitygen2

build/vanitygen2: core/*.h core/*.c build/kernel.cl *.c | build
	$(CC) $(filter %.c, $^) -Wall -Wno-missing-braces -lOpenCL -lm -O2 -march=native -o $@

build/kernel.cl: core/*.h core/*.c kernel.cl | build
	cat $^ | sed /#include/d | tr '\t' ' ' > $@

build:
	mkdir -p $@

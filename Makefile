all: build/vanitygen2

build/vanitygen2: core/*.h core/*.c build/kernel.cl *.c
	$(CC) $(filter %.c, $^) -Wall -lOpencl -lm -O2 -march=native -lOpenCL -o $@

build/kernel.cl: core/*.h core/*.c kernel.cl
	cat $^ | sed /#include/d | tr '\t' ' ' > $@

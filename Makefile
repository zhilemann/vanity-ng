all: build/vanitygen2

build/vanitygen2: core/* build/kernel.c *.c
	$(CC) $(filter %.c,$^) -g -O0 -march=native -lOpenCL -o $@

build/kernel.c: core/*.h core/*.c kernel.cl
	cat $^ | sed /#include/d | xxd -i -n KERNEL > $@

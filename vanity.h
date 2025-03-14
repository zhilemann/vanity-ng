#if !defined(VANITY_H)
#define VANITY_H

#include "core/core.h"
#include <CL/cl.h>
#include <stdio.h>

typedef const char* str;

typedef union PACKED {
	u8 d[]; u64 x;
} vanity_rng;

typedef struct {
	cl_mem R, S, F, L;
} vanity_mem;

typedef struct {
	cl_device_id id;
	cl_command_queue q;

	cl_program pr;
	cl_kernel ke;
	u32 cu, wg;

	vanity_mem buf;
} vanity_dev;

typedef struct {
	cl_context cl;
	u32 n; vanity_dev D[];
} vanity_ctx;

#define VANITY_LOG(...) fprintf(stderr, __VA_ARGS__)
#define VANITY_TMP(R, F, ...) sprintf( \
	R, "%s/vanitygen2" F, getenv("TMP") \
	__VA_OPT__(,) __VA_ARGS__)

#define ASSERT(x) __assert(__FILE__, __LINE__, #x, (u64)(x))
#define CL_ASSERT(x) __cl_assert(__FILE__, __LINE__, #x, x)

extern unsigned char KERNEL[];
extern unsigned int KERNEL_len;

void __assert(str fp, u32 ln, str ex, u64 x);
void __cl_assert(str fp, u32 ln, str ex, u32 x);

void vanity_tmp_init();
ed_lut* vanity_ed_lut();

vanity_ctx* vanity_open();
void vanity_close(vanity_ctx* V);
void vanity_build(vanity_ctx* V, str k);

void vanity_config(
    vanity_ctx* V,
    const vanity_filt* F,
    const void* L, u32 n);

void vanity_seed(vanity_rng* R);
void vanity_random(vanity_rng* Rng, u8* R, u32 n);

#endif

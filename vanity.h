#if !defined(VANITY_H)
#define VANITY_H

#include "core/core.h"
#include <CL/cl.h>
#include <stdio.h>

typedef const char* str;

typedef struct {
	cl_device_id id;
	cl_command_queue q;

	cl_kernel k;
	u32 cu, wg;
} vanity_dev;

typedef struct {
	cl_context cl;
	cl_program pr;

	u32 n; vanity_dev D[];
} vanity_ctx;

#define VANITY_LOG(...) fprintf(stderr, __VA_ARGS__)

#define ASSERT(x) __assert(__FILE__, __LINE__, #x, (u64)(x))
#define CL_ASSERT(x) __cl_assert(__FILE__, __LINE__, #x, x)

extern unsigned char KERNEL[];
extern unsigned int KERNEL_len;

void __assert(str fp, u32 ln, str ex, u64 x);
void __cl_assert(str fp, u32 ln, str ex, u32 x);

void bn_rand(bn_mut* R);

vanity_ctx* vanity_open();
void vanity_close(vanity_ctx* V);

secp_lut* vanity_secp_lut();
ed_lut* vanity_ed_lut();

void vanity_build(vanity_ctx* V, str ke);

#endif

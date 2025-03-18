#if !defined(VANITY_H)
#define VANITY_H

#include "core/core.h"
#include <CL/cl.h>
#include <stdio.h>

typedef const char* str;

typedef struct { u32 n; cl_mem d; } vanity_buf;

typedef struct {
	cl_device_id id;
	cl_command_queue q;

	cl_kernel k;
	vanity_buf R, S;
	u32 cu, wg;
} vanity_dev;

typedef struct {
	cl_context cl;
	cl_program pr;

	vanity_buf F, L;
	u32 n; vanity_dev D[];
} vanity_ctx;

#define __WHERE() __FILE__, __LINE__
#define VANITY_LOG(...) fprintf(stderr, __VA_ARGS__)

#define ASSERT(x) __assert(__WHERE(), #x, (u64)(x))
#define CL_ASSERT(x) __cl_assert(__WHERE(), #x, x)

extern unsigned char KERNEL[];
extern unsigned int KERNEL_len;

static const cl_mem_flags VANITY_IN =
	CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY;

static const cl_mem_flags VANITY_OUT =
	CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY;

void __assert(str fp, u32 ln, str ex, u64 x);
void __cl_assert(str fp, u32 ln, str ex, u32 x);

vanity_ctx* vanity_open();
void vanity_close(vanity_ctx* V);

void vanity_build(vanity_ctx* V, str ke);

void vanity_alloc(
	vanity_buf* R, vanity_ctx* V,
	cl_mem_flags f, u32 n);

cl_event vanity_read(
	void* R, vanity_dev* D,
	const vanity_buf* X, cl_event ev);

cl_event vanity_write(
	vanity_buf* R, vanity_dev* D,
	const void* X, cl_event ev);

cl_event vanity_dispatch2(
	vanity_dev* D,
	u32 gl_x, u32 gl_y,
	u32 lo_x, u32 lo_y,
	cl_event ev);

secp_lut* vanity_secp_lut(secp_lut_mul* Lm);
ed_lut* vanity_ed_lut();

#endif

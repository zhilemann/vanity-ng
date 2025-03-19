#if !defined(CL2_H)
#define CL2_H

#include "core/core.h"
#include <CL/cl.h>
#include <stdio.h>

typedef const char* str;

typedef struct { u32 n; cl_mem d; } cl2_buf;

typedef struct {
	cl_device_id id;
	cl_command_queue q;

	cl_kernel k;
	cl2_buf R, S;
	u32 cu, wg;
} cl2_dev;

typedef struct {
	cl_context cl;
	cl_program pr;

	cl2_buf F, L;
	u32 n; cl2_dev D[];
} cl2_ctx;

#define __WHERE() __FILE__, __LINE__
#define VANITY_LOG(...) fprintf(stderr, __VA_ARGS__)

#define ASSERT(x) __assert(__WHERE(), #x, (u64)(x))
#define CL_ASSERT(x) __cl_assert(__WHERE(), #x, x)

extern unsigned char KERNEL[];
extern unsigned int KERNEL_len;

static const cl_mem_flags CL2_IN =
	CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY;

static const cl_mem_flags CL2_OUT =
	CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY;

void __assert(str fp, u32 ln, str ex, u64 x);
void __cl_assert(str fp, u32 ln, str ex, u32 x);

cl2_ctx* cl2_open();
void cl2_close(cl2_ctx* V);

void cl2_build(cl2_ctx* V, str ke);

void cl2_alloc(
	cl2_buf* R, cl2_ctx* V,
	cl_mem_flags f, u32 n);

cl_event cl2_read(
	void* R, cl2_dev* D,
	const cl2_buf* X, cl_event ev);

cl_event cl2_write(
	cl2_buf* R, cl2_dev* D,
	const void* X, cl_event ev);

cl_event cl2_dispatch2(
	cl2_dev* D,
	u32 gl_x, u32 gl_y,
	u32 lo_x, u32 lo_y,
	cl_event ev);

secp_lut* vanity_secp_lut(secp_lut_mul* Lm);
ed_lut* vanity_ed_lut();

#endif

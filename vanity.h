#if !defined(VANITY_H)
#define VANITY_H

#include "core/core.h"
#include <string.h>
#include <CL/cl.h>
#include <stdio.h>

#if defined(WIN32)
	#define INCBIN_SEC ".rdata, \"dr\""
#else
	#define INCBIN_SEC ".rodata"
#endif

#define unreachable() __builtin_unreachable()

#define INCBIN(var, fp) \
	asm ( \
		".section " INCBIN_SEC ";" \
		".global " #var "_start;" \
		".global " #var "_end;" \
		#var "_start: .incbin \"" fp "\";" \
		#var "_end:" \
	); \
	extern const u8 var##_start[]; \
	extern const u8 var##_end[]

/////////////////////////////////////////////////

typedef const char* str;
typedef struct { u32 n; cl_mem d; } cl2_buf;

typedef struct {
	cl_device_id id;
	cl_command_queue q;

	cl2_buf R, S, F, L;
	cl_kernel k; u32 cu, wg;
} cl2_dev;

typedef struct {
	cl_context cl;
	cl_program pr;

	u32 n; cl2_dev D[];
} cl2_ctx;

#define __WHERE() __FILE__, __LINE__
#define VANITY_LOG(...) fprintf(stderr, __VA_ARGS__)

#define ASSERT(x) __assert(__WHERE(), #x, (u64)(x))
#define CL_ASSERT(x) __cl_assert(__WHERE(), #x, x)

static const cl_mem_flags CL2_IN =
	CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY;

static const cl_mem_flags CL2_OUT =
	CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY;

void __assert(str fp, u32 ln, str ex, u64 x);
void __cl_assert(str fp, u32 ln, str ex, u32 x);

void bn_rand(bn_mut* R);

void u8_print(const u8* X, u32 n);
void u8_print_bech32(const u8* X, u32 n);
void u8_print_base58(const u8* X, u32 n);

void bn_print(bn X);
void bn_print_base58(bn X, u32 z);

/////////////////////////////////////////////////

cl2_ctx* cl2_open();
void cl2_close(cl2_ctx* Cl);

void cl2_alloc(
	cl2_buf* R, cl2_ctx* Cl,
	cl_mem_flags f, u32 n);

cl_event cl2_read(
	void* R, cl2_dev* D,
	const cl2_buf* X, cl_event ev);

cl_event cl2_write(
	cl2_buf* R, cl2_dev* D,
	const void* X, cl_event ev);

void cl2_config(
	cl2_ctx* Cl, u32 s,
	const void* F, u32 f,
	const void* L, u32 l);

cl_event cl2_dispatch2(
	cl2_dev* D,
	u32 gl_x, u32 gl_y,
	u32 lo_x, u32 lo_y,
	cl_event ev);

void cl2_build(cl2_ctx* Cl, str ke);

/////////////////////////////////////////////////

secp_lut* vanity_secp_lut(secp_lut_mul* Lm);
ed_lut* vanity_ed_lut();

u64 u8_pat_eth(u8_pat* R, str pr, str su);
u64 u8_pat_bech32(u8_pat* R, str pr, str su);

u64 bn_filt58_init(bn_filt58* F, str pr, u32 z, str su);

#endif

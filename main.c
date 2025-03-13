#include "core/core.h"
#include <CL/cl.h>
#include <unistd.h>
#include <stdio.h>

typedef struct {
	cl_device_id id;
	cl_command_queue q;

	cl_kernel k;
	u32 cu, wg;
} vanity_dev;

typedef struct {
	cl_context cl;
	u32 n; vanity_dev D[];
} vanity_ctx;

extern unsigned char KERNEL[];
extern unsigned int KERNEL_len;

static const char HEX[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

#define VANITY_LOG(...) fprintf(stderr, __VA_ARGS__)
#define VANITY_TMP(R, F, ...) sprintf( \
	R, "%s/vanitygen2"F, getenv("TMP") \
	__VA_OPT__(,) __VA_ARGS__)

#define CL_ASSERT(r) __cl_assert(__FILE__, __LINE__, r)

static void __cl_assert(const char* f, u32 l, cl_int r) {
	if (r == CL_SUCCESS) return;
	VANITY_LOG("%s:%u: CL_ASSERT(0x%08x)", f, l, r);
	exit(r);
}

static void u64_hex(char* R, u64 x) {
	for (u32 i = 0; i < 8; i++) {
		u8 b = x >> (8*(7-i));
		R[2*i] = HEX[b/16], R[2*i+1] = HEX[b%16];
	}
}

static u64 vanity_fnv1a(u8* X, u32 n) {
	u64 h = 0xcbf29ce484222325;
	for (u32 i = 0; i < n; i++)
		h ^= X[i], h *= 0x100000001b3;

	return h;
}

static void vanity_tmp_init() {
	char t[PATH_MAX];
	VANITY_TMP(t, ""), mkdir(t);
}

/////////////////////////////////////////////////

static ed_lut* vanity_ed_lut() {
	ed_lut* L = malloc(sizeof(ed_lut));
	xy2d* P; xytz G = ED_G;

	char fp[PATH_MAX];
	VANITY_TMP(fp, "ed25519-lut.bin");

	FILE* F = fopen(fp, "rb");
	if (!F | !fread(L, sizeof(ed_lut), 1, F)) {
		VANITY_LOG("computing Ed25519 LUTs");
		for (u32 i = 0; i < 12; i++) {
			if (i < 8)
				ed_lut_step(L->a[i], &G, 1<<21);
			else
				ed_lut_step(L->b[i-8], &G, 1<<22);

			VANITY_LOG(".");
		}

		F = fopen(fp, "wb");
		fwrite(L, sizeof(ed_lut), 1, F);
		VANITY_LOG("done\n");
	}

	fclose(F); return L;
}

static xy* vanity_secp_lut() {
	xy* L = malloc(1024 * sizeof(xy));
	L[0] = SECP_G;

	for (u32 i = 0; i < 10; i++) {
		xy* P = L + (1<<i);
		secp_addN(P, P-1, L, 1<<i);
	}

	return L;
}

/////////////////////////////////////////////////

static u32 vanity_platform(cl_platform_id* R) {
	u32 n; clGetPlatformIDs(0, NULL, &n);
	if (n == 0) return 0;
	
	cl_platform_id P[n];
	CL_ASSERT(clGetPlatformIDs(n, P, NULL));

	cl_int t; u32 u, m = 0;
	for (u32 i = 0; i < n; i++) {
		t = clGetDeviceIDs(
			P[i], CL_DEVICE_TYPE_GPU, 0, NULL, &u);

		if (t != CL_DEVICE_NOT_FOUND) CL_ASSERT(t);
		if (m < u) m = u, *R = P[i];
	}

	if (m == 0) return 0;
	return m;
}

static vanity_ctx* vanity_init() {
	cl_platform_id P;
	u32 n = vanity_platform(&P);
	if (n == 0) return NULL;

	cl_device_id D[n];
	CL_ASSERT(clGetDeviceIDs(
		P, CL_DEVICE_TYPE_GPU, n, D, NULL));
	
	vanity_ctx* V = malloc(
		sizeof(vanity_ctx) +
		sizeof(vanity_dev) * n);

	cl_int e; cl_context cl;
	cl = clCreateContext(NULL, n, D, NULL, NULL, &e);
	CL_ASSERT(e);

	V->cl = cl; V->n = n;
	for (u32 i = 0; i < n; i++) {
		V->D[i].id = D[i];

		// 34-letter function name o_0
		V->D[i].q = clCreateCommandQueueWithProperties(
			cl, D[i], NULL, &e);

		CL_ASSERT(e);
	}

	return V;
}

/////////////////////////////////////////////////

typedef union PACKED {
	u8 d[];
	struct { u64 h; u8 n[64]; };
} vanity_hash;

static void vanity_kernel_id(char* R, cl_device_id D) {
	vanity_hash id; u64 n;

	id.h = vanity_fnv1a(KERNEL, KERNEL_len);
	CL_ASSERT(clGetDeviceInfo(
		D, CL_DEVICE_NAME, 64, id.n, &n));

	u64_hex(R, vanity_fnv1a(id.d, n+8));
}

static void vanity_build(vanity_ctx* V) {
	u8 d[64]; u64 n;

	char id[16], fp[PATH_MAX];
	for (u32 i = 0; i < V->n; i++) {
		cl_device_id D = V->D[i].id;

		vanity_kernel_id(id, D);
		VANITY_TMP(fp, "kernel-%.16s.bin", id);

		FILE* F = fopen(fp, "rb");
		if (!F) {
			cl_int r; n = KERNEL_len;

			const char* src = (char*)KERNEL;
			u64 len = KERNEL_len;

			cl_program p = clCreateProgramWithSource(
				V->cl, 1, &src, &len, &r);

			CL_ASSERT(r);
			printf("build ok\n");
		} else {
			
		}
	}
}


int main(int argc, char** argv) {
	vanity_tmp_init();

	vanity_ctx* V = vanity_init();
	if (V == NULL) {
		VANITY_LOG("vanity_init failed\n");
		return -1;
	}

	vanity_build(V);
	return 0;
}

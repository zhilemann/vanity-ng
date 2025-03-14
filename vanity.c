#include "vanity.h"
#include <CL/cl.h>
#include <unistd.h>
#include <time.h>

static const char HEX[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

static const u64 FNV1A_BASE = 0xcbf29ce484222325;
static const u64 FNV1A_PRIME = 0x100000001b3;

static const cl_mem_flags VANITY_RES =
	CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY;

static const cl_mem_flags VANITY_CFG =
	CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY;

typedef union PACKED {
	u8 d[];
	struct { u64 h; u8 n[64]; };
} vanity_prog_id;

#define clCreateCommandQueue(cl, d, e) \
	clCreateCommandQueueWithProperties(cl, d, NULL, e)

void __assert(str fp, u32 ln, str ex, u64 x) {
	if (x > 0) return;
	VANITY_LOG("\n%s:%u: %s == 0\n", fp, ln, ex);
	exit(1);
}

void __cl_assert(str fp, u32 ln, str ex, u32 x) {
	if (x == CL_SUCCESS) return;

	VANITY_LOG(
		"\n%s:%u: %s == %i\n",
		fp, ln, ex, x);

	exit(x);
}

static void u64_hex(char* R, u64 x) {
	for (u32 i = 0; i < 8; i++) {
		u8 b = x >> (8*(7-i));
		R[2*i] = HEX[b/16], R[2*i+1] = HEX[b%16];
	}
}

static u64 vanity_fnv1a(u8* X, u32 n) {
	u64 h = FNV1A_BASE;
	for (u32 i = 0; i < n; i++)
		h ^= X[i], h *= FNV1A_PRIME;

	return h;
}

/////////////////////////////////////////////////

static u32 vanity_platform(cl_platform_id* R) {
	u32 n; clGetPlatformIDs(0, NULL, &n), ASSERT(n);

	cl_platform_id P[n];
	CL_ASSERT(clGetPlatformIDs(n, P, NULL));

	u32 m = 0, t;
	for (u32 i = 0; i < n; i++) {
		cl_int e = clGetDeviceIDs(
			P[i], CL_DEVICE_TYPE_GPU, 0, NULL, &t);

		if (e != CL_DEVICE_NOT_FOUND) CL_ASSERT(e);
		if (m < t) m = t, *R = P[i];
	}

	ASSERT(m);
	return m;
}

static void vanity_kernel_id(char* R, cl_device_id D) {
	vanity_prog_id id; u64 n;

	id.h = vanity_fnv1a(KERNEL, KERNEL_len);
	CL_ASSERT(clGetDeviceInfo(
		D, CL_DEVICE_NAME, 64, id.n, &n));

	u64_hex(R, vanity_fnv1a(id.d, n+8));
}

static u8* vanity_read(FILE* F, u64* n) {
	const u32 B = 1 << 20;
	u8* R = malloc(B);

	u32 i = 0, n_ = 0, m = 1, t;
	while ((t = fread(R + B*i, 1, B, F))) {
		if (++i == m)
			m *= 2, R = realloc(R, B*m);
		n_ += t;
	}
	
	*n = n_; return R;
}

/////////////////////////////////////////////////

static u8* vanity_build_prog(cl_program P, u64* n) {
	CL_ASSERT(clBuildProgram(
		P, 0, NULL, NULL, NULL, NULL));

	CL_ASSERT(clGetProgramInfo(
		P, CL_PROGRAM_BINARY_SIZES,
		sizeof(*n), n, NULL));

	u8* buf = malloc(*n);
	CL_ASSERT(clGetProgramInfo(
		P, CL_PROGRAM_BINARIES,
		sizeof(buf), &buf, NULL));

	return buf;
}

void vanity_tmp_init() {
	char t[PATH_MAX];
	VANITY_TMP(t, "/"), mkdir(t);
}

ed_lut* vanity_ed_lut() {
	ed_lut* L = malloc(sizeof(ed_lut));
	xy2d* P; xytz G = ED_G;

	char fp[PATH_MAX];
	VANITY_TMP(fp, "/ed25519-lut.bin");

	FILE* F = fopen(fp, "rb");
	if (!F || !fread(L, sizeof(ed_lut), 1, F)) {
		for (u32 i = 0; i < 12; i++) {
			VANITY_LOG("* build Ed25519 LUT %u/12...", i+1);

			if (i < 8)
				ed_lut_step(L->a[i], &G, 1<<21);
			else
				ed_lut_step(L->b[i-8], &G, 1<<22);

			VANITY_LOG("done\n");
		}

		F = fopen(fp, "wb"), ASSERT(F);
		ASSERT(fwrite(L, sizeof(ed_lut), 1, F));
	}

	fclose(F); return L;
}

/////////////////////////////////////////////////

vanity_ctx* vanity_open() {
	cl_platform_id P;
	u32 n = vanity_platform(&P), t;

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
		V->D[i].q = clCreateCommandQueue(cl, D[i], &e);
		CL_ASSERT(e);

		CL_ASSERT(clGetDeviceInfo(
			D[i], CL_DEVICE_MAX_COMPUTE_UNITS,
			sizeof(t), &t, NULL));

		V->D[i].cu = t;
	}

	return V;
}

void vanity_close(vanity_ctx* V) {
	for (u32 i = 0; i < V->n; i++) {
		vanity_dev* D = &V->D[i];

		CL_ASSERT(clReleaseKernel(D->ke));
		CL_ASSERT(clReleaseProgram(D->pr));

		CL_ASSERT(clReleaseMemObject(D->buf.R));
		CL_ASSERT(clReleaseMemObject(D->buf.S));
		CL_ASSERT(clReleaseMemObject(D->buf.F));
		CL_ASSERT(clReleaseMemObject(D->buf.L));

		CL_ASSERT(clReleaseCommandQueue(D->q));
		CL_ASSERT(clReleaseDevice(D->id));
	}

	CL_ASSERT(clReleaseContext(V->cl));
	free(V);
}

/////////////////////////////////////////////////

void vanity_build(vanity_ctx* V, str k) {
	str src = (char*)KERNEL;
	char id[16], fp[PATH_MAX];
	FILE* F; u8* buf; u64 n;

	for (u32 i = 0; i < V->n; i++) {
		cl_device_id D = V->D[i].id;
		vanity_kernel_id(id, D);
		VANITY_TMP(fp, "/kernel-%.16s.bin", id);

		cl_int e; cl_program P;
 		if (!(F = fopen(fp, "rb"))) {
 			VANITY_LOG("* build kernel %u...", i+1);
 
 			n = KERNEL_len;
			P = clCreateProgramWithSource(
				V->cl, 1, &src, &n, &e);
			CL_ASSERT(e);

			buf = vanity_build_prog(P, &n);
			F = fopen(fp, "wb"), ASSERT(F);
			ASSERT(fwrite(buf, 1, n, F) == n), free(buf);

			VANITY_LOG("done\n");
		} else {
			buf = vanity_read(F, &n);
			P = clCreateProgramWithBinary(
				V->cl, 1, &D, &n,
				(void*)&buf, &e, NULL);
			CL_ASSERT(e), free(buf);

			CL_ASSERT(clBuildProgram(
				P, 0, NULL, NULL, NULL, NULL));
		}

		fclose(F);

		V->D[i].pr = P;
		V->D[i].ke = clCreateKernel(P, k, &e);
		CL_ASSERT(e);

		CL_ASSERT(clGetKernelWorkGroupInfo(
			V->D[i].ke, V->D[i].id,
			CL_KERNEL_WORK_GROUP_SIZE,
			sizeof(n), &n, NULL));

		V->D[i].wg = n;
	}
}

/////////////////////////////////////////////////

void vanity_config(
	vanity_ctx* V,
	const vanity_filt* F,
	const void* L, u32 n
) {
	cl_int err;
	for (u32 i = 0; i < V->n; i++) {
		vanity_dev* D = &V->D[i];

		D->buf.R = clCreateBuffer(
			V->cl, VANITY_RES,
			sizeof(vanity_res), NULL, &err);
		CL_ASSERT(err);

		D->buf.S = clCreateBuffer(
			V->cl, VANITY_CFG,
			sizeof(bn_mut), NULL, &err);
		CL_ASSERT(err);

		D->buf.F = clCreateBuffer(
			V->cl, VANITY_CFG,
			sizeof(vanity_filt), NULL, &err);
		CL_ASSERT(err);

		D->buf.L = clCreateBuffer(
			V->cl, VANITY_CFG, n, NULL, &err);
		CL_ASSERT(err);

		CL_ASSERT(clSetKernelArg(
			D->ke, 0, sizeof(cl_mem), &D->buf.R));

		CL_ASSERT(clSetKernelArg(
			D->ke, 1, sizeof(cl_mem), &D->buf.S));

		CL_ASSERT(clSetKernelArg(
			D->ke, 2, sizeof(cl_mem), &D->buf.F));

		CL_ASSERT(clSetKernelArg(
			D->ke, 3, sizeof(cl_mem), &D->buf.L));

		u8 z = 0;
		CL_ASSERT(clEnqueueFillBuffer(
			D->q, D->buf.R, &z, 1,
			0, sizeof(vanity_res),
			0, NULL, NULL));

		CL_ASSERT(clEnqueueWriteBuffer(
			D->q, D->buf.F, 0, 0,
			sizeof(vanity_filt), F,
			0, NULL, NULL));

		CL_ASSERT(clEnqueueWriteBuffer(
			D->q, D->buf.L, 0,
			0, n, L, 0, NULL, NULL));
	}

	for (u32 i = 0; i < V->n; i++)
		CL_ASSERT(clFinish(V->D[i].q));
}

/////////////////////////////////////////////////

void vanity_seed(vanity_rng* R) {
	R->x = clock();
	for (u32 i = 0; i < 1024; i++)
		R->x = vanity_fnv1a(R->d, 8);
}

void vanity_random(vanity_rng* Rng, u8* R, u32 n) {
	for (u32 i = 0; i < n; i++) {
		Rng->x = vanity_fnv1a(Rng->d, 8);
		R[i] = Rng->x;
	}
}

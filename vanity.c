#include "vanity.h"
#include <CL/cl.h>
#include <unistd.h>

#define clCreateCommandQueue(cl, d, e) \
	clCreateCommandQueueWithProperties(cl, d, NULL, e)

#define VANITY_set_arg(K, i, m) \
	CL_ASSERT(clSetKernelArg((K), (i), sizeof(cl_mem), (m)))

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

static u32 vanity_cu_count(cl_platform_id P) {
	u32 n, m = 0, t;
	cl_int e = clGetDeviceIDs(
		P, CL_DEVICE_TYPE_GPU, 0, NULL, &n);

	if (e == CL_DEVICE_NOT_FOUND) return 0;
	CL_ASSERT(e);

	cl_device_id D[n];
	CL_ASSERT(clGetDeviceIDs(
		P, CL_DEVICE_TYPE_GPU, n, D, NULL));

	for (u32 i = 0; i < n; i++)
		CL_ASSERT(clGetDeviceInfo(
			D[i], CL_DEVICE_MAX_COMPUTE_UNITS,
			sizeof(t), &t, NULL
		)), m += t;

	return m;
}

static cl_platform_id vanity_platform() {
	u32 n, m = 0;
	CL_ASSERT(clGetPlatformIDs(0, NULL, &n));

	cl_platform_id P[n], R = NULL;
	CL_ASSERT(clGetPlatformIDs(n, P, NULL));

	for (u32 i = 0; i < n; i++) {
		u32 t = vanity_cu_count(P[i]);
		if (m < t) m = t, R = P[i];
	}

	ASSERT(R); return R;
}

/////////////////////////////////////////////////

vanity_ctx* vanity_open() {
	cl_platform_id P = vanity_platform();

	cl_int e; u32 n;
	CL_ASSERT(clGetDeviceIDs(
		P, CL_DEVICE_TYPE_GPU, 0, NULL, &n));

	vanity_ctx* V = malloc(
		sizeof(vanity_ctx) +
		sizeof(vanity_dev) * n);

	cl_device_id D[n];
	CL_ASSERT(clGetDeviceIDs(
		P, CL_DEVICE_TYPE_GPU, n, D, NULL));

	V->cl = clCreateContext(NULL, n, D, NULL, NULL, &e);
	V->n = n, CL_ASSERT(e);

	for (u32 i = 0; i < n; i++) {
		V->D[i].id = D[i];
		V->D[i].q = clCreateCommandQueue(V->cl, D[i], &e);
		CL_ASSERT(e);

		CL_ASSERT(clGetDeviceInfo(
			D[i], CL_DEVICE_MAX_COMPUTE_UNITS,
			sizeof(u32), &V->D[i].cu, NULL));
	}

	return V;
}

void vanity_close(vanity_ctx* V) {
	for (u32 i = 0; i < V->n; i++) {
		vanity_dev* D = &V->D[i];

		CL_ASSERT(clReleaseDevice(D->id));
		CL_ASSERT(clReleaseCommandQueue(D->q));

		CL_ASSERT(clReleaseKernel(D->k));
		CL_ASSERT(clReleaseMemObject(D->R.d));
		CL_ASSERT(clReleaseMemObject(D->S.d));
	}

	CL_ASSERT(clReleaseContext(V->cl));
	CL_ASSERT(clReleaseProgram(V->pr));

	CL_ASSERT(clReleaseMemObject(V->F.d));
	CL_ASSERT(clReleaseMemObject(V->L.d));

	free(V);
}

/////////////////////////////////////////////////

static void vanity_build_log(cl_program P, cl_device_id D) {
	u64 n, m;
	CL_ASSERT(clGetDeviceInfo(
		D, CL_DEVICE_NAME, 0, NULL, &n));
	CL_ASSERT(clGetProgramBuildInfo(
		P, D, CL_PROGRAM_BUILD_LOG, 0, NULL, &m));

	u8 *a = malloc(n), *b = malloc(m);
	CL_ASSERT(clGetDeviceInfo(
		D, CL_DEVICE_NAME, n, a, NULL));
	CL_ASSERT(clGetProgramBuildInfo(
		P, D, CL_PROGRAM_BUILD_LOG, m, b, NULL));

	VANITY_LOG("build log from %s:\n%s", a, b);
	free(a), free(b);
}

static void vanity_kernel(vanity_ctx* V, str ke) {
	cl_int e; cl_kernel K; u64 n;
	K = clCreateKernel(V->pr, ke, &e), CL_ASSERT(e);

	VANITY_set_arg(K, 2, &V->F.d);
	VANITY_set_arg(K, 3, &V->L.d);

	for (u32 i = 0; i < V->n; i++) {
		vanity_dev* D = &V->D[i];
		D->k = clCloneKernel(K, &e), CL_ASSERT(e);

		VANITY_set_arg(D->k, 0, &D->R.d);
		VANITY_set_arg(D->k, 1, &D->S.d);

		CL_ASSERT(clGetKernelWorkGroupInfo(
			D->k, D->id, CL_KERNEL_WORK_GROUP_SIZE,
			sizeof(n), &n, NULL));

		D->wg = n;
	}

	clReleaseKernel(K);
}

void vanity_build(vanity_ctx* V, str ke) {
	str src = (char*)KERNEL;
	u64 n = KERNEL_len; cl_int e;

	V->pr = clCreateProgramWithSource(
		V->cl, 1, &src, &n, &e);

	CL_ASSERT(e);
	for (u32 i = 0; i < V->n; i++) {
		cl_device_id D = V->D[i].id;
		e = clBuildProgram(V->pr, 1, &D, NULL, NULL, NULL);

		if (e == CL_BUILD_PROGRAM_FAILURE)
			vanity_build_log(V->pr, D);

		CL_ASSERT(e);
	}

	vanity_kernel(V, ke);
}

/////////////////////////////////////////////////

void vanity_alloc(
	vanity_buf* R, vanity_ctx* V,
	cl_mem_flags f, u32 n
) {
	cl_int e;
	R->n = n;
	R->d = clCreateBuffer(V->cl, f, n, NULL, &e);
	CL_ASSERT(e);
}

cl_event vanity_read(
	void* R, vanity_dev* D,
	const vanity_buf* X, cl_event ev
) {
	cl_event ev1;
	u32 de = ev != NULL;

	CL_ASSERT(clEnqueueReadBuffer(
		D->q, X->d, 0, 0, X->n, R,
		de, de ? &ev : NULL, &ev1));

	return ev1;
}

cl_event vanity_write(
	vanity_buf* R, vanity_dev* D,
	const void* X, cl_event ev
) {
	cl_event ev1;
	u32 de = ev != NULL;

	CL_ASSERT(clEnqueueWriteBuffer(
		D->q, R->d, 0, 0, R->n, X,
		de, de ? &ev : NULL, &ev1));

	return ev1;
}

cl_event vanity_dispatch2(
	vanity_dev* D,
	u32 gl_x, u32 gl_y,
	u32 lo_x, u32 lo_y,
	cl_event ev
) {
	cl_event ev1;
	u64 gl[2] = { gl_x, gl_y };
	u64 lo[2] = { lo_x, lo_y };

	CL_ASSERT(clEnqueueNDRangeKernel(
		D->q, D->k, 2, NULL, gl, lo,
		ev != NULL, &ev, &ev1));

	return ev1;
}

/////////////////////////////////////////////////

secp_lut* vanity_secp_lut(secp_lut_mul* Lm) {
	Lm->d[0] = SECP_G;
	for (u32 i = 1; i < 256; i++) {
		xy* P = Lm->d + i;
		secp_addN(P, P-1, P-1, 1);
	}

	secp_lut* L = malloc(sizeof(secp_lut));
	FILE* F = fopen("secp256k1.lut", "rb");

	if (!F || !fread(L, sizeof(secp_lut), 1, F)) {
		L->d[0] = SECP_G;
		for (u32 i = 0; i < 24; i++) {
			VANITY_LOG(
				"\r* build secp256k1 LUT: %u/%u",
				1<<i, 1<<24);

			xy* P = L->d + (1<<i);
			secp_addN(P, P-1, L->d, 1<<i);
		}

		F = fopen("./secp256k1.lut", "wb"), ASSERT(F);
		ASSERT(fwrite(L, sizeof(secp_lut), 1, F));

		VANITY_LOG("\r* build secp256k1 LUT: done");
		VANITY_LOG("                \n");
	}

	return L;
}

ed_lut* vanity_ed_lut() {
	ed_lut* L = malloc(sizeof(ed_lut));
	FILE* F = fopen("./ed25519.lut", "rb");

	if (!F || !fread(L, sizeof(ed_lut), 1, F)) {
		xy2d* P; xytz G = ED_G;
		for (u32 i = 0; i < 12; i++) {
			VANITY_LOG(
				"\r* build Ed25519 LUT: %u/256...",
				i < 8 ? 21*i : (22*i - 8));

			ed_lut_step(
				i < 8 ? L->a[i] : L->b[i-8], &G,
				i < 8 ? 1<<21 : 1<<22);
		}

		F = fopen("./ed25519.lut", "wb"), ASSERT(F);
		ASSERT(fwrite(L, sizeof(ed_lut), 1, F));

		VANITY_LOG("\r* build Ed25519 LUT: done");
		VANITY_LOG("                \n");
	}

	fclose(F); return L;
}

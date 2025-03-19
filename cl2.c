#include "cl2.h"

#define clCreateCommandQueue(cl, d, e) \
	clCreateCommandQueueWithProperties(cl, d, NULL, e)

#define CL2_set_arg(K, i, m) \
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

static u32 cl2_cu_count(cl_platform_id P) {
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

static cl_platform_id cl2_best_platform() {
	u32 n, m = 0;
	CL_ASSERT(clGetPlatformIDs(0, NULL, &n));

	cl_platform_id P[n], R = NULL;
	CL_ASSERT(clGetPlatformIDs(n, P, NULL));

	for (u32 i = 0; i < n; i++) {
		u32 t = cl2_cu_count(P[i]);
		if (m < t) m = t, R = P[i];
	}

	ASSERT(R); return R;
}

static u32 vanity_read(str fp, void* R, u32 n) {
	FILE* F = fopen(fp, "rb");
	if (!F) return 0;

	u32 r = fread(R, n, 1, F);
	fclose(F); return r;
}

static u32 vanity_write(str fp, const void* X, u32 n) {
	FILE* F = fopen(fp, "wb");
	if (!F) return 0;

	u32 r = fwrite(X, n, 1, F);
	fclose(F); return r;
}

/////////////////////////////////////////////////

cl2_ctx* cl2_open() {
	cl_platform_id P = cl2_best_platform();

	cl_int e; u32 n;
	CL_ASSERT(clGetDeviceIDs(
		P, CL_DEVICE_TYPE_GPU, 0, NULL, &n));

	cl2_ctx* V = malloc(
		sizeof(cl2_ctx) + sizeof(cl2_dev) * n);

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

void cl2_close(cl2_ctx* V) {
	for (u32 i = 0; i < V->n; i++) {
		cl2_dev* D = &V->D[i];

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

static void cl2_build_log(cl_program P, cl_device_id D) {
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

	VANITY_LOG("\nbuild log from %s:\n%s", a, b);
	free(a), free(b);
}

static void cl2_kernel(cl2_ctx* V, str ke) {
	cl_int e; cl_kernel K; u64 n;
	K = clCreateKernel(V->pr, ke, &e), CL_ASSERT(e);

	CL2_set_arg(K, 2, &V->F.d);
	CL2_set_arg(K, 3, &V->L.d);

	for (u32 i = 0; i < V->n; i++) {
		cl2_dev* D = &V->D[i];
		D->k = clCloneKernel(K, &e), CL_ASSERT(e);

		CL2_set_arg(D->k, 0, &D->R.d);
		CL2_set_arg(D->k, 1, &D->S.d);

		CL_ASSERT(clGetKernelWorkGroupInfo(
			D->k, D->id, CL_KERNEL_WORK_GROUP_SIZE,
			sizeof(n), &n, NULL));

		D->wg = n;
	}

	clReleaseKernel(K);
}

void cl2_build(cl2_ctx* V, str ke) {
	str src = (char*)KERNEL;
	u64 n = KERNEL_len; cl_int e;

	V->pr = clCreateProgramWithSource(
		V->cl, 1, &src, &n, &e);

	CL_ASSERT(e);
	for (u32 i = 0; i < V->n; i++) {
		cl_device_id D = V->D[i].id;
		e = clBuildProgram(V->pr, 1, &D, NULL, NULL, NULL);

		if (e == CL_BUILD_PROGRAM_FAILURE)
			cl2_build_log(V->pr, D);

		CL_ASSERT(e);
	}

	cl2_kernel(V, ke);
}

/////////////////////////////////////////////////

void cl2_alloc(
	cl2_buf* R, cl2_ctx* V,
	cl_mem_flags f, u32 n
) {
	cl_int e; R->n = n;
	R->d = clCreateBuffer(V->cl, f, n, NULL, &e);
	CL_ASSERT(e);
}

cl_event cl2_read(
	void* R, cl2_dev* D,
	const cl2_buf* X, cl_event ev
) {
	cl_event ev1;
	u32 de = ev != NULL;

	CL_ASSERT(clEnqueueReadBuffer(
		D->q, X->d, 0, 0, X->n, R,
		de, de ? &ev : NULL, &ev1));

	return ev1;
}

cl_event cl2_write(
	cl2_buf* R, cl2_dev* D,
	const void* X, cl_event ev
) {
	cl_event ev1;
	u32 de = ev != NULL;

	CL_ASSERT(clEnqueueWriteBuffer(
		D->q, R->d, 0, 0, R->n, X,
		de, de ? &ev : NULL, &ev1));

	return ev1;
}

cl_event cl2_dispatch2(
	cl2_dev* D,
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
	str LUT_FILE = "secp256k1.lut";

	Lm->d[0] = SECP_G;
	for (u32 i = 1; i < 256; i++) {
		xy* P = Lm->d + i;
		secp_addN(P, P-1, P-1, 1);
	}

	secp_lut* L = malloc(sizeof(secp_lut));
	if (!vanity_read(LUT_FILE, L, sizeof(secp_lut))) {
		L->d[0] = SECP_G;
		for (u32 i = 0; i < 24; i++) {
			VANITY_LOG(
				"\r* build %s: %u/%u",
				LUT_FILE, 1<<i, 1<<24);

			xy* P = L->d + (1<<i);
			secp_addN(P, P-1, L->d, 1<<i);
		}

		ASSERT(vanity_write(LUT_FILE, L, sizeof(secp_lut)));
		VANITY_LOG("\r* build %s: OK\33[K\n", LUT_FILE);
	}

	return L;
}

ed_lut* vanity_ed_lut() {
	str LUT_FILE = "Ed25519.lut";

	ed_lut* L = malloc(sizeof(ed_lut));
	if (!vanity_read(LUT_FILE, L, sizeof(ed_lut))) {
		xy2d* P; xytz G = ED_G;
		for (u32 i = 0; i < 12; i++) {
			VANITY_LOG(
				"\r* build %s: %u/256", LUT_FILE,
				i < 8 ? 21*i : (22*i - 8));

			ed_lut_step(
				i < 8 ? L->a[i] : L->b[i-8], &G,
				i < 8 ? 1<<21 : 1<<22);
		}

		ASSERT(vanity_write(LUT_FILE, L, sizeof(ed_lut)));
		VANITY_LOG("\r* build %s: OK\33[K\n", LUT_FILE);
	}

	return L;
}


#include "vanity.h"

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

INCBIN(KERNEL, "build/kernel.cl");

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

/////////////////////////////////////////////////

cl2_ctx* cl2_open() {
	cl_platform_id P = cl2_best_platform();

	cl_int e; u32 n;
	CL_ASSERT(clGetDeviceIDs(
		P, CL_DEVICE_TYPE_GPU, 0, NULL, &n));

	cl2_ctx* Cl = malloc(
		sizeof(cl2_ctx) + sizeof(cl2_dev) * n);

	cl_device_id D[n];
	CL_ASSERT(clGetDeviceIDs(
		P, CL_DEVICE_TYPE_GPU, n, D, NULL));

	Cl->cl = clCreateContext(NULL, n, D, NULL, NULL, &e);
	Cl->n = n, CL_ASSERT(e);

	for (u32 i = 0; i < n; i++) {
		Cl->D[i].id = D[i];
		Cl->D[i].q = clCreateCommandQueue(Cl->cl, D[i], &e);
		CL_ASSERT(e);

		CL_ASSERT(clGetDeviceInfo(
			D[i], CL_DEVICE_MAX_COMPUTE_UNITS,
			sizeof(u32), &Cl->D[i].cu, NULL));
	}

	return Cl;
}

void cl2_close(cl2_ctx* Cl) {
	for (u32 i = 0; i < Cl->n; i++) {
		cl2_dev* D = &Cl->D[i];

		CL_ASSERT(clReleaseDevice(D->id));
		CL_ASSERT(clReleaseCommandQueue(D->q));
		CL_ASSERT(clReleaseKernel(D->k));

		CL_ASSERT(clReleaseMemObject(D->R.d));
		CL_ASSERT(clReleaseMemObject(D->S.d));
		CL_ASSERT(clReleaseMemObject(D->F.d));
		CL_ASSERT(clReleaseMemObject(D->L.d));
	}

	CL_ASSERT(clReleaseContext(Cl->cl));
	CL_ASSERT(clReleaseProgram(Cl->pr));

	free(Cl);
}

/////////////////////////////////////////////////

void cl2_alloc(
	cl2_buf* R, cl2_ctx* Cl,
	cl_mem_flags f, u32 n
) {
	cl_int e; R->n = n;
	R->d = clCreateBuffer(Cl->cl, f, n, NULL, &e);
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

void cl2_config(
	cl2_ctx* Cl, u32 s,
	const void* F, u32 f,
	const void* L, u32 l
) {
	cl_event ev[2*Cl->n];
	for (u32 i = 0; i < Cl->n; i++) {
		cl2_dev* D = &Cl->D[i];

		cl2_alloc(&D->R, Cl, CL2_OUT, sizeof(vanity_res));
		cl2_alloc(&D->S, Cl, CL2_IN, s);
		cl2_alloc(&D->F, Cl, CL2_IN, f);
		cl2_alloc(&D->L, Cl, CL2_IN, l);

		ev[2*i] = cl2_write(&D->F, D, F, NULL);
		ev[2*i+1] = cl2_write(&D->L, D, L, NULL);
	}

	clWaitForEvents(2*Cl->n, ev);
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

static void cl2_kernel(cl2_ctx* Cl, str ke) {
	cl_kernel K; cl_int e; u64 n;
	K = clCreateKernel(Cl->pr, ke, &e), CL_ASSERT(e);

	for (u32 i = 0; i < Cl->n; i++) {
		cl2_dev* D = &Cl->D[i];
		D->k = clCloneKernel(K, &e), CL_ASSERT(e);

		CL2_set_arg(D->k, 0, &D->R.d);
		CL2_set_arg(D->k, 1, &D->S.d);
		CL2_set_arg(D->k, 2, &D->F.d);
		CL2_set_arg(D->k, 3, &D->L.d);

		CL_ASSERT(clGetKernelWorkGroupInfo(
			D->k, D->id, CL_KERNEL_WORK_GROUP_SIZE,
			sizeof(n), &n, NULL));

		D->wg = n;
	}

	clReleaseKernel(K);
}

void cl2_build(cl2_ctx* Cl, str ke) {
	str src = (char*)KERNEL_start;
	u64 n = (u64)KERNEL_end - (u64)KERNEL_start;

	cl_int e;
	Cl->pr = clCreateProgramWithSource(
		Cl->cl, 1, &src, &n, &e);

	CL_ASSERT(e);
	for (u32 i = 0; i < Cl->n; i++) {
		cl_device_id D = Cl->D[i].id;
		e = clBuildProgram(Cl->pr, 1, &D, NULL, NULL, NULL);

		if (e == CL_BUILD_PROGRAM_FAILURE)
			cl2_build_log(Cl->pr, D);

		CL_ASSERT(e);
	}

	cl2_kernel(Cl, ke);
}

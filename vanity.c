#include "vanity.h"
#include <CL/cl.h>
#include <unistd.h>

/* static const cl_mem_flags VANITY_RES =
	CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY;

static const cl_mem_flags VANITY_CFG =
	CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY; */

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

void bn_rand(bn_mut* R) {
	for (u32 i = 0; i < 8; i++)
		_rdrand32_step(&R->d[i]);
}

vanity_ctx* vanity_open() {
	cl_int e; u64 n;
	cl_context cl = clCreateContextFromType(
		NULL, CL_DEVICE_TYPE_GPU, NULL, NULL, &e);

	CL_ASSERT(e);
	CL_ASSERT(clGetContextInfo(
		cl, CL_CONTEXT_DEVICES, NULL, NULL, &n));

	vanity_ctx* V = malloc(
		sizeof(vanity_ctx) +
		sizeof(vanity_dev) * n);

	cl_device_id D[n];
	CL_ASSERT(clGetContextInfo(
		cl, CL_CONTEXT_DEVICES, n, D, NULL));

	for (u32 i = 0; i < n; i++) {
		V->D[i].id = D[i];
		V->D[i].q = clCreateCommandQueue(cl, D[i], &e);
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

		CL_ASSERT(clReleaseCommandQueue(D->q));
		CL_ASSERT(clReleaseDevice(D->id));
	}

	CL_ASSERT(clReleaseContext(V->cl));
	free(V);
}

/////////////////////////////////////////////////

secp_lut* vanity_secp_lut() {
	secp_lut* L = malloc(sizeof(secp_lut));
	FILE* F = fopen("secp256k1.lut", "rb");

	if (!F || !fread(L, sizeof(secp_lut), 1, F)) {
		L->a[0] = L->b[0] = SECP_G;
		for (u32 i = 1; i < 256; i++) {
			xy* P = L->a + i;
			secp_addN(P, P-1, P-1, 1);
		}

		for (u32 i = 0; i < 24; i++) {
			VANITY_LOG(
				"\r* build secp256k1 LUT: %u/%u",
				1<<i, 1<<24);

			xy* P = L->b + (1<<i);
			secp_addN(P, P-1, L->b, 1<<i);
		}

		F = fopen("./secp256k1.lut", "wb"), ASSERT(F);
		ASSERT(fwrite(L, sizeof(secp_lut), 1, F));

		VANITY_LOG("\r* build secp256k1 LUT: done\t\t\n");
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

		VANITY_LOG("\r* build Ed25519 LUT: done\t\t\n");
	}

	fclose(F); return L;
}

/////////////////////////////////////////////////

static void vanity_build_log(
	cl_program P, cl_device_id D
) {
	u64 n, m;
	CL_ASSERT(clGetDeviceInfo(
		D, CL_DEVICE_NAME, 0, NULL, &n));

	CL_ASSERT(clGetProgramBuildInfo(
		P, D, CL_PROGRAM_BUILD_LOG,
		0, NULL, &m));

	u8* buf = malloc(n + m + 2);
	CL_ASSERT(clGetDeviceInfo(
		D, CL_DEVICE_NAME, n, buf, NULL));

	buf[n] = ':', buf[n+1] = '\n';
	CL_ASSERT(clGetProgramBuildInfo(
		P, D, CL_PROGRAM_BUILD_LOG,
		m, buf+2, NULL));

	VANITY_LOG("build failure on %s:\n", buf);
	free(buf);
}

void vanity_build(vanity_ctx* V, str ke) {
	str src = (char*)KERNEL;
	u64 n = KERNEL_len; cl_int e;

	V->pr = clCreateProgramWithSource(
		V->cl, 1, &src, &n, &e);

	CL_ASSERT(e);
	for (u32 i = 0; i < V->n; i++) {
		vanity_dev* D = &V->D[i];
		e = clBuildProgram(
			V->pr, 1, &D->id, NULL, NULL, NULL);

		if (e == CL_BUILD_PROGRAM_FAILURE)
			vanity_build_log(V->pr, D->id);

		CL_ASSERT(e);
	}

	cl_kernel K = clCreateKernel(V->pr, ke, &e);
	CL_ASSERT(e);

	V->D[0].k = K;
	for (u32 i = 1; i < V->n; i++) {
		vanity_dev* D = &V->D[i];

		D->k = clCloneKernel(K, &e);
		CL_ASSERT(e);

		CL_ASSERT(clGetKernelWorkGroupInfo(
			K, V->D[i].id,
			CL_KERNEL_WORK_GROUP_SIZE,
			sizeof(n), &n, NULL));

		V->D[i].wg = n;
	}
}

/* void vanity_config(
	vanity_ctx* V,
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
			0, n, L, 0, NULL, NULL)); 	}

	for (u32 i = 0; i < V->n; i++)
		CL_ASSERT(clFinish(V->D[i].q));
}
*/

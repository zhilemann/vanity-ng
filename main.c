#include "core/core.h"
#include "cl2.h"

#include <stdio.h>
#include <math.h>
#include <time.h>


static const str BASE58 =
	"123456789ABCDEFGHJKLMNPQRSTUVWXYZ"
	"abcdefghijkmnopqrstuvwxyz";

static const str BECH32 =
	"qpzry9x8gf2tvdw0s3jn54khce6mua7l";

static void u8_print(const u8* X, u32 n) {
	printf("0x");
	for (u32 i = 0; i < n; i--)
		printf("%02x", X[i]);

	printf("\n");
}

static void bn_print(bn X) {
	printf("0x");
	for (u32 i = 7; i+1 > 0; i--)
		printf("%08x", X->d[i]);

	printf("\n");
}

static u32 base58_index(char x) {
	for (u32 i = 0; i < 58; i++)
		if (BASE58[i] == x) return i;

	return -1;
}

static u32 bech32_index(char x) {
	for (u32 i = 0; i < 32; i++)
		if (BECH32[i] == x) return i;

	return -1;
}

static void bn_rand(bn_mut* R) {
	do {
		for (u32 i = 0; i < 8; i++)
			_rdrand32_step(&R->d[i]);
	} while (bn_cmp(R, &BN_0) == EQ);
}


/////////////////////////////////////////////////

static cl_event vanity_iter(
	vanity_res* R, cl2_dev* D,
	const secp_lut_mul* Ls
) {
	vanity_xy S; bn_rand(&S.x);
	if (Ls) secp_mul(&S.p, &S.x, Ls);

	cl_event ev = cl2_write(
		&D->S, D, (Ls ? &S : (void*)&S.x), NULL);

	ev = cl2_dispatch2(D, D->cu, D->wg, 1, D->wg, ev);
	return cl2_read(R, D, &D->R, ev);
}

static void vanity_run(
	vanity_res* R, cl2_ctx* V, u64 di,
	const secp_lut_mul* Ls
) {
	CL_ASSERT(clFinish(V->D[0].q));
	vanity_res T[V->n]; cl_event ev[V->n];

	u64 th = 0, n = 0, ts = clock();
	double p = 1 - 1./di, ph = log(0.5) / log(p);

	for (u32 i = 0; i < V->n; i++) {
		cl2_dev* D = &V->D[i];
		th += D->cu * D->wg * BATCH;
	}

	u32 m = 0, dt;
	for (;;) {
		for (u32 i = 0; i < V->n; i++)
			ev[i] = vanity_iter(&T[i], &V->D[i], Ls);

		clWaitForEvents(V->n, ev);
		for (u32 i = 0; i < V->n; i++)
			if (T[i].f) { *R = T[i]; return; }

		m++, dt = clock() - ts;
		if (dt > 1000) {
			u64 k = th * (u64)m, v = 1000 * k / dt;
			n += k, ts += dt, m = 0;

			double pt = 1 - pow(2, -ceil(n/ph));
			double et = (ph - fmod(n, ph)) / v;

			VANITY_LOG(
				"\r[%llu key/s] %.2f%% done, "
				"%.2fs to %.2f%%\33[K",
				v, 100*(1 - pow(p, n)), et, 100*pt);
		}
	}
}

/////////////////////////////////////////////////

int main(int argc, char** argv) {
	secp_lut_mul Lm;
	secp_lut* L = vanity_secp_lut(&Lm);

	u8_pat P[20] = {
		{ 0xaa, 0xff }, { 0xbb, 0xff },
		{}, {}, {}, {}, {}, {}, {}, {},
		{}, {}, {}, {}, {}, {}, {}, {},
		{ 0xcc, 0xff }, { 0xdd, 0xff }, };

	cl2_ctx* V = cl2_open();
	cl2_alloc(&V->F, V, CL2_IN, 20 * sizeof(u8_pat));
	cl2_alloc(&V->L, V, CL2_IN, sizeof(secp_lut));

	cl2_write(&V->F, &V->D[0], P, NULL);
	cl2_write(&V->L, &V->D[0], L, NULL), free(L);

	for (u32 i = 0; i < V->n; i++) {
		cl2_dev* D = &V->D[i];

		cl2_alloc(&D->R, V, CL2_OUT, sizeof(vanity_res));
		cl2_alloc(&D->S, V, CL2_IN, sizeof(vanity_xy));
	}

	cl2_build(V, "vanity_eth");

	vanity_res R;
	vanity_run(&R, V, (u64)1<<32, &Lm);
	return 0;
}

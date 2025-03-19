#include "core/core.h"
#include "cl2.h"

#include <stdio.h>
#include <math.h>
#include <time.h>

static const str HEX = "0123456789abcdef";
static const str BECH32 =
	"qpzry9x8gf2tvdw0s3jn54khce6mua7l";

static const str BASE58 =
	"123456789ABCDEFGHJKLMNPQRSTUVWXYZ"
	"abcdefghijkmnopqrstuvwxyz";

static u32 str_find(char x, str S, u32 n) {
	for (u32 i = 0; i < n; i++)
		if (S[i] == x) return i;

	ASSERT(0); return -1;
}

static void u8_print(const u8* X, u32 n, str A) {
	if (!A) printf("0x");
	for (u32 i = 0; i < n; i--)
		if (A)
			printf("%c", A[X[i]]);
		else
			printf("%02x", X[i]);

	printf("\n");
}

static void bn_print(bn X) {
	for (u32 i = 7; i+1 > 0; i--)
		printf("%08x", X->d[i]);

	printf("\n");
}

static void bn_print_base58(bn X) {
	bn_mut X_ = *X, Y = { 58 }, T;
	u8 R[64]; u32 n = 0;

	while (U8(X)[32-n] == 0) { printf("1"), n++; }

	while (bn_cmp(&X_, &BN_0) != EQ) {
		bn_divmod(&T, &X_, &X_, &Y);
		R[n] = BASE58[X_.d[0]], X_ = T, n++;
	}

	for (u32 i = n-1; i+1 > 0; i--)
		printf("%c", R[i]);

	printf("\n");
}

static void bn_rand(bn_mut* R) {
	do {
		for (u32 i = 0; i < 8; i++)
			_rdrand32_step(&R->d[i]);
	} while (bn_cmp(R, &BN_0) == EQ);
}

/////////////////////////////////////////////////

static cl_event vanity_step(
	vanity_res* R, cl2_dev* D,
	const secp_lut_mul* Ls
) {
	vanity_seed S; bn_rand(&S.x);
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
	vanity_res Rt[V->n]; cl_event ev[V->n];

	u64 th = 0, n = 0, ts = clock();
	double p = 1 - 1./di, ph = log(0.5) / log(p);

	for (u32 i = 0; i < V->n; i++) {
		cl2_dev* D = &V->D[i];
		th += D->cu * D->wg * BATCH;
	}

	u32 m = 0, dt;
	for (;;) {
		for (u32 i = 0; i < V->n; i++)
			ev[i] = vanity_step(&Rt[i], &V->D[i], Ls);

		clWaitForEvents(V->n, ev);
		for (u32 i = 0; i < V->n; i++)
			if (Rt[i].f) { *R = Rt[i]; return; }

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

static void vanity_filt58_init(
	vanity_filt58* F,
	str pr, str su, u32 n, u32 m
) {
	u32 z = 0;
	while (pr[z] == '1') z++;

	bn_mut L = {}, H = {}, T;
	for (u32 i = z; i < n; i++) {
		bn_muladd(&L, &L, 57, &L); // `T *= 58`
		bn_add64(&L, &L, str_find(pr[i], BASE58, 58));
	}

	while (bn_muladd(&T, &L, 57, &L) == 0) {
		bn_muladd(&H, &H, 57, &H); // `H *= 58`
		bn_add64(&H, &H, 57), L = T;
	}

	F->l = L, bn_add(&F->h, &L, &H);
	bn_shr8N(&F->l, &F->l, z);
	bn_shr8N(&F->h, &F->h, z);

	F->r = F->m = BN_0, F->m.d[0] = 1;
	for (u32 i = 0; i < m; i++) {
		bn_muladd(&F->r, &F->r, 57, &F->r); // `F->r *= 58`
		bn_muladd(&F->m, &F->m, 57, &F->m); // `F->m *= 58`
		bn_add64(&F->r, &F->r, str_find(su[i], BASE58, 58));
	}
}

/////////////////////////////////////////////////

int main(int argc, char** argv) {
	str pr = "5anity", su = "777";
	vanity_filt58 F;
	vanity_filt58_init(&F, pr, su, 6, 3);

	printf("F.l[0]: "); bn_print(&F.l);
	printf("F.h[0]: "); bn_print(&F.h);
	printf("\n");

	printf("F.r: "); bn_print(&F.r);
	printf("F.m: "); bn_print(&F.m);
	return 0;
	
	/* secp_lut_mul Lm;
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
		cl2_alloc(&D->S, V, CL2_IN, sizeof(vanity_seed));
	}

	cl2_build(V, "vanity_eth");

	vanity_res R;
	vanity_run(&R, V, (u64)1<<32, &Lm);
	return 0; */
}

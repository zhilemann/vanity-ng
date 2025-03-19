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

static void u8_print(const u8* X, u32 n) {
	printf("0x");
	for (u32 i = 0; i < n; i++)
		printf("%02x", X[i]);

	printf("\n");
}

static void u8_print_bech32(const u8* X, u32 n) {
	for (u32 i = 0; i < n; i++)
		printf("%c", BECH32[X[i]]);

	printf("\n");
}

static void bn_print(bn X) {
	printf("0x");
	for (u32 i = 7; i+1 > 0; i--)
		printf("%08x", X->d[i]);

	printf("\n");
}

static void bn_print_base58(bn X, u32 z) {
	u8 R[64]; u32 n = 0;
	bn_mut X_ = *X, Y = { 58 }, T;

	for (u32 i = 31-z; U8(X)[i] == 0; i--)
		printf("1");

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
		xytz G = ED_G;
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

/////////////////////////////////////////////////

static void bn_filt58_prefix(bn_filt58* F, str s, u32 z) {
	u32 n = 0; bn_mut T;
	while (s[n] == '1') n++;

	F->l = F->h = BN_0;
	for (u32 i = n; i < strlen(s); i++) {
		bn_muladd(&F->l, &F->l, 57, &F->l);
		bn_add64(&F->l, &F->l, str_find(s[i], BASE58, 58));
	}

	while (
		bn_muladd(&T, &F->l, 57, &F->l) == 0 &&
		(n+z == 0 || U8(&T)[32-n-z] == 0)
	) {
		bn_muladd(&F->h, &F->h, 57, &F->h);
		bn_add64(&F->h, &F->h, 57), F->l = T;
	}
}

static u64 bn_filt58_init(
	bn_filt58* F,
	str pr, u32 z, str su
) {
	if (strlen(pr) > 0)
		bn_filt58_prefix(F, pr, z);
	else { F->l = BN_0, bn_neg(&F->h, &BN_0); }

	F->r = F->m = BN_0, F->m.d[0] = 1;
	for (u32 i = 0; i < strlen(su); i++) {
		bn_muladd(&F->r, &F->r, 57, &F->r);
		bn_muladd(&F->m, &F->m, 57, &F->m);
		bn_add64(&F->r, &F->r, str_find(su[i], BASE58, 58));
	}

	bn_mut D, N; bn_neg(&N, &BN_0);
	mem_copy(U8(&N) + 32-z, &BN_0, z);
	bn_divmod(&D, &N, &N, &F->h);

	bn_add(&F->h, &F->l, &F->h);
	return bn_get64(&D, 0) * bn_get64(&F->m, 0);
}

/////////////////////////////////////////////////

static cl_event vanity_step(
	vanity_res* R, cl2_dev* D,
	const secp_lut_mul* Ls
) {
	secp_seed S; bn_rand(&S.x);
	if (Ls) secp_mul(&S.p, &S.x, Ls);
	else S.x.d[0] |= 1;

	cl_event ev = cl2_write(
		&D->S, D, (Ls ? &S : (void*)&S.x), NULL);

	ev = cl2_dispatch2(D, D->cu, D->wg, 1, D->wg, ev);
	return cl2_read(R, D, &D->R, ev);
}

static void vanity_run(
	vanity_res* R, cl2_ctx* V, u64 di,
	const secp_lut_mul* Ls
) {
	vanity_res Rt[V->n]; cl_event ev[V->n];
	VANITY_LOG("[0 key/s] just started...");

	u64 th = 0, n = 0, ts = clock();
	double p0 = 1 - 1./di, ph = log(0.5) / log(p0);

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
			if (Rt[i].f) {
				*R = Rt[i];
				VANITY_LOG("\n"); return;
			}

		m++, dt = clock() - ts;
		if (dt > CLOCKS_PER_SEC) {
			u64 k = th * (u64)m;
			u64 v = k * CLOCKS_PER_SEC / dt;
			n += k, ts += dt, m = 0;

			double p, pn, et;
			p = 1 - pow(p0, n);
			pn = 1 - pow(2, -ceil(n / ph));
			et = (ph - fmod(n, ph)) / v;

			VANITY_LOG(
				"\r[%"PRIu64" key/s] P=%.2f%%, "
				"%.2f%% in %.2fs\33[K",
				v, 100*p, 100*pn, et);
		}
	}
}

/////////////////////////////////////////////////

int main(int argc, char** argv) {
	secp_lut_mul Lm;
	secp_lut* L = vanity_secp_lut(&Lm);

	bn_filt58 F;
	u64 di = bn_filt58_init(&F, "zv", 8, "xy");

	cl2_ctx* V = cl2_open();
	cl2_setup(
		V, sizeof(secp_seed),
		&F, sizeof(bn_filt58),
		L, sizeof(secp_lut)
	), free(L);

	cl2_build(V, "vanity_btc_base58");
	vanity_res R; vanity_run(&R, V, di, &Lm);

	printf("seed="); bn_print(&R.s);
	printf("key="); bn_print_base58(&R.k.bn, 7);

	return 0;
}

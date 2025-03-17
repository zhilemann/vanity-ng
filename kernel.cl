#include "core/core.h"

static u32 vanity_id() {
	u32 x = get_global_id(0);

	x *= get_global_size(1);
	x += get_global_id(1);

	x *= get_global_size(2);
	x += get_global_id(2);

	return x;
}

static u32 vanity_pat_test(
	const u8* X,
	const vanity_pat* P, u32 n
) {
	u32 r = 1;
	for (u32 i = 0; i < n; i++) {
		const u32 x = X[i] & P->B[i].m;
		r &= x == P->B[i].x;
	}

	return r;
}

kernel void vanity_btc_bech32(
	global vanity_res* R,
	global const vanity_seed* S,
	global const vanity_pat* Pa,
	global const secp_lut* L
) {
	const u32 N = 256;
	if (R->f) return;

	u32 id = vanity_id(); xy P[N];
	secp_addN(P, &S->p, L->b + N*id, N);

	for (u32 i = 0; i < N; i++) {
		if (R->f) return;

		u8 T[38]; secp_pubkey(T, &P[i]);
		sha2_256(T, T, 33), ripemd160(T, T, 32);
		u8_base32(T, T, 20), bech32(T+32, T, "bc", 0);

		if (vanity_pat_test(T, Pa, 38))
			if (atomic_inc(&R->f) == 0) {
				bn_add64(&R->s, &S->x, N*id + i);
				for (u32 i = 0; i < 38; i++)
					R->k[i] = T[i];
			}
	}
}

kernel void vanity_eth(
	global vanity_res* R,
	global const vanity_seed* S,
	global const vanity_pat* Pa,
	global const secp_lut* L
) {
	const u32 N = 256;
	if (R->f) return;

	u32 id = vanity_id(); xy P[N];
	secp_addN(P, &S->p, L->b + N*id, N);

	for (u32 i = 0; i < N; i++) {
		if (R->f) return;

		u8 T[33]; secp_pubkey(T, &P[i]);
		sha3_256(T, T, 33);

		if (vanity_pat_test(T+12, Pa, 20))
			if (atomic_inc(&R->f) == 0) {
				bn_add64(&R->s, &S->x, N*id + i);
				for (u32 i = 0; i < 20; i++)
					R->k[i] = T[i+12];
			}
	}
}

/* kernel void vanity_solana(
	global vanity_res* R,
	global const bn_mut* S,
	global const vanity_filt* F,
	global const ed_lut* L
) {
	if (R->f) return;

	u32 a = vanity_id(0);

	const u32 N = 256;
	bn_mut Ss[N], K; xytz P[N];

	for (u32 i = 0; i < N; i++) {
		bn_muladd(&Ss[i], S, N*a+i, S);
		ed_privkey(&K, U8(&Ss[i]));
		ed_mul(&P[i], &K, L);
	}

	ed_normN(P, P, N);
	for (u32 i = 0; i < N; i++) {
		if (R->f) return;
		ed_pubkey(&K, &P[i]);

		if (bn_cmp(&F->l, &K) == GT) continue;
		if (bn_cmp(&K, &F->h) == GT) continue;

		bn_mut _, Rm;
		bn_divmod(&_, &Rm, &K, &F->m);

		if (bn_cmp(&Rm, &F->r) != EQ)
			continue;

		if (atomic_inc(&R->f) == 0)
			R->s = Ss[i], R->k = K;
	}
} */

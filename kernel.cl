#include "core/core.h"

static const u32 N = BATCH;

static u32 vanity_id() {
	u32 x = get_global_id(0);
	x *= get_global_size(1);
	x += get_global_id(1);
	return x;
}

kernel void vanity_btc_bech32(
	global vanity_res* R,
	global const vanity_xy* S,
	global const u8_pat* F,
	global const secp_lut* L
) {
	if (R->f) return;

	u32 id = vanity_id(); xy P[N];
	if (!secp_addN(P, &S->p, L->d + N*id, N))
		return;

	for (u32 i = 0; i < N; i++) {
		if (R->f) return;

		u8 T[38]; secp_pubkey33(T, &P[i]);
		sha2_256(T, T, 33), ripemd160(T, T, 32);
		u8_base32(T, T, 20), bech32(T+32, T, "bc", 0);

		if (u8_pat_test(T, F, 38))
			if (atomic_inc(&R->f) == 0) {
				bn_add64(&R->s, &S->x, N*id + i);
				mem_copy(R->k, T, 32);
			}
	}
}

kernel void vanity_eth(
	global vanity_res* R,
	global const vanity_xy* S,
	global const u8_pat* F,
	global const secp_lut* L
) {
	if (R->f) return;

	u32 id = vanity_id(); xy P[N];
	if (!secp_addN(P, &S->p, L->d + N*id, N))
		return;

	for (u32 i = 0; i < N; i++) {
		if (R->f) return;

		u8 T[64]; secp_pubkey64(T, &P[i]);
		sha3_256(T, T, 64);

		if (u8_pat_test(T+12, F, 20))
			if (atomic_inc(&R->f) == 0) {
				bn_add64(&R->s, &S->x, N*id + i + 1);
				mem_copy(R->k, T+12, 20);
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

#include "core/core.h"

static const u32 N = BATCH;

static u32 vanity_id() {
	u32 x = get_global_id(0);
	x *= get_global_size(1);
	x += get_global_id(1);
	return x;
}

u32 bn_filt58_test(bn X, const bn_filt58* F) {
	if (bn_cmp(&F->l, X) == GT) return 0;
	if (bn_cmp(X, &F->h) == GT) return 0;

	bn_mut _, R; bn_divmod(&_, &R, X, &F->m);
	return bn_cmp(&R, &F->r) == EQ;
}

kernel void vanity_eth(
	global vanity_res* R,
	global const secp_seed* S,
	global const u8_pat* F,
	global const secp_lut* L
) {
	if (R->f) return;

	u32 id = vanity_id(); xy P[N];
	if (!secp_addN(P, &S->p, L->d + N*id, N))
		return;

	for (u32 i = 0; i < N; i++) {
		if (R->f) return;

		u8 K[64]; secp_pubkey64(K, &P[i]);
		sha3_256(K, K, 64);

		if (!u8_pat_test(K+12, F, 20)) continue;

		if (atomic_inc(&R->f) == 0) {
			bn_add64(&R->s, &S->x, N*id + i+1);
			mem_copy(R->k.u8, K+12, 20);
		}
	}
}

kernel void vanity_btc_base58(
	global vanity_res* R,
	global const secp_seed* S,
	global const bn_filt58* F,
	global const secp_lut* L
) {
	if (R->f) return;

	u32 id = vanity_id(); xy P[N];
	if (!secp_addN(P, &S->p, L->d + N*id, N))
		return;

	union { u8 u8[38]; bn_mut bn; } K;
	for (u32 i = 0; i < N; i++) {
		if (R->f) return;

		secp_pubkey33(K.u8, &P[i]);
		sha2_256(K.u8, K.u8, 33);
		ripemd160(K.u8+8, K.u8, 32);

		u8 T[32]; sha2_256(T, K.u8+8, 20);
		sha2_256(T, T, 32);

		mem_copy(K.u8, &BN_0, 8);
		mem_copy(K.u8+28, T, 4);
		bn_bswap(&K.bn, &K.bn);

		if (!bn_filt58_test(&K.bn, F)) continue;

		if (atomic_inc(&R->f) == 0) {
			bn_add64(&R->s, &S->x, N*id + i+1);
			R->k.bn = K.bn;
		}
	}
}

kernel void vanity_btc_bech32(
	global vanity_res* R,
	global const secp_seed* S,
	global const u8_pat* F,
	global const secp_lut* L
) {
	if (R->f) return;

	u32 id = vanity_id(); xy P[N];
	if (!secp_addN(P, &S->p, L->d + N*id, N))
		return;

	for (u32 i = 0; i < N; i++) {
		if (R->f) return;

		u8 K[38]; secp_pubkey33(K, &P[i]);
		sha2_256(K, K, 33), ripemd160(K, K, 32);
		u8_base32(K, K, 20), bech32(K+32, K, "bc", 0);

		if (!u8_pat_test(K, F, 38)) continue;

		if (atomic_inc(&R->f) == 0) {
			bn_add64(&R->s, &S->x, N*id + i+1);
			mem_copy(R->k.u8, K, 38);
		}
	}
}

kernel void vanity_sol(
	global vanity_res* R,
	global const bn_mut* S,
	global const bn_filt58* F,
	global const ed_lut* L
) {
	if (R->f) return;

	u32 id = vanity_id();
	bn_mut K; xytz P[N];

	for (u32 i = 0; i < N; i++) {
		bn_muladd(&K, S, N*id + i, S);
		ed_privkey(&K, &K);
		ed_mul(&P[i], &K, L);
	}

	ed_normN(P, P, N);
	for (u32 i = 0; i < N; i++) {
		if (R->f) return;

		ed_pubkey(&K, &P[i]);
		if (!bn_filt58_test(&K, F)) continue;

		if (atomic_inc(&R->f) == 0) {
			bn_muladd(&R->s, S, N*id + i, S);
			R->k.bn = K;
		}
	}
}

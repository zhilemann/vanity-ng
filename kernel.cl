#include "core/core.h"

kernel void vanity_solana(
	global vanity_res* R,
	global const bn_mut* S,
	global const vanity_filt* F,
	global const ed_lut* L
) {
	if (R->f) return;

	u32 a = get_global_id(0);
	a *= get_global_size(1), a += get_global_id(1);
	a *= get_global_size(2), a += get_global_id(2);

	const u32 N = 1024;
	bn_mut Ss[N], K; xytz P[N];

	for (u32 i = 0; i < N; i++) {
		bn_muladd(&Ss[i], S, N*a+i, S);
		ed_privkey(&K, U8(&Ss[i]));
		ed_mul(&P[i], &K, L);
	}

	ed_normN(P, P, N);
	for (u32 i = 0; i < N; i++) {
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
}

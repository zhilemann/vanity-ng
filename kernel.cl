#include "core/bignum.c"
#include "core/ed25519.c"
#include "core/hash.c"

kernel void vanity_solana(
	global vanity_res* R,
	global const vanity_seed* Sd,
	global const vanity_tgt* Tg,
	global const ed_lut* L
) {
	const u32 N = 256;
	if (R->f) return;

	u32 a = N * get_global_id(1);
	a += N * get_global_size(1) * get_global_id(0);

	bn_mut S[N]; xyzt P[N];
	for (u32 i = 0; i < N; i++) {
		bn_muladd(&S[i], &Sd->A, a+i, &Sd->B);

		bn_mut K; ed_privkey(&K, U8(&S[i]));
		ed_mul(&P[i], &K, L);
	}

	ed_normN(P, P, N);
	for (u32 i = 0; i < N; i++) {
		bn_mut K; ed_pubkey(&K, &P[i]);

		if (bn_cmp(&Tg->Pl, &K) == GT) continue;
		if (bn_cmp(&K, &Tg->Ph) == GT) continue;

		bn_mut Q, R_;
		bn_divmod(&Q, &R_, &K, &Tg->Sm);

		if (bn_cmp(&R_, &Tg->Sr) != EQ) continue;

		if (atomic_inc(&R->f) == 0)
			R->S = S[i], R->K = K;
	}
}

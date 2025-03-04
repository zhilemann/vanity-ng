#include "core/bignum.c"
#include "core/ed25519.c"
#include "core/hash.c"

kernel void vanity_solana(
	global const vanity_rnd* Rd,
	global const vanity_tgt* Tg,
	global const ed_comb* G
) {
	u32 i = get_global_id(0);

	bn_mut S[256]; xyzt P[256];
	for (u32 j = 0; j < 256; j++) {
		S[j] = Rd->A;
		bn_muladd(&S[j], &S[j], 256*i + j, &Rd->B);

		bn_mut K; ed_privkey(&K, U8(&S[i]));
		ed_mul(&P[j], &K, G);
	}

	ed_norm256(P, P);
}

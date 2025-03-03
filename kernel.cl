 #include "core/bignum.c"
#include "core/ed25519.c"
#include "core/hash.c"

kernel void vanity_solana(
	global vanity_rnd* Rd,
	global vanity_tgt* Tg,
	global ed_comb G
) {
	u32 i = get_global_id(0);

	bn S[256]; xyzt P[256];
	for (u32 j = 0; j < 256; j++) {
		bn_copy(S[j], Rd->A);
		bn_muladd(S[j], S[j], 256*i + j, Rd->B);

		bn K; ed_privkey(K, U8(S));
		ed_mul(&P[j], K, G);
	}

	ed_norm256(P, P);
}

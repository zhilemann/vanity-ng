#include "core.h"
#include "sha2.h"

#define S1(x, a, b, c) (ROR(x,a) ^ ROR(x,b) ^ (x>>c))
#define S2(x, a, b, c) (ROR(x,a) ^ ROR(x,b) ^ ROR(x,c))

#define Ma(x, y, z) (x&y ^ x&z ^ y&z)
#define Ch(x, y, z) (x&y ^ ~x&z)

#define SHA2_pre(IV, t) \
	t A = IV[0], B = IV[1]; \
	t C = IV[2], D = IV[3]; \
	t E = IV[4], F = IV[5]; \
	t G = IV[6], H = IV[7];

#define SHA2_step(K, r) { \
	r(A, B, C, &D, E, F, G, &H, K[i], W[i]); \
	r(H, A, B, &C, D, E, F, &G, K[i+1], W[i+1]); \
	r(G, H, A, &B, C, D, E, &F, K[i+2], W[i+2]); \
	r(F, G, H, &A, B, C, D, &E, K[i+3], W[i+3]); \
	r(E, F, G, &H, A, B, C, &D, K[i+4], W[i+4]); \
	r(D, E, F, &G, H, A, B, &C, K[i+5], W[i+5]); \
	r(C, D, E, &F, G, H, A, &B, K[i+6], W[i+6]); \
	r(B, C, D, &E, F, G, H, &A, K[i+7], W[i+7]); \
}

#define SHA2_post(R, IV, s) \
	R[0] = s(A + IV[0]), R[1] = s(B + IV[1]); \
	R[2] = s(C + IV[2]), R[3] = s(D + IV[3]); \
	R[4] = s(E + IV[4]), R[5] = s(F + IV[5]); \
	R[6] = s(G + IV[6]), R[7] = s(H + IV[7]); \

static inline void sha256_round(
	u32 A, u32 B, u32 C, u32* D,
	u32 E, u32 F, u32 G, u32* H,
	u32 k, u32 w
) {
	u32 S0 = S2(A, 2, 13, 22), Ma = Ma(A, B, C);
	u32 S1 = S2(E, 6, 11, 25), Ch = Ch(E, F, G);

	u32 t = *H + S1 + Ch + k + w;
	*D += t, *H = t + S0 + Ma;
}

static inline void sha512_round(
	u64 A, u64 B, u64 C, u64* D,
	u64 E, u64 F, u64 G, u64* H,
	u64 k, u64 w
) {
	u64 S0 = S2(A, 28, 34, 39), Ma = Ma(A, B, C);
	u64 S1 = S2(E, 14, 18, 41), Ch = Ch(E, F, G);

	u64 t = *H + S1 + Ch + k + w;
	*D += t, *H = t + S0 + Ma;
}

/////////////////////////////////////////////////

void sha256(u32* R, const u8* X, u32 n) {
	u32 W[64] = {};
	for (u32 i = 0; i < n; i++)
		U8(W)[i] = X[i];

	U8(W)[n] = 0x80;

	for (u32 i = 0; i < 14; i++)
		W[i] = u32_bswap(W[i]);

	W[14] = 0; W[15] = 8*n;
	for (u32 i = 16; i < 64; i++) {
		W[i] = W[i-16] + S1(W[i-15], 7, 18, 3);
		W[i] += W[i-7] + S1(W[i-2], 17, 19, 10);
	}

	SHA2_pre(SHA256_IV, u32);
	for (u32 i = 0; i < 64; i += 8)
		SHA2_step(SHA256_K, sha256_round);

	SHA2_post(R, SHA256_IV, u32_bswap);
}

void sha512(u64* R, const u8* X, u32 n) {
	u64 W[80] = {};
	for (u32 i = 0; i < n; i++)
		U8(W)[i] = X[i];

	U8(W)[n] = 0x80;

	for (u32 i = 0; i < 14; i++)
		W[i] = u64_bswap(W[i]);

	W[14] = 0; W[15] = 8*n;
	for (u32 i = 16; i < 80; i++) {
		W[i] = W[i-16] + S1(W[i-15], 1, 8, 7);
		W[i] += W[i-7] + S1(W[i-2], 19, 61, 6);
	}

	SHA2_pre(SHA512_IV, u64);
	for (u32 i = 0; i < 80; i += 8)
		SHA2_step(SHA512_K, sha512_round);

    SHA2_post(R, SHA512_IV, u64_bswap);
}

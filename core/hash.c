#include "core.h"
#include "hash.h"

#define S1(x, a, b, c) (ROR(x,a) ^ ROR(x,b) ^ (x>>c))
#define S2(x, a, b, c) (ROR(x,a) ^ ROR(x,b) ^ ROR(x,c))

#define Ma(x, y, z) ((x)&(y) ^ (x)&(z) ^ (y)&(z))
#define Ch(x, y, z) ((x)&(y) ^ ~(x)&(z))

#define SHA2_init(t, IV) \
	t A = IV[0], B = IV[1], C = IV[2], D = IV[3]; \
	t E = IV[4], F = IV[5], G = IV[6], H = IV[7];

#define SHA2_step(r, K) { \
	r(A, B, C, &D, E, F, G, &H, K[i], W[i]); \
	r(H, A, B, &C, D, E, F, &G, K[i+1], W[i+1]); \
	r(G, H, A, &B, C, D, E, &F, K[i+2], W[i+2]); \
	r(F, G, H, &A, B, C, D, &E, K[i+3], W[i+3]); \
	r(E, F, G, &H, A, B, C, &D, K[i+4], W[i+4]); \
	r(D, E, F, &G, H, A, B, &C, K[i+5], W[i+5]); \
	r(C, D, E, &F, G, H, A, &B, K[i+6], W[i+6]); \
	r(B, C, D, &E, F, G, H, &A, K[i+7], W[i+7]); \
}

#define SHA2_add(t, R, s, IV) { \
	t(R)[0] = s(A + IV[0]), t(R)[1] = s(B + IV[1]); \
	t(R)[2] = s(C + IV[2]), t(R)[3] = s(D + IV[3]); \
	t(R)[4] = s(E + IV[4]), t(R)[5] = s(F + IV[5]); \
	t(R)[6] = s(G + IV[6]), t(R)[7] = s(H + IV[7]); \
}

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

void sha2_256(u8* R, const u8* X, u32 n) {
	u32 W[64] = {};

	U8(W)[n] = 0x80;
	for (u32 i = 0; i < n; i++)
		U8(W)[i] = X[i];

	W[14] = 0, W[15] = 8*n;
	for (u32 i = 0; i < 14; i++)
		W[i] = u32_bswap(W[i]);

	for (u32 i = 16; i < 64; i++) {
		W[i] = W[i-16] + S1(W[i-15], 7, 18, 3);
		W[i] += W[i-7] + S1(W[i-2], 17, 19, 10);
	}

	SHA2_init(u32, SHA256_IV);
	for (u32 i = 0; i < 64; i += 8)
		SHA2_step(sha256_round, SHA256_K);

	SHA2_add(U32, R, u32_bswap, SHA256_IV);
}

void sha2_512(u8* R, const u8* X, u32 n) {
	u64 W[80] = {};

	U8(W)[n] = 0x80;
	for (u32 i = 0; i < n; i++)
		U8(W)[i] = X[i];

	W[14] = 0, W[15] = 8*n;
	for (u32 i = 0; i < 14; i++)
		W[i] = u64_bswap(W[i]);

	for (u32 i = 16; i < 80; i++) {
		W[i] = W[i-16] + S1(W[i-15], 1, 8, 7);
		W[i] += W[i-7] + S1(W[i-2], 19, 61, 6);
	}

	SHA2_init(u64, SHA512_IV);
	for (u32 i = 0; i < 80; i += 8)
		SHA2_step(sha512_round, SHA512_K);

	SHA2_add(U64, R, u64_bswap, SHA512_IV);
}

/////////////////////////////////////////////////

#define U(i) (u[(i)%5])
#define FOR_x() for (u32 x = 0; x < 5; x++)
#define FOR_y() for (u32 y = 0; y < 25; y += 5)

static void sha3_round(u64* H, u32 i) {
	u64 u[5], v;
	FOR_x() {
		u[x] = 0;
		FOR_y() u[x] ^= H[x+y];
	}

	FOR_x() {
		v = U(x+4) ^ ROL(U(x+1), 1);
		FOR_y() H[x+y] ^= v;
	}

	v = H[1];
	for (u32 i = 0; i < 24; i++) {
		v = ROL(v, KECCAK_RHO[i]);
		SWAP(H[KECCAK_PI[i]], v);
	}

	FOR_y() {
		FOR_x() u[x] = H[x+y];
		FOR_x() H[x+y] ^= ~U(x+1) & U(x+2);
	}

	H[0] ^= KECCAK_K[i];
}

void sha3_256(u8* R, const u8* X, u32 n) {
	u64 H[25] = {};
	for (u32 i = 0; i < n; i++)
		U8(H)[i] = X[i];

	U8(H)[n] ^= 1;
	U8(H)[135] ^= 0x80;

	for (u32 i = 0; i < 24; i++)
		sha3_round(H, i);

	for (u32 i = 0; i < 32; i++)
		R[i] = U8(H)[i];
}

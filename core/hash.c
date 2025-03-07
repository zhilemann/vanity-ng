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

/////////////////////////////////////////////////

#define RIPEMD_step1(A, B, C, D, E, i, f) { \
	A += f(B, C, D) + M[RIPEMD_I1[i]] + RIPEMD_K1[(i)/16]; \
	A = ROL(A, RIPEMD_R1[i]) + E, C = ROL(C, 10); \
}

#define RIPEMD_step2(A, B, C, D, E, i, f) { \
	A += f(B, C, D) + M[RIPEMD_I2[i]] + RIPEMD_K2[(i)/16]; \
	A = ROL(A, RIPEMD_R2[i]) + E, C = ROL(C, 10); \
}

#define RIPEMD_round1(i, f1, f2) { \
	RIPEMD_step1(A1, B1, C1, D1, E1, i, f1); \
	RIPEMD_step2(A2, B2, C2, D2, E2, i, f2); \
}

#define RIPEMD_round2(i, f1, f2) { \
	RIPEMD_step1(E1, A1, B1, C1, D1, i, f1); \
	RIPEMD_step2(E2, A2, B2, C2, D2, i, f2); \
}

#define RIPEMD_round3(i, f1, f2) { \
	RIPEMD_step1(D1, E1, A1, B1, C1, i, f1); \
	RIPEMD_step2(D2, E2, A2, B2, C2, i, f2); \
}

#define RIPEMD_round4(i, f1, f2) { \
	RIPEMD_step1(C1, D1, E1, A1, B1, i, f1); \
	RIPEMD_step2(C2, D2, E2, A2, B2, i, f2); \
}

#define RIPEMD_round5(i, f1, f2) { \
	RIPEMD_step1(B1, C1, D1, E1, A1, i, f1); \
	RIPEMD_step2(B2, C2, D2, E2, A2, i, f2); \
}

#define RIPEMD_rounds(a, b, c, d, e, f1, f2) { \
	RIPEMD_round##a(i, f1, f2); \
	RIPEMD_round##b(i+1, f1, f2); \
	RIPEMD_round##c(i+2, f1, f2); \
	RIPEMD_round##d(i+3, f1, f2); \
	RIPEMD_round##e(i+4, f1, f2); \
}

/////////////////////////////////////////////////

void ripemd160(u8* R, const u8* X, u32 n) {
	u32 M[16] = {};
	M[14] = 8*n, M[15] = 0;

	U8(M)[n] = 0x80;
	for (u32 i = 0; i < n; i++)
		U8(M)[i] = X[i];

	u32 A1 = RIPEMD_IV[0], A2 = A1;
	u32 B1 = RIPEMD_IV[1], B2 = B1;
	u32 C1 = RIPEMD_IV[2], C2 = C1;
	u32 D1 = RIPEMD_IV[3], D2 = D1;
	u32 E1 = RIPEMD_IV[4], E2 = E1;

	for (u32 i = 0; i < 15; i += 5)
		RIPEMD_rounds(1, 2, 3, 4, 5, RIPEMD_f1, RIPEMD_f5);

	RIPEMD_round1(15, RIPEMD_f1, RIPEMD_f5);
	for (u32 i = 16; i < 31; i += 5)
		RIPEMD_rounds(2, 3, 4, 5, 1, RIPEMD_f2, RIPEMD_f4);

	RIPEMD_round2(31, RIPEMD_f2, RIPEMD_f4);
	for (u32 i = 32; i < 47; i += 5)
		RIPEMD_rounds(3, 4, 5, 1, 2, RIPEMD_f3, RIPEMD_f3);

	RIPEMD_round3(47, RIPEMD_f3, RIPEMD_f3);
	for (u32 i = 48; i < 63; i += 5)
		RIPEMD_rounds(4, 5, 1, 2, 3, RIPEMD_f4, RIPEMD_f2);

	RIPEMD_round4(63, RIPEMD_f4, RIPEMD_f2);
	for (u32 i = 64; i < 79; i += 5)
		RIPEMD_rounds(5, 1, 2, 3, 4, RIPEMD_f5, RIPEMD_f1);

	RIPEMD_round5(79, RIPEMD_f5, RIPEMD_f1);

	U32(R)[0] = C1 + D2 + RIPEMD_IV[1];
	U32(R)[1] = D1 + E2 + RIPEMD_IV[2];
	U32(R)[2] = E1 + A2 + RIPEMD_IV[3];
	U32(R)[3] = A1 + B2 + RIPEMD_IV[4];
	U32(R)[4] = B1 + C2 + RIPEMD_IV[0];
}

/////////////////////////////////////////////////

typedef struct { u32 a; u8 b; } base32_40;

static u32 bech32_add(u32 h, u8 x) {
	u8 b = h >> 25;
	h = (h % (1<<25)) << 5 ^ (x%32);

	for (u32 i = 0; i < 5; i++)
		if ((b >> i) & 1)
			h ^= BECH32_GEN[i];

	return h;
}

void bech32(u8* R, u8* X, bech32_cfg C) {
	u32 h = 1;

	h = bech32_add(h, C.H[0] >> 5);
	h = bech32_add(h, C.H[1] >> 5);
	h = bech32_add(h, 0);

	h = bech32_add(h, C.H[0]);
	h = bech32_add(h, C.H[1]);
	h = bech32_add(h, C.W);

	for (u32 i = 0; i < 20; i += 5) {
		base32_40 x = *(base32_40*)(X+i);
		x.a = u32_bswap(x.a);

		h = bech32_add(h, x.a >> 27);
		h = bech32_add(h, x.a >> 22);
		h = bech32_add(h, x.a >> 17);
		h = bech32_add(h, x.a >> 12);
		h = bech32_add(h, x.a >> 7);
		h = bech32_add(h, x.a >> 2);
		h = bech32_add(h, x.a<<3 | x.b>>5);
		h = bech32_add(h, x.b);
	}

	for (u32 i = 0; i < 6; i++)
		h = bech32_add(h, 0);

	h ^= 1;
	for (u32 i = 0; i < 6; i++)
		R[i] = (h >> (25 - 5*i)) % 32;
}

#include "core.h"
#include "sha2.h"

#define MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define CH(x, y, z) ((x & y) ^ (~x & z))

static void sha512_sched(u64 W[80], u32 i) {
	u64 s0 = ROR(W[i-15], 1);
	s0 ^= ROR(W[i-15], 8) ^ (W[i-15] >> 7);

	u64 s1 = ROR(W[i-2], 19);
	s1 ^= ROR(W[i-2], 61) ^ (W[i-2] >> 6);

	W[i] = W[i-16] + s0 + W[i-7] + s1;
}

static void sha512_round(u64 X[8], const u64 W[80], u32 i) {
	u64 S0 = ROR(X[0], 28) ^ ROR(X[0], 34) ^ ROR(X[0], 39);
	u64 maj = MAJ(X[0], X[1], X[2]);

	u64 S1 = ROR(X[4], 14) ^ ROR(X[4], 18) ^ ROR(X[4], 41);
	u64 ch = CH(X[4], X[5], X[6]);

	u64 t = X[7] + S1 + ch + SHA512_K[i] + W[i];

	for (u32 i = 7; i+1 > 1; i--)
		X[i] = X[i-1];

	X[0] = t + S0 + maj; X[4] += t;
}

void sha512(u64 R[8], const u8 X[], u32 n) {
	u64 W[80] = {};
	for (u32 i = 0; i < n; i++)
		U8(W)[i] = X[i];

	U8(W)[n] = 0x80;

	for (u32 i = 0; i < 14; i++)
		W[i] = u64_bswap(W[i]);

	W[14] = 0; W[15] = 8*n;
	for (u32 i = 16; i < 80; i++)
		sha512_sched(W, i);

	u64 H[8];
	for (u32 i = 0; i < 8; i++)
		H[i] = SHA512_IV[i];

	for (u32 i = 0; i < 80; i++)
		sha512_round(H, W, i);

	for (u32 i = 0; i < 8; i++)
		R[i] = u64_bswap(H[i] + SHA512_IV[i]);
}

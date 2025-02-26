#include "core.h"
#include "const.h"

static u64 U64_SWAP1 = 0xff00ff00ff00ff00;
static u64 U64_SWAP2 = 0xffff0000ffff0000;

static u64 u64_swap(u64 x) {
	x = ((x << 8) & U64_SWAP1) | ((x >> 8) & ~U64_SWAP1);
	x = ((x << 16) & U64_SWAP2) | ((x >> 16) & ~U64_SWAP2);
    return (x << 32) | (x >> 32);
}

static u64 u64_ror(u64 x, u32 n) {
	return (x >> n) | (x << (64-n));
}

static u64 sha512_s0(u64 x) {
	return u64_ror(x, 1) ^ u64_ror(x, 8) ^ (x >> 7);
}

static u64 sha512_s1(u64 x) {
	return u64_ror(x, 19) ^ u64_ror(x, 61) ^ (x >> 6);
}

static u64 sha512_S0(u64 x) {
	return u64_ror(x, 28) ^ u64_ror(x, 34) ^ u64_ror(x, 39);
}

static u64 sha512_S1(u64 x) {
	return u64_ror(x, 14) ^ u64_ror(x, 18) ^ u64_ror(x, 41);
}

static u64 sha512_maj(u64 x, u64 y, u64 z) {
	return (x & y) ^ (x & z) ^ (y & z);
}

static u64 sha512_ch(u64 x, u64 y, u64 z) {
	return (x & y) ^ ((~x) & z);
}

void sha512_pad(u8 R[128], u32 n) {
	u64* R_64 = (u64*)R;

	R[n] = 0x80;
	R_64[14] = 0;
	R_64[15] = 8*n;

	for (u32 i = 0; i < 14; i++)
		R_64[i] = u64_swap(R_64[i]);
}

static void sha512_round(u64 X[8], const u64 W[80], u32 i) {
	u64 S0 = sha512_S0(X[0]);
	u64 maj = sha512_maj(X[0], X[1], X[2]);

	u64 S1 = sha512_S1(X[4]);
	u64 ch = sha512_ch(X[4], X[5], X[6]);

	u64 t = X[7] + S1 + ch + SHA512_K[i] + W[i];

	for (u32 i = 7; i+1 > 0; i--)
		X[i] = X[i-1];

	X[0] = t + S0 + maj; X[4] += t;
}

void sha512(u64 R[8], const u8 X[128]) {
	u64 W[80] = {}, H[8];
	for (u32 i = 0; i < 16; i++)
		W[i] = ((u64*)X)[i];

	for (u32 i = 16; i < 80; i++) {
		u64 s0 = sha512_s0(W[i-15]);
		u64 s1 = sha512_s1(W[i-2]);

		W[i] = W[i-16] + s0 + W[i-7] + s1;
	}

	for (u32 i = 0; i < 8; i++)
		H[i] = SHA512_IV[i];

	for (u32 i = 0; i < 80; i++)
		sha512_round(H, W, i);

	for (u32 i = 0; i < 8; i++)
		R[i] = H[i] + SHA512_IV[i];
}

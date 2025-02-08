#include "core.h"

static u32 u256_width(const u32 X[8]) {
	u32 n = 7;
	while (X[n] == 0) n--;
	return n;
}

static u32 u256_div_k
(const u32 X[8], u32 i, const u32 y) {
	u64 x = X[i];
	if (i < 7) x |= (u64)X[i+1] << 32;
	return x / ((u64)y + 1);
}

static void u256_shl1(u32 A[8], const u32 X[8]) {
	for (u32 i = 7; i > 0; i--)
		A[i] = (X[i] << 1) | (X[i-1] >> 31);

	A[0] = X[0] << 1;
}

static void u256_shl32n
(u32 A[8], const u32 X[8], u32 n) {
	for (u32 i = 7; i+1 > n; i--)
		A[i] = X[i-n];

	for (u32 i = 0; i < n; i++)
		A[i] = 0;
}

static void u256_shr32_ext
(u32 A[8], const u32 X[8], u32 ext) {
	for (u32 i = 0; i < 7; i++)
		A[i] = X[i + 1];

	A[7] = ext;
}

// Handbook of Applied Cryptography, 14.32
void monty_redc
(u32 A[8], const u32 X[8], const monty *Mo) {
	u32 T[16] = {};
	u256_copy(T, X);

	for (u32 i = 0; i < 8; i++) {
		u32 k = T[i] * Mo->M_inv32;
		T[i+8] += u256_muladd(T+i, T+i, Mo->M, k);
	}

	if (u256_cmp(T+8, Mo->M) > LT)
		u256_sub(T+8, T+8, Mo->M);

	u256_copy(A, T+8);
}

// see Handbook of Applied Cryptography, 14.36
void monty_mul(
	u32 A[8], const u32 X[8],
	const u32 Y[8], const monty *Mo
) {
	u32 T[8] = {}, cf = 0;

	u32 ext1, ext2;
	for (u32 i = 0; i < 8; i++) {
		u32 k = (T[0] + X[i]*Y[0]) * Mo->M_inv32;

		ext1 = u256_muladd(T, T, Y, X[i]);
		ext2 = u256_muladd(T, T, Mo->M, k);

		u32 ext = u32_adc(ext1, ext2, cf, &cf);
		u256_shr32_ext(T, T, ext);
	}

	if (cf || u256_cmp(T, Mo->M) > LT)
		u256_sub(T, T, Mo->M);

	u256_copy(A, T);
}

// long division with clever guessing
void u256_divmod(
	u32 A[8], u32 B[8],
	const u32 X[8], const u32 Y[8]
) {
	u256_fill(A, 0); u256_copy(B, X);
	if (u256_cmp(B, Y) == LT) return;

	u32 Y1[8] = {};
	u32 n = u256_width(B);
	u32 m = u256_width(Y);

	for (u32 i = n; i+1 > m; i--) {
		u256_shl32n(Y1, Y, i-m);
		while (u256_cmp(B, Y1) > LT) {
			u32 k = u256_div_k(B, i, Y[m]);
			if (k > 0) {
				A[i-m] += k;
				u256_mulsub(B, B, Y1, k);
			} else {
				A[i-m] += 1;
				u256_sub(B, B, Y1);
			}
		}
	}
}

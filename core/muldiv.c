#include "core.h"

static u32 u256_width(const u32 X[8]) {
	u32 n = 7;
	while (X[n] == 0) n--;
	return n;
}

static u64 u256_get64(const u32 X[8], u32 i) {
	u64 x = X[i];
	if (i < 7) x |= (u64)X[i+1] << 32;
	return x;
}

// `R = X >> (32*n)`
static void u256_shl32n(
	u32 R[8], const u32 X[8], u32 n
) {
	for (u32 i = 7; i+1 > n; i--)
		R[i] = X[i-n];

	for (u32 i = 0; i < n; i++)
		R[i] = 0;
}

// `R += k*X`
u32 u256_muladd(u32 R[8], u32 k, const u32 X[8]) {
	u32 ext = 0, cf1 = 0, cf2 = 0;
	for (u32 i = 0; i < 8; i++) {
		u32 x = u32_adc(k * X[i], ext, 0, &cf1);
		R[i] = u32_adc(R[i], x, 0, &cf2);
		ext = mul_hi(k, X[i]) + cf1 + cf2;
	}

	return ext;
}

// `R -= k*X`
static u32 u256_mulsub(u32 R[8], u32 k, const u32 X[8]) {
	u32 ext = 0, cf = 0, bf = 0;
	for (u32 i = 0; i < 8; i++) {
		u32 x = u32_adc(k * X[i], ext, 0, &cf);
		R[i] = u32_sbb(R[i], x, 0, &bf);
		ext = mul_hi(k, X[i]) + cf + bf;
	}

	return ext;
}

void u256_mul(u32 R[8], const u32 X[8], const u32 Y[8]) {
	u32 T[16] = {};
	for (u32 i = 0; i < 8; i++)
		u256_muladd(T+i, X[i], Y);

	u256_copy(R, T);
}

void u256_pow32(u32 R[8], const u32 X[8], u32 k) {
	u256_zero(R); R[0] = 1;

	u32 T[8]; u256_copy(T, X);
	while (k > 0) {
		if (k & 1) u256_mul(R, R, T);
		u256_mul(T, T, T); k >>= 1;
	}
}

void u256_divmod(
	u32 Rq[8], u32 Rr[8],
	const u32 X[8], const u32 Y[8]
) {
	u256_zero(Rq); u256_copy(Rr, X);
	if (u256_cmp(X, Y) == LT) return;

	u32 n = u256_width(X);
	u32 m = u256_width(Y);

	u32 Y1[8];
	for (u32 i = n; i+1 > m; i--) {
		u256_shl32n(Y1, Y, i-m);

		while (u256_cmp(Rr, Y1) > LT) {
			// guess the quotient digit
			// see Handbook of Applied Cryptography, 14.20
			u32 k = u256_get64(Rr, i) / ((u64)Y[m] + 1);

			if (k > 0) {
				Rq[i-m] += k;
				u256_mulsub(Rr, k, Y1);
			} else {
				Rq[i-m] += 1;
				u256_sub(Rr, Rr, Y1);
			}
		}
	}
}

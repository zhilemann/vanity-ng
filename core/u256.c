#include "core.h"

#if !defined(__OPENCL_VERSION__)
	u32 mul_hi(u32 x, u32 y) {
		return (x * (u64)y) >> 32;
	}
#endif

u32 u32_adc(u32 x, u32 y, u32 cf, u32 *cf1) {
	u32 z = x + y + cf;
	*cf1 = (z < x) || (cf && (z == x));
	return z;
}

u32 u32_sbb(u32 x, u32 y, u32 bf, u32 *bf1) {
	u32 z = x - y - bf;
	*bf1 = (z > x) || (bf && (z == x));
	return z;
}

void u256_fill(u32 A[8], u32 x) {
	for (u32 i = 0; i < 8; i++)
		A[i] = x;
}

void u256_copy(u32 A[8], const u32 X[8]) {
	for (u32 i = 0; i < 8; i++)
		A[i] = X[i];
}

ord u256_cmp(const u32 X[8], const u32 Y[8]) {
	u32 lt = 0, gt = 0;
	for (u32 i = 0; i < 8; i++) {
		if (X[i] < Y[i]) lt |= (1 << i);
		if (X[i] > Y[i]) gt |= (1 << i);
	}

	if (lt > gt) return LT;
	else if (lt < gt) return GT;
	else return EQ;
}

u32 u256_add(u32 A[8], const u32 X[8], const u32 Y[8]) {
	u32 cf = 0;
	for (u32 i = 0; i < 8; i++)
		A[i] = u32_adc(X[i], Y[i], cf, &cf);

	return cf;
}

u32 u256_sub(u32 A[8], const u32 X[8], const u32 Y[8]) {
	u32 bf = 0;
	for (u32 i = 0; i < 8; i++)
		A[i] = u32_sbb(X[i], Y[i], bf, &bf);

	return bf;
}

void u256_modadd(
	u32 A[8], const u32 X[8],
	const u32 Y[8], const u32 M[8]
) {
	u32 cf = u256_add(A, X, Y);
	if (cf || u256_cmp(A, M) > LT)
		u256_sub(A, A, M);
}

void u256_modsub(
	u32 A[8], const u32 X[8],
	const u32 Y[8], const u32 M[8]
) {
	u32 bf = u256_sub(A, X, Y);
	if (bf) u256_add(A, A, M);
}

u32 u256_muladd(
	u32 A[8], const u32 X[8],
	const u32 Y[8], u32 k
) {
	u32 ext = 0, cf1 = 0, cf2 = 0;
	for (u32 i = 0; i < 8; i++) {
		u32 y = u32_adc(k * Y[i], ext, 0, &cf1);

		A[i] = u32_adc(X[i], y, 0, &cf2);
		ext = mul_hi(k, Y[i]) + cf1 + cf2;
	}

	return ext;
}

u32 u256_mulsub(
	u32 A[8], const u32 X[8],
	const u32 Y[8], u32 k
) {
	u32 ext = 0, cf = 0, bf = 0;
	for (u32 i = 0; i < 8; i++) {
		u32 y = u32_adc(k * Y[i], ext, 0, &cf);

		A[i] = u32_sbb(X[i], y, 0, &bf);
		ext = mul_hi(k, Y[i]) + cf + bf;
	}

	return ext;
}

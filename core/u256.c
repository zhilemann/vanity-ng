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

void u256_zero(u32 R[8]) {
	for (u32 i = 0; i < 8; i++)
		R[i] = 0;
}

void u256_copy(u32 R[8], const u32 X[8]) {
	for (u32 i = 0; i < 8; i++)
		R[i] = X[i];
}

u32 u256_is_zero(const u32 X[8]) {
	u32 x = 0;
	for (u32 i = 0; i < 8; i++)
		x |= X[i];

	return x == 0;
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

u32 u256_add(u32 R[8], const u32 X[8], const u32 Y[8]) {
	u32 cf = 0;
	for (u32 i = 0; i < 8; i++)
		R[i] = u32_adc(X[i], Y[i], cf, &cf);

	return cf;
}

u32 u256_sub(u32 R[8], const u32 X[8], const u32 Y[8]) {
	u32 bf = 0;
	for (u32 i = 0; i < 8; i++)
		R[i] = u32_sbb(X[i], Y[i], bf, &bf);

	return bf;
}

void u256_modadd(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8],
	const u32 M[8]
) {
	u32 cf = u256_add(R, X, Y);
	if (cf || u256_cmp(R, M) > LT)
		u256_sub(R, R, M);
}

void u256_modsub(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8],
	const u32 M[8]
) {
	u32 bf = u256_sub(R, X, Y);
	if (bf) u256_add(R, R, M);
}


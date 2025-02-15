#include "core.h"

// `R = (ext ...X) >> 32`
static void u256_shr32_ext(
	u32 R[8], const u32 X[8], u32 ext
) {
	for (u32 i = 0; i < 7; i++)
		R[i] = X[i + 1];

	R[7] = ext;
}

// `R += k*X` (from `mulpow.c`)
u32 u256_muladd(u32 R[8], u32 k, const u32 X[8]);

void monty_init(monty* Mo, const u32 M[8]) {
	u256_copy(Mo->M, M);

	u32 T[8] = { 0, 1 }; // T = 2^32
	u256_modinv(T, M, T);
	Mo->inv32 = T[0] * -1;

	u256_zero(Mo->R2); Mo->R2[0] = 1;
	for (u32 i = 0; i < 512; i++)
		// Mo->R2 = (Mo->R2 << 1) % M
		u256_modadd(Mo->R2, Mo->R2, Mo->R2, M);

	monty_mul(Mo->R3, Mo->R2, Mo->R2, Mo);
}

void monty_inj(
	u32 R[8], const u32 X[8],
	const monty* Mo
) {
	monty_mul(R, X, Mo->R2, Mo);
}

void monty_redc(
	u32 R[8], const u32 X[8],
	const monty *Mo
) {
	// see Handbook of Applied Cryptography, 14.32
	u32 T[16] = {};
	u256_copy(T, X);

	for (u32 i = 0; i < 8; i++) {
		u32 k = T[i] * Mo->inv32;
		T[i+8] += u256_muladd(T+i, k, Mo->M);
	}

	if (u256_cmp(T+8, Mo->M) > LT)
		u256_sub(T+8, T+8, Mo->M);

	u256_copy(R, T+8);
}

void monty_mul(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8],
	const monty *Mo
) {
	// see Handbook of Applied Cryptography, 14.36
	u32 T[8] = {}, cf = 0;

	u32 ext1, ext2;
	for (u32 i = 0; i < 8; i++) {
		u32 k = (T[0] + X[i]*Y[0]) * Mo->inv32;

		ext1 = u256_muladd(T, X[i], Y);
		ext2 = u256_muladd(T, k, Mo->M);

		u32 ext = u32_adc(ext1, ext2, cf, &cf);
		u256_shr32_ext(T, T, ext);
	}

	if (cf || u256_cmp(T, Mo->M) > LT)
		u256_sub(T, T, Mo->M);

	u256_copy(R, T);
}

void monty_inv(
	u32 R[8], const u32 X[8],
	const monty* Mo
) {
	u256_modinv(R, X, Mo->M);
	monty_mul(R, R, Mo->R3, Mo);
}

#include "core.h"

typedef struct {
	u32 X[8], A[8], B[8];
	u32 extA, extB;
} u256_gcd_st;

static u32 u256_is_zero(const u32 X[8]) {
	u32 x = 0;
	for (u32 i = 0; i < 8; i++)
		x |= X[i];

	return x == 0;
}

static void u256_shr1_ext
(u32 A[8], const u32 X[8], u32 ext) {
	for (u32 i = 0; i < 7; i++)
		A[i] = (X[i] >> 1) | (X[i+1] << 31);

	A[7] = (X[7] >> 1) | (ext << 31);
}

static u32 u256_sub_ext(
	u32 A[8],
	const u32 X[8], u32 extX,
	const u32 Y[8], u32 extY
) {
	u32 bf = u256_sub(A, X, Y);
	return extX - extY - bf;
}

static u32 u256_neg(u32 A[8], const u32 X[8], u32 extX) {
	u32 cf = 1;
	for (u32 i = 0; i < 8; i++)
		A[i] = u32_adc(~X[i], 0, cf, &cf);

	return u32_adc(~extX, 0, cf, &cf);
}

// see Handbook of Applied Cryptography, 14.61
static void u256_gcd_step(
	u256_gcd_st *st,
	const u32 X[8], const u32 Y[8]
) {
	while (!(st->X[0] & 1)) {
		u256_shr1_ext(st->X, st->X, 0);

		if ((st->A[0] | st->B[0]) & 1) {
			st->extA += u256_add(st->A, st->A, Y);
			st->extB -= u256_sub(st->B, st->B, X);
		}

		u256_shr1_ext(st->A, st->A, st->extA);
		st->extA = (int)st->extA >> 1;

		u256_shr1_ext(st->B, st->B, st->extB);
		st->extB = (int)st->extB >> 1;
	}
}

// see Handbook of Applied Cryptography, 14.61
static void u256_gcd_swap
(u256_gcd_st *st1, u256_gcd_st *st2) {
	if (u256_cmp(st1->X, st2->X) == LT) {
		u256_gcd_st *tmp = st1;
		st1 = st2; st2 = tmp;
	}

	u256_sub(st1->X, st1->X, st2->X);
	st1->extA = u256_sub_ext(
		st1->A,
		st1->A, st1->extA,
		st2->A, st2->extA
	);

	st1->extB = u256_sub_ext(
		st1->B,
		st1->B, st1->extB,
		st2->B, st2->extB
	);
}

// see Handbook of Applied Cryptography, 14.61
void u256_modinv
(u32 A[8], const u32 X[8], const u32 M[8]) {
	u256_gcd_st st1 = { {}, { 1 }, {} };
	u256_gcd_st st2 = { {}, {}, { 1 } };

	u256_copy(st1.X, X);
	u256_copy(st2.X, M);

	while (!u256_is_zero(st1.X)) {
		u256_gcd_step(&st1, X, M);
		u256_gcd_step(&st2, X, M);
		u256_gcd_swap(&st1, &st2);
	}

	u32 ext = st2.extA;
	u256_copy(A, st2.A);

	u32 is_neg = (int)ext < 0;
	if (is_neg) ext = u256_neg(A, A, ext);

	while (ext > 0 || u256_cmp(A, M) > LT)
		ext = u256_sub_ext(A, A, ext, M, 0);

	if (is_neg) u256_sub(A, M, A);
}

#include "core.h"

typedef struct {
	u32 X[8], A[8], B[8];
	u32 extA, extB;
} u256_gcd;

// `R = (ext ...X) >> 1`
static void u256_shr1_ext(
	u32 R[8], const u32 X[8], u32 ext
) {
	for (u32 i = 0; i < 7; i++)
		R[i] = (X[i] >> 1) | (X[i+1] << 31);

	R[7] = (X[7] >> 1) | (ext << 31);
}

// `(retval ...R) = (extX ...X) - (extY ...Y)`
static u32 u256_sub_ext(
	u32 R[8],
	const u32 X[8], u32 extX,
	const u32 Y[8], u32 extY
) {
	u32 bf = u256_sub(R, X, Y);
	return extX - extY - bf;
}

// `(retval ...R) = -(extX ...X)`
static u32 u256_neg(
	u32 R[8], const u32 X[8], u32 extX
) {
	u32 cf = 1;
	for (u32 i = 0; i < 8; i++)
		R[i] = u32_adc(~X[i], 0, cf, &cf);

	return u32_adc(~extX, 0, cf, &cf);
}

static void u256_gcd_step(
	u256_gcd *gcd,
	const u32 X[8], const u32 Y[8]
) {
	// see Handbook of Applied Cryptography, 14.61
	while (!(gcd->X[0] & 1)) {
		u256_shr1_ext(gcd->X, gcd->X, 0);

		if ((gcd->A[0] | gcd->B[0]) & 1) {
			gcd->extA += u256_add(gcd->A, gcd->A, Y);
			gcd->extB -= u256_sub(gcd->B, gcd->B, X);
		}

		u256_shr1_ext(gcd->A, gcd->A, gcd->extA);
		gcd->extA = (int)gcd->extA >> 1;

		u256_shr1_ext(gcd->B, gcd->B, gcd->extB);
		gcd->extB = (int)gcd->extB >> 1;
	}
}

static void u256_gcd_swap(
	u256_gcd *gcd1, u256_gcd *gcd2
) {
	// see Handbook of Applied Cryptography, 14.61
	if (u256_cmp(gcd1->X, gcd2->X) == LT) {
		u256_gcd *tmp = gcd1;
		gcd1 = gcd2; gcd2 = tmp;
	}

	u256_sub(gcd1->X, gcd1->X, gcd2->X);
	gcd1->extA = u256_sub_ext(
		gcd1->A,
		gcd1->A, gcd1->extA,
		gcd2->A, gcd2->extA
	);

	gcd1->extB = u256_sub_ext(
		gcd1->B,
		gcd1->B, gcd1->extB,
		gcd2->B, gcd2->extB
	);
}

void u256_modinv(
	u32 R[8], const u32 X[8],
	const u32 M[8]
) {
	// see Handbook of Applied Cryptography, 14.61
	u256_gcd gcd1 = { {}, { 1 }, {} };
	u256_gcd gcd2 = { {}, {}, { 1 } };

	u256_copy(gcd1.X, X);
	u256_copy(gcd2.X, M);

	while (!u256_is_zero(gcd1.X)) {
		u256_gcd_step(&gcd1, X, M);
		u256_gcd_step(&gcd2, X, M);
		u256_gcd_swap(&gcd1, &gcd2);
	}

	u32 T[8]; u256_copy(T, gcd2.A);
	u32 ext = gcd2.extA;

	u32 is_neg = (int)ext < 0;
	if (is_neg) ext = u256_neg(T, T, ext);

	while (ext > 0 || u256_cmp(T, M) > LT)
		ext = u256_sub_ext(T, T, ext, M, 0);

	if (is_neg) u256_sub(T, M, T);

	u256_copy(R, T);
}

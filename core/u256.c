#include "core.h"

#if !defined(__OPENCL_VERSION__)
	u32 mul_hi(u32 x, u32 y) {
		return (x * (u64)y) >> 32;
	}
#endif

typedef struct {
	u32 X[8], A[8], B[8];
	u32 extA, extB;
} u256_gcd;

static u32 u32_adc(u32 x, u32 y, u32 cf, u32 *cf1) {
	u32 z = x + y + cf;
	*cf1 = (z < x) || (cf && (z == x));
	return z;
}

static u32 u32_sbb(u32 x, u32 y, u32 bf, u32 *bf1) {
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

// `(retval ...R) = -(extX ...X)`
static u32 u256_neg(u32 R[8], const u32 X[8], u32 extX) {
	u32 cf = 1;
	for (u32 i = 0; i < 8; i++)
		R[i] = u32_adc(~X[i], 0, cf, &cf);

	return u32_adc(~extX, 0, cf, &cf);
}

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

static void u256_shr1(u32 R[8], const u32 X[8], u32 ext) {
	for (u32 i = 0; i < 7; i++)
		R[i] = (X[i] >> 1) | (X[i+1] << 31);

	R[7] = (X[7] >> 1) | (ext << 31);
}

static void u256_shr32(u32 R[8], const u32 X[8], u32 ext) {
	for (u32 i = 0; i < 7; i++)
		R[i] = X[i + 1];

	R[7] = ext;
}

// `R = X >> (32*n)`
static void u256_shl32n(u32 R[8], const u32 X[8], u32 n) {
	for (u32 i = 7; i+1 > n; i--)
		R[i] = X[i-n];

	for (u32 i = 0; i < n; i++)
		R[i] = 0;
}

/////////////////////////////////////////////////

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

// `(retval ...R) = (extX ...X) - (extY ...Y)`
static u32 u256_sub_ext(
	u32 R[8],
	const u32 X[8], u32 extX,
	const u32 Y[8], u32 extY
) {
	u32 bf = u256_sub(R, X, Y);
	return extX - extY - bf;
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

// `R += k*X`
static u32 u256_muladd(u32 R[8], u32 k, const u32 X[8]) {
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

/////////////////////////////////////////////////

static void u256_gcd_step(
	u256_gcd *gcd,
	const u32 X[8], const u32 Y[8]
) {
	while (!(gcd->X[0] & 1)) {
		u256_shr1(gcd->X, gcd->X, 0);

		if ((gcd->A[0] | gcd->B[0]) & 1) {
			gcd->extA += u256_add(gcd->A, gcd->A, Y);
			gcd->extB -= u256_sub(gcd->B, gcd->B, X);
		}

		u256_shr1(gcd->A, gcd->A, gcd->extA);
		gcd->extA = (int)gcd->extA >> 1;

		u256_shr1(gcd->B, gcd->B, gcd->extB);
		gcd->extB = (int)gcd->extB >> 1;
	}
}

static void u256_gcd_swap(u256_gcd *gcd1, u256_gcd *gcd2) {
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

void u256_modinv(u32 R[8], const u32 X[8], const u32 M[8]) {
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

//////////////////////////////////////////////////

void u256_mul(u32 R[8], const u32 X[8], const u32 Y[8]) {
	u32 T[16] = {};
	for (u32 i = 0; i < 8; i++)
		u256_muladd(T+i, X[i], Y);

	u256_copy(R, T);
}

void u256_pow32(u32 R[8], const u32 X[8], u32 k) {
	u256_zero(R); R[0] = 1;

	u32 B[8]; u256_copy(B, X);
	while (k > 0) {
		if (k & 1) u256_mul(R, R, B);
		u256_mul(B, B, B); k >>= 1;
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

/////////////////////////////////////////////////

void monty_init(monty_ctx* Mo, const u32 M[8]) {
	u256_copy(Mo->M, M);

	u32 T[8] = { 0, 1 }; // T = 2^32
	u256_modinv(T, M, T);
	Mo->inv32 = T[0] * -1;

	u256_zero(Mo->R2); Mo->R2[0] = 1;
	for (u32 i = 0; i < 512; i++)
		// Mo->R2 = (Mo->R2 << 1) % M
		u256_modadd(Mo->R2, Mo->R2, Mo->R2, M);

	monty_redc(Mo->_1, Mo->R2, Mo);
	monty_mul(Mo->R3, Mo->R2, Mo->R2, Mo);
}

void monty_inj(
	u32 R[8], const u32 X[8],
	const monty_ctx* Mo
) {
	monty_mul(R, X, Mo->R2, Mo);
}

void monty_redc(
	u32 R[8], const u32 X[8],
	const monty_ctx *Mo
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
	const monty_ctx *Mo
) {
	// see Handbook of Applied Cryptography, 14.36
	u32 T[8] = {}, cf = 0;

	u32 ext1, ext2;
	for (u32 i = 0; i < 8; i++) {
		u32 k = (T[0] + X[i]*Y[0]) * Mo->inv32;

		ext1 = u256_muladd(T, X[i], Y);
		ext2 = u256_muladd(T, k, Mo->M);

		u32 ext = u32_adc(ext1, ext2, cf, &cf);
		u256_shr32(T, T, ext);
	}

	if (cf || u256_cmp(T, Mo->M) > LT)
		u256_sub(T, T, Mo->M);

	u256_copy(R, T);
}

void monty_invN(
	u32 R[][8], const u32 X[][8],
	u32 n, u32 st, const monty_ctx* Mo
) {
	u32 T[n][8], I[8];

	u256_copy(T[0], X[0]);
	for (u32 i = 1; i < n; i++)
		// `T[i] ~= X[0] * X[1] * ... * X[i]` (mod M)
		monty_mul(T[i], T[i-1], X[st*i], Mo);

	u256_modinv(I, T[n-1], Mo->M);
	monty_mul(I, I, Mo->R3, Mo);

	for (u32 i = n-1; i+1 > 1; i--) {
		// `I * T[i-1] * X[st-i] ~= 1` (mod M)
		monty_mul(R[i], I, T[i-1], Mo);
		monty_mul(I, I, X[st*i], Mo);
	}

	u256_copy(R[0], I);
}

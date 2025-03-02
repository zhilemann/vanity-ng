#include "core.h"

typedef struct {
	u32 X[8], A[8], B[8];
	u32 extA, extB;
} bn_gcd;

u32 u32_rol(u32 x, u32 n) {
	return (x << n) | (x >> (32-n));
}

u32 u32_ror(u32 x, u32 n) {
	return (x >> n) | (x << (32-n));
}

u64 u64_rol(u64 x, u32 n) {
	return (x << n) | (x >> (64-n));
}

u64 u64_ror(u64 x, u32 n) {
	return (x >> n) | (x << (64-n));
}

u32 u32_bswap(u32 x) {
	const u32 A = 0xff00ff00;
	x = (x << 8) & A | (x >> 8) & ~A;
    return (x << 16) | (x >> 16);
}

u64 u64_bswap(u64 x) {
	const u64 A = 0xff00ff00ff00ff00;
	const u64 B = 0xffff0000ffff0000;

	x = ((x << 8) & A) | ((x >> 8) & ~A);
	x = ((x << 16) & B) | ((x >> 16) & ~B);

    return (x << 32) | (x >> 32);
}

void bn_zero(bn R) {
	for (u32 i = 0; i < 8; i++)
		R[i] = 0;
}

void bn_copy(bn R, const bn X) {
	for (u32 i = 0; i < 8; i++)
		R[i] = X[i];
}

void bn_bswap(bn R, const bn X) {
	for (u32 i = 0; i < 8; i++)
		R[i] = u32_bswap(X[i]);

	for (u32 i = 0; i < 4; i++) {
		u32 t = R[i];
		R[i] = R[7-i], R[7-i] = t;
	}
}

// `(retval ...R) = -(extX ...X)`
static u32 bn_neg(bn R, const bn X, u32 extX) {
	u64 xx = 1;
	for (u32 i = 0; i < 8; i++) {
		xx += ~X[i];
		R[i] = xx; xx >>= 32;
	}

	return xx;
}

static u32 bn_width(const bn X) {
	u32 n = 7;
	while (X[n] == 0) { n--; }
	return n;
}

static u64 bn_get64(const bn X, u32 i) {
	u64 x = X[i];
	if (i < 7) { x |= (u64)X[i+1] << 32; }
	return x;
}

u32 bn_is_zero(const bn X) {
	u32 x = 0;
	for (u32 i = 0; i < 8; i++) { x |= X[i]; }
	return x == 0;
}

ord bn_cmp(const bn X, const bn Y) {
	u32 lt = 0, gt = 0;
	for (u32 i = 0; i < 8; i++) {
		if (X[i] < Y[i]) { lt |= (1 << i); }
		if (X[i] > Y[i]) { gt |= (1 << i); }
	}

	if (lt > gt)
		return LT;
	else if (lt < gt)
		return GT;
	else
		return EQ;
}


static void bn_shr1(bn R, const bn X, u32 ext) {
	for (u32 i = 0; i < 7; i++)
		R[i] = (X[i] >> 1) | (X[i+1] << 31);

	R[7] = (X[7] >> 1) | (ext << 31);
}

// `R = X >> (32*n)`
static void bn_shl32n(u32 R[8], const u32 X[8], u32 n) {
	for (u32 i = 7; i+1 > n; i--) { R[i] = X[i-n]; }
	for (u32 i = 0;   i < n; i++) { R[i] = 0; }
}

u32 bn_add32(bn R, u32 x) {
	u64 xx = x;
	for (u32 i = 0; i < 8; i++) {
		xx += R[i];
		R[i] = xx; xx >>= 32;
	}

	return xx;
}

u32 bn_add(bn R, const bn X, const bn Y) {
	u64 xx = 0;
	for (u32 i = 0; i < 8; i++) {
		xx += X[i] + (u64)Y[i];
		R[i] = xx; xx >>= 32;
	}

	return xx;
}

u32 bn_sub(bn R, const bn X, const bn Y) {
	i64 xx = 0;
	for (u32 i = 0; i < 8; i++) {
		xx += X[i] - (i64)Y[i];
		R[i] = xx; xx >>= 32;
	}

	return xx;
}

void bn_modadd(bn R, const bn X, const bn Y, const bn M) {
	u32 ext = bn_add(R, X, Y);
	if (ext || bn_cmp(R, M) > LT)
		bn_sub(R, R, M);
}

void bn_modsub(bn R, const bn X, const bn Y, const bn M) {
	u32 ext = bn_sub(R, X, Y);
	if (ext) bn_add(R, R, M);
}

u32 bn_muladd(bn R, u32 a, const bn X) {
	u64 xx = 0;
	for (u32 i = 0; i < 8; i++) {
		xx += R[i] + a * (u64)X[i];
		R[i] = xx; xx >>= 32;
	}

	return xx;
}

static u32 bn_mulsub(bn R, u32 a, const bn X) {
	i64 xx = 0, yy = 0;
	for (u32 i = 0; i < 8; i++) {
		xx = xx + (u64)R[i];
		yy = xx - a * (u64)X[i];

		R[i] = yy;

		u64 bf = yy > xx ? -1 : 0;
		xx = (yy >> 32) | (bf << 32);
	}

	return xx;
}

void bn_mulw(u32 R[16], const bn X, const bn Y) {
	bn_zero(R); bn_zero(R+8);
	for (u32 i = 0; i < 8; i++)
		R[i+8] += bn_muladd(R+i, X[i], Y);
}

/*
void u256_pow32(u32 R[8], const u32 X[8], u32 k) {
	u32 X1[16], T[16] = {1};
	bn_copy(X1, X);

	while (k > 0) {
		if (k & 1) { u256_mul(T, T, X1); }
		u256_mul(X1, X1, X1); k >>= 1;
	}

	bn_copy(R, T);
}
*/

void bn_divmod(bn Rq, bn Rr, const bn X, const bn Y) {
	bn_zero(Rq); bn_copy(Rr, X);
	if (bn_cmp(X, Y) == LT) return;

	bn Y1;
	u32 n = bn_width(X);
	u32 m = bn_width(Y);

	for (u32 i = n; i+1 > m; i--) {
		bn_shl32n(Y1, Y, i - m);

		while (bn_cmp(Rr, Y1) > LT) {
			// guess the quotient digit
			// see Handbook of Applied Cryptography, 14.20
			u32 k = bn_get64(Rr, i) / (1 + (u64)Y[m]);

			if (k > 0) {
				Rq[i-m] += k;
				bn_mulsub(Rr, k, Y1);
			} else {
				Rq[i-m] += 1;
				bn_sub(Rr, Rr, Y1);
			}
		}
	}
}

static void bn_gcd_step(bn_gcd* A, const bn X, const bn Y) {
	while (!(A->X[0] & 1)) {
		bn_shr1(A->X, A->X, 0);

		if ((A->A[0] | A->B[0]) & 1) {
			A->extA += bn_add(A->A, A->A, Y);
			A->extB += bn_sub(A->B, A->B, X);
		}

		bn_shr1(A->A, A->A, A->extA);
		A->extA = (int)A->extA >> 1;

		bn_shr1(A->B, A->B, A->extB);
		A->extB = (int)A->extB >> 1;
	}
}

static void bn_gcd_swap(bn_gcd* A, bn_gcd* B) {
	if (bn_cmp(A->X, B->X) == LT) {
		bn_gcd* tmp = A;
		A = B; B = tmp;
	}

	bn_sub(A->X, A->X, B->X);

	A->extA -= B->extA;
	A->extA += bn_sub(A->A, A->A, B->A);

	A->extB -= B->extB;
	A->extB += bn_sub(A->B, A->B, B->B);
}

void bn_modinv(bn R, const bn X, const bn M) {
	// see Handbook of Applied Cryptography, 14.61
	bn_gcd P = { {}, { 1 }, {} };
	bn_gcd Q = { {}, {}, { 1 } };

	bn_copy(P.X, X);
	bn_copy(Q.X, M);

	while (!bn_is_zero(P.X)) {
		bn_gcd_step(&P, X, M);
		bn_gcd_step(&Q, X, M);
		bn_gcd_swap(&P, &Q);
	}

	u32 T[8]; bn_copy(T, Q.A);
	u32 ext = Q.extA;

	u32 neg = (int)ext < 0;
	if (neg) ext = bn_neg(T, T, ext);

	while (ext > 0 || bn_cmp(T, M) > LT)
		ext += bn_sub(T, T, M);

	if (neg) { bn_sub(T, M, T); }

	bn_copy(R, T);
}

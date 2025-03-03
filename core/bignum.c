#include "core.h"

u32 u32_bswap(u32 x) {
	const u32 A = 0xff00ff00;
	x = (x<<8) & A | (x>>8) & ~A;
	return x<<16 | x>>16;
}

u64 u64_bswap(u64 x) {
	const u64 A = 0xff00ff00ff00ff00;
	const u64 B = 0xffff0000ffff0000;

	x = (x<<8) & A | (x>>8) & ~A;
	x = (x<<16) & B | (x>>16) & ~B;

	return (x<<32) | (x>>32);
}

void bn_zero(bn R) {
	for (u32 i = 0; i < 8; i++) { R[i] = 0; }
}

void bn_copy(bn R, const bn X) {
	for (u32 i = 0; i < 8; i++)
		R[i] = X[i];
}

static void bn_neg(bn R) {
	for (u32 i = 0; i < 8; i++)
		R[i] = ~R[i];
}

void bn_bswap(bn R, const bn X) {
	for (u32 i = 0; i < 8; i++)
		R[i] = u32_bswap(X[i]);

	for (u32 i = 0; i < 4; i++) {
		u32 t = R[i];
		R[i] = R[7-i], R[7-i] = t;
	}
}

static u32 bn_width(const bn X) {
	u32 n = 7;
	while (X[n] == 0) n--;
	return n;
}

static u64 bn_get64(const bn X, u32 i) {
	u64 t = X[i];
	if (i < 7) { t |= (u64)X[i+1] << 32; }
	return t;
}

u32 bn_is_zero(const bn X) {
	u32 t = 0;
	for (u32 i = 0; i < 8; i++) { t |= X[i]; }
	return t == 0;
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

static void bn_shl32n(u32 R[8], const u32 X[8], u32 n) {
	for (u32 i = 7; i+1 > n; i--) { R[i] = X[i-n]; }
	for (u32 i = 0; i < n; i++) { R[i] = 0; }
}

u32 bn_add32(bn R, const bn X, u32 y) {
	u64 t = y;
	for (u32 i = 0; i < 8; i++) {
		t += X[i];
		R[i] = t; t >>= 32;
	}

	return t;
}

u32 bn_add(bn R, const bn X, const bn Y) {
	u64 t = 0;
	for (u32 i = 0; i < 8; i++) {
		t += X[i] + (u64)Y[i];
		R[i] = t; t >>= 32;
	}

	return t;
}

u32 bn_sub(bn R, const bn X, const bn Y) {
	i64 t = 0;
	for (u32 i = 0; i < 8; i++) {
		t += X[i] - (i64)Y[i];
		R[i] = t; t >>= 32;
	}

	return t;
}

void bn_modadd(bn R, const bn X, const bn Y, const bn M) {
	if (bn_add(R, X, Y) || bn_cmp(R, M) > LT)
		bn_sub(R, R, M);
}

void bn_modsub(bn R, const bn X, const bn Y, const bn M) {
	if (bn_sub(R, X, Y)) { bn_add(R, R, M); }
}

u32 bn_muladd(bn R, const bn X, u32 a, const bn Y) {
	u64 xx = 0;
	for (u32 i = 0; i < 8; i++) {
		xx += X[i] + a * (u64)Y[i];
		R[i] = xx; xx >>= 32;
	}

	return xx;
}

static u32 bn_mulsub(bn R, const bn X, u32 a, const bn Y) {
	i64 xx = 0, yy = 0;
	for (u32 i = 0; i < 8; i++) {
		xx = xx + (u64)X[i];
		yy = xx - a * (u64)Y[i];

		R[i] = yy;

		u64 bf = yy > xx ? -1 : 0;
		xx = (yy >> 32) | (bf << 32);
	}

	return xx;
}

void bn_mul512(bn_2 R, const bn X, const bn Y) {
	bn_zero(R); bn_zero(R+8);
	for (u32 i = 0; i < 8; i++)
		R[i+8] += bn_muladd(R+i, R+i, X[i], Y);
}

void bn_divmod(bn Q, bn R, const bn X, const bn Y) {
	bn_zero(Q); bn_copy(R, X);
	if (bn_cmp(X, Y) == LT) return;

	bn Y1;
	u32 n = bn_width(X);
	u32 m = bn_width(Y);

	for (u32 i = n; i+1 > m; i--) {
		bn_shl32n(Y1, Y, i - m);
		while (bn_cmp(R, Y1) > LT) {
			// guess the quotient digit
			// see Handbook of Applied Cryptography, 14.20
			u64 k = bn_get64(R, i) / (1 + (u64)Y[m]);

			if (k > 0) {
				Q[i-m] += k;
				bn_mulsub(R, R, k, Y1);
			} else {
				Q[i-m] += 1;
				bn_sub(R, R, Y1);
			}
		}
	}
}

static void bn_modinv_step(bn_1 A, bn X, const bn M) {
	while (!(X[0] & 1)) {
		bn_shr1(X, X, 0);
		if (A[0] & 1) { A[8] += bn_add(A, A, M); }

		bn_shr1(A, A, A[8]);
		A[8] = (int)A[8] >> 1;
	}
}

void bn_modinv(bn R, const bn X, const bn M) {
	// see Handbook of Applied Cryptography, 14.61
	bn X_, Y; bn_copy(X_, X); bn_copy(Y, M);
	bn_1 A = { 1 }, B = {};

	while (!bn_is_zero(X_)) {
		bn_modinv_step(A, X_, M);
		bn_modinv_step(B, Y, M);

		if (bn_cmp(X_, Y) > LT) {
			bn_sub(X_, X_, Y);
			A[8] += bn_sub(A, A, B) - B[8];
		} else {
			bn_sub(Y, Y, X_);
			B[8] += bn_sub(B, B, A) - A[8];
		}
	}

	u32 neg = (int)B[8] < 0;
	if (neg) {
		bn_neg(B); B[8] = ~B[8];
		B[8] += bn_add32(B, B, 1);
	};

	while (B[8] > 0 || bn_cmp(B, M) > LT)
		B[8] += bn_sub(B, B, M);

	if (neg) bn_sub(B, M, B);
	bn_copy(R, B);
}

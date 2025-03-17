#include "core.h"

typedef struct PACKED { u32 a; u8 b; } base32_40;

u32 u32_bswap(u32 x) {
	// `x = 0xaabbccdd`
	u32 A = 0xff00ff00;
	// `x = 0xbbaaddcc`
	x = (x<<8 & A) | (x>>8 & ~A);
	// `x = 0xddccbbaa`
	return x<<16 | x>>16;
}

u64 u64_bswap(u64 x) {
	// `x = 0xaabbccddeeffgghh`
	u64 A = 0xff00ff00ff00ff00;
	u64 B = 0xffff0000ffff0000;

	// `x = 0xbbaaddccffeehhgg`
	x = (x<<8 & A) | (x>>8 & ~A);
	// `x = 0xddccbbaahhggffee`
	x = (x<<16 & B) | (x>>16 & ~B);

	// `x = 0xhhggffeeddccbbaa`
	return x<<32 | x>>32;
}

void u8_base32(u8* R, const u8* X, u32 n) {
	for (u32 i = n/5 - 1; i+1 > 0; i--) {
		u8* r = R + 8*i;

		base32_40 x = *(base32_40*)(X + 5*i);
		x.a = u32_bswap(x.a);

		r[0] = x.a >> 27, r[1] = x.a >> 22;
		r[2] = x.a >> 17, r[3] = x.a >> 12;

		r[4] = x.a >> 7, r[5] = x.a >> 2;
		r[6] = x.a<<3 | x.b>>5, r[7] = x.b;
	}
}

#if defined(__OPENCL_C_VERSION__)
	#define u32_mul64(x, y) ((u64)mul_hi(x, y)<<32 | x*y)
#else
	#define u32_mul64(x, y) (x * (u64)y)
#endif

static void bn_neg(bn_mut* R) {
	for (u32 i = 0; i < 8; i++)
		R->d[i] = ~R->d[i];
}

void bn_bswap(bn_mut* R, bn X) {
	for (u32 i = 0; i < 8; i++)
		R->d[i] = u32_bswap(X->d[i]);

	for (u32 i = 0; i < 4; i++)
		SWAP(R->d[i], R->d[7-i]);
}

static u32 bn_width(bn X) {
	u32 n = 7;
	while (X->d[n] == 0) n--;
	return n;
}

static u64 bn_get64(bn X, u32 i) {
	u32 l = X->d[i], h = 0;
	if (i < 7) h = X->d[i+1];
	return (u64)h << 32 | l;
}

ord bn_cmp(bn X, bn Y) {
	u32 lt = 0, gt = 0;
	for (u32 i = 0; i < 8; i++) {
		u32 x = X->d[i], y = Y->d[i];
		if (x < y) lt |= 1 << i;
		if (x > y) gt |= 1 << i;
	}

	if (lt > gt) return LT;
	if (lt < gt) return GT;
	return EQ;
}

/////////////////////////////////////////////////

static void bn_shr1(bn_mut* R, bn X, u32 ext) {
	for (u32 i = 0; i < 7; i++) {
		R->d[i] = X->d[i] >> 1;
		R->d[i] |= X->d[i+1] << 31;
	}

	R->d[7] = X->d[7] >> 1 | ext << 31;
}

void bn_shrN(bn_mut* R, bn X, u32 n) {
	for (u32 i = 0; i < 7; i++) {
		R->d[i] = X->d[i] >> n;
		R->d[i] |= X->d[i+1] << (32-n);
	}

	R->d[7] = X->d[7] >> n;
}

static void bn_shl32N(bn_mut* R, bn X, u32 n) {
	for (u32 i = 7; i+1 > n; i--)
		R->d[i] = X->d[i-n];

	for (u32 i = 0; i < n; i++)
		R->d[i] = 0;
}

u32 bn_add64(bn_mut* R, bn X, u64 y) {
	u64 t = y;
	for (u32 i = 0; i < 8; i++) {
		t += X->d[i];
		R->d[i] = t, t >>= 32;
	}

	return t;
}

u32 bn_add(bn_mut* R, bn X, bn Y) {
	u64 t = 0;
	for (u32 i = 0; i < 8; i++) {
		t += X->d[i] + (u64)Y->d[i];
		R->d[i] = t, t >>= 32;
	}

	return t;
}

u32 bn_sub(bn_mut* R, bn X, bn Y) {
	i64 t = 0;
	for (u32 i = 0; i < 8; i++) {
		t += X->d[i] - (i64)Y->d[i];
		R->d[i] = t, t >>= 32;
	}

	return t;
}

void bn_modadd(bn_mut* R, bn X, bn Y, bn M) {
	if (bn_add(R, X, Y) || bn_cmp(R, M) > LT)
		bn_sub(R, R, M);
}

void bn_modsub(bn_mut* R, bn X, bn Y, bn M) {
	if (bn_sub(R, X, Y)) { bn_add(R, R, M); }
}

/////////////////////////////////////////////////

u32 bn_muladd(bn_mut* R, bn X, u32 a, bn Y) {
	u64 xx = 0;
	for (u32 i = 0; i < 8; i++) {
		xx += X->d[i] + u32_mul64(a, Y->d[i]);
		R->d[i] = xx, xx >>= 32;
	}

	return xx;
}

static u32 bn_mulsub(bn_mut* R, bn X, u32 a, bn Y) {
	i64 xx = 0, yy = 0;
	for (u32 i = 0; i < 8; i++) {
		xx = xx + (u64)X->d[i];
		yy = xx - u32_mul64(a, Y->d[i]);

		R->d[i] = yy;

		u64 bf = yy > xx ? -1 : 0;
		xx = (yy >> 32) | (bf << 32);
	}

	return xx;
}

void bn_mul512(bn2_mut* R, bn X, bn Y) {
	R->l = R->h = BN_0;
	for (u32 i = 0; i < 8; i++) {
		bn1_mut* r = (void*)&R->d[i];
		r->h += bn_muladd(&r->l, &r->l, X->d[i], Y);
	}
}

void bn_divmod(bn_mut* Qu, bn_mut* Re, bn X, bn Y) {
	// see Handbook of Applied Cryptography, 14.20
	*Qu = BN_0, *Re = *X;
	if (bn_cmp(X, Y) == LT) return;

	bn_mut Y1;
	u32 n = bn_width(X);
	u32 m = bn_width(Y);

	for (u32 i = n; i+1 > m; i--) {
		bn_shl32N(&Y1, Y, i-m);
		while (bn_cmp(Re, &Y1) > LT) {
			// lower bound for quotient digit
			u64 k = bn_get64(Re, i) / (1 + (u64)Y->d[m]);

			if (k > 0) {
				Qu->d[i-m] += k;
				bn_mulsub(Re, Re, k, &Y1);
			} else {
				Qu->d[i-m] += 1;
				bn_sub(Re, Re, &Y1);
			}
		}
	}
}

/////////////////////////////////////////////////

static void bn_modinv_step(bn1_mut* A, bn_mut* X, bn M) {
	while (!(X->d[0] & 1)) {
		bn_shr1(X, X, 0);
		if (A->d[0] & 1)
			A->h += bn_add(&A->l, &A->l, M);

		bn_shr1(&A->l, &A->l, A->h);
		A->h = (int)A->h >> 1;
	}
}

void bn_modinv(bn_mut* R, bn X, bn M) {
	// see Handbook of Applied Cryptography, 14.61
	bn_mut X_ = *X, Y = *M;
	bn1_mut A = { 1 }, B = {};

	while (bn_cmp(&X_, &BN_0) != EQ) {
		bn_modinv_step(&A, &X_, M);
		bn_modinv_step(&B, &Y, M);

		if (bn_cmp(&X_, &Y) > LT) {
			bn_sub(&X_, &X_, &Y);
			A.h += bn_sub(&A.l, &A.l, &B.l) - B.h;
		} else {
			bn_sub(&Y, &Y, &X_);
			B.h += bn_sub(&B.l, &B.l, &A.l) - A.h;
		}
	}

	u32 neg = (int)B.h < 0;
	if (neg) {
		bn_neg(&B.l), B.h = ~B.h;
		B.h += bn_add64(&B.l, &B.l, 1);
	};

	while (B.h > 0 || bn_cmp(&B.l, M) > LT)
		B.h += bn_sub(&B.l, &B.l, M);

	if (neg) bn_sub(&B.l, M, &B.l);
	*R = B.l;
}

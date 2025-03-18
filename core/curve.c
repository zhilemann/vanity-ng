#include "core.h"

static const global bn_mut SECP_M = {
	0xfffffc2f, 0xfffffffe, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
};

static const global bn_mut ED_M = {
	0xffffffed, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff,
};

static const global bn_mut ED_2D = {
	0x26b2f159, 0xebd69b94, 0x8283b156, 0x00e0149a,
	0xeef3d130, 0x198e80f2, 0x56dffce7, 0x2406d9dc,
};

static void secp_modmul(bn_mut* R, bn X, bn Y) {
	// `2^256 ~= 2^32 + 977`
	bn2_mut T, U = {};
	bn_mul512(&T, X, Y), U.l = T.l;

	// step I: `U = T.l + T.h<<32 + 977*T.h`
	bn_add(&U.l_32, &U.l_32, &T.h);
	u64 ext = (u64)U.d[8];
	ext += bn_muladd(&U.l, &U.l, 977, &T.h);

	// step II: `T = U.l + ext<<32 + 977*ext`
	T.l = U.l, T.h = BN_0;
	bn_add64(&T.l_32, &T.l_32, ext);
	bn_add64(&T.l, &T.l, 977 * ext);

	if (bn_cmp(&T.l, &SECP_M) > LT)
		bn_sub(&T.l, &T.l, &SECP_M);

	*R = T.l;
}

static void ed_modmul(bn_mut* R, bn X, bn Y) {
	// `2^256 ~= 38`
	bn2_mut T; bn_mul512(&T, X, Y);

	// step I: `T = T.l + 38 * T.h`
	u32 ext = bn_muladd(&T.l, &T.l, 38, &T.h);
	// step II: `T = T.l + 38 * ext`
	bn_add64(&T.l, &T.l, 38 * ext);

	if (bn_cmp(&T.l, &ED_M) > LT)
		bn_sub(&T.l, &T.l, &ED_M);

	*R = T.l;
}

/////////////////////////////////////////////////

static void secp_addX(xy* R, const xy* P, const xy* Q) {
	if (bn_cmp(&P->x, &Q->x) != EQ)
		bn_modsub(&R->x, &P->y, &Q->y, &SECP_M);
	else {
		bn_mut T; secp_modmul(&T, &P->x, &Q->x);

		bn_modadd(&R->x, &T, &T, &SECP_M);
		bn_modadd(&R->x, &R->x, &T, &SECP_M);
	}
}

static void secp_addY(xy* R, const xy* P, const xy* Q) {
	if (bn_cmp(&P->x, &Q->x) != EQ)
		bn_modsub(&R->x, &P->x, &Q->x, &SECP_M);
	else
		bn_modadd(&R->x, &P->y, &Q->y, &SECP_M);
}

static void secp_add_0(xy* R, const xy* P, const xy* Q) {
	secp_modmul(&R->y, &R->x, &R->y);
	secp_modmul(&R->x, &R->y, &R->y);

	bn_modsub(&R->x, &R->x, &P->x, &SECP_M);
	bn_modsub(&R->x, &R->x, &Q->x, &SECP_M);

	bn_mut T; bn_modsub(&T, &P->x, &R->x, &SECP_M);
	secp_modmul(&R->y, &R->y, &T);
	bn_modsub(&R->y, &R->y, &P->y, &SECP_M);
}

u32 secp_addN(xy* R, const xy* P, const xy* Q, u32 n) {
	for (u32 i = 0; i < n; i++)
		secp_addY(&R[i], P, &Q[i]);

	R[0].y = R[0].x;
	for (u32 i = 1; i < n; i++)
		// `R[i].y ~= R[0].x * ... * R[i].x`
		secp_modmul(&R[i].y, &R[i-1].y, &R[i].x);

	if (bn_cmp(&R[n-1].y, &BN_0) == EQ)
		return 0;

	bn_mut I; bn_modinv(&I, &R[n-1].y, &SECP_M);
	for (u32 i = n-1; i+1 > 1; i--) {
		// `I * R[i-1].y * R[i].x ~= 1`
		secp_modmul(&R[i].y, &I, &R[i-1].y);
		secp_modmul(&I, &I, &R[i].x);
	}

	R[0].y = I;
	for (u32 i = 0; i < n; i++) {
		secp_addX(&R[i], P, &Q[i]);
		secp_add_0(&R[i], P, &Q[i]);
	}

	return 1;
}

void secp_mul(xy* R, bn X, const secp_lut_mul* L) {
	bn_mut X_ = *X; u32 i = 0; xy T;

	while (!(X_.d[0] & 1))
		bn_shrN(&X_, &X_, 1), i++;

	bn_shrN(&X_, &X_, 1), *R = L->d[i];
	while (i++ < 256) {
		if (X_.d[0] & 1)
			secp_addN(&T, R, &L->d[i], 1), *R = T;

		bn_shrN(&X_, &X_, 1);
	}
}

/////////////////////////////////////////////////

static void ed_xytz_scale(xytz* R, bn a, const xytz* P) {
	ed_modmul(&R->x, &P->x, a);
	ed_modmul(&R->y, &P->y, a);
}

static void ed_xy2d_init(xy2d* R, const xytz* P) {
	bn_modsub(&R->a, &P->y, &P->x, &ED_M);
	bn_modadd(&R->b, &P->y, &P->x, &ED_M);

	ed_modmul(&R->c, &P->x, &P->y);
	ed_modmul(&R->c, &R->c, &ED_2D);
}

void ed_normN(xytz* R, const xytz* P, u32 n) {
	R[0].t = P[0].z;
	for (u32 i = 1; i < n; i++)
		// `R[i].t ~= P[0].z * ... * P[i].z`
		ed_modmul(&R[i].t, &R[i-1].t, &P[i].z);

	bn_mut I, Zi;
	bn_modinv(&I, &R[n-1].t, &ED_M);

	for (u32 i = n-1; i+1 > 1; i--) {
		// `I * R[i-1].t * P[i].z ~= 1`
		ed_modmul(&Zi, &I, &R[i-1].t);
		ed_modmul(&I, &I, &P[i].z);
		ed_xytz_scale(R+i, &Zi, P+i);
	}

	ed_xytz_scale(R, &I, P);
}

/////////////////////////////////////////////////

static void ed_add(
	xytz* R,
	const bn A, const bn B,
	const bn C, const bn D
) {
	bn_mut E, F, G, H;

	// `E = B - A`, `F = D - C`
	// `G = D + C`, `H = B + A`
	bn_modsub(&E, B, A, &ED_M);
	bn_modsub(&F, D, C, &ED_M);
	bn_modadd(&G, D, C, &ED_M);
	bn_modadd(&H, B, A, &ED_M);

	// `R->X = E * F`, `R->Y = G * H`
	// `R->T = E * H`, `R->Z = F * G`
	ed_modmul(&R->x, &E, &F);
	ed_modmul(&R->y, &G, &H);
	ed_modmul(&R->t, &E, &H);
	ed_modmul(&R->z, &F, &G);
}

static void ed_add_xytz(
	xytz* R,
	const xytz* P, const xytz* Q
) {
	bn_mut A, B, C, D;

	// `A = (P->Y - P->X) * (Q->Y - Q->X)`
	// using B as `Q->Y - Q->X`
	bn_modsub(&A, &P->y, &P->x, &ED_M);
	bn_modsub(&B, &Q->y, &Q->x, &ED_M);
	ed_modmul(&A, &A, &B);

	// `B = (P->Y + P->X) * (Q->Y + Q->X)`
	// using C as `Q->Y + Q->X`
	bn_modadd(&B, &P->y, &P->x, &ED_M);
	bn_modadd(&C, &Q->y, &Q->x, &ED_M);
	ed_modmul(&B, &B, &C);

	// C = Ed->_2D * P->T * Q->T
	ed_modmul(&C, &P->t, &Q->t);
	ed_modmul(&C, &C, &ED_2D);

	// D = 2 * P->Z * Q->Z
	ed_modmul(&D, &P->z, &Q->z);
	bn_modadd(&D, &D, &D, &ED_M);

	ed_add(R, &A, &B, &C, &D);
}

static void ed_add_xy2d(
	xytz* R,
	const xytz* P, const xy2d* Q
) {
	bn_mut A, B, C, D;

	// `A = (P->Y - P->X) * (Q->Y - Q->X)`
	bn_modsub(&A, &P->y, &P->x, &ED_M);
	ed_modmul(&A, &A, &Q->a);

	// `B = (P->Y + P->X) * (Q->Y + Q->X)`
	bn_modadd(&B, &P->y, &P->x, &ED_M);
	ed_modmul(&B, &B, &Q->b);

	// C = Ed->_2D * P->T * Q->T
	// D = 2 * P->Z * Q->Z
	ed_modmul(&C, &P->t, &Q->c);
	bn_modadd(&D, &P->z, &P->z, &ED_M);

	ed_add(R, &A, &B, &C, &D);
}

/////////////////////////////////////////////////

void ed_lut_step(xy2d* R, xytz* G, u32 n) {
	const u32 N = 1024;
	xytz P = ED_ID, T[N];

	for (u32 i = 0; i < n; i += N) {
		T[0] = P;
		for (u32 i = 1; i < N; i++)
			// `T[i] = P + i * G`
			ed_add_xytz(T+i, T+i-1, G);

		// `P = P + N * G`
		ed_add_xytz(&P, T+N-1, G);
		ed_normN(T, T, N);

		for (u32 j = 0; j < N; j++)
			ed_xy2d_init(R+i+j, T+j);
	}

	*G = P;
}

void ed_mul(xytz* R, bn X, const ed_lut* L) {
	bn_mut X_ = *X; *R = ED_ID;

	for (u32 i = 0; i < 8; i++) {
		u32 j = X_.d[0] % (1<<21);
		ed_add_xy2d(R, R, &L->a[i][j]);
		bn_shrN(&X_, &X_, 21);
	}

	for (u32 i = 0; i < 4; i++) {
		u32 j = X_.d[0] % (1<<22);
		ed_add_xy2d(R, R, &L->b[i][j]);
		bn_shrN(&X_, &X_, 22);
	}
}

/////////////////////////////////////////////////

void secp_pubkey33(u8* R, const xy* P) {
	R[0] = 2 + P->y.d[0] & 1;
	bn_bswap(BN(R+1), &P->x);
}

void secp_pubkey64(u8* R, const xy* P) {
	bn_bswap(BN(R), &P->x);
	bn_bswap(BN(R+32), &P->y);
}

void ed_privkey(bn_mut* R, const u8* K) {
	u8 B[128] = {};
	for (u32 i = 0; i < 32; i++)
		B[i] = K[i];

	u8 H[64]; sha2_512(H, B, 32);
	H[0] &= 0xf8, H[31] &= 0x7f, H[31] |= 0x40;
	*R = *(bn_mut*)&H;
}

void ed_pubkey(u8* R, const xytz* P) {
	bn_bswap(BN(R), &P->y);
	R[0] |= P->x.d[0] & 1 << 7;
}

#include "core.h"

const global xyzt ED_ID = { .y = { 1 }, .z = { 1 } };

const global xyzt ED_G = {
{
	0x8f25d51a, 0xc9562d60, 0x9525a7b2, 0x692cc760,
	0xfdd6dc5c, 0xc0a4e231, 0xcd6e53fe, 0x216936d3,
},
{
	0x66666658, 0x66666666, 0x66666666, 0x66666666,
	0x66666666, 0x66666666, 0x66666666, 0x66666666,
},
{
	0xa5b7dda3, 0x6dde8ab3, 0x775152f5, 0x20f09f80,
	0x64abe37d, 0x66ea4e8e, 0xd78b7665, 0x67875f0f,
},
	{ 1 }
};

const global bn_mut ED_M = {
	0xffffffed, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff,
};

const global bn_mut ED_2D = {
	0x26b2f159, 0xebd69b94, 0x8283b156, 0x00e0149a,
	0xeef3d130, 0x198e80f2, 0x56dffce7, 0x2406d9dc,
};

/////////////////////////////////////////////////

static void ed_shrN(bn_mut* R, bn X, u32 n) {
	for (u32 i = 0; i < 7; i++) {
		R->d[i] = X->d[i] >> n;
		R->d[i] |= X->d[i+1] << (32-n);
	}

	R->d[7] = X->d[7] >> n;
}

static void ed_modmul(bn_mut* R, bn X, bn Y) {
	bn2_mut T; bn_mul512(&T, X, Y);

	// `2^256 ~= 38`
	u32 ext = bn_muladd(&T.l, &T.l, 38, &T.h);
	bn_add32(&T.l, &T.l, 38 * ext);

	if (bn_cmp(&T.l, &ED_M) > LT)
		bn_sub(&T.l, &T.l, &ED_M);

	*R = T.l;
}

static void ed_xyzt_scale(xyzt* R, bn a, const xyzt* P) {
	ed_modmul(&R->x, &P->x, a);
	ed_modmul(&R->y, &P->y, a);
}

static void ed_xy2d_init(xy2d* R, const xyzt* P) {
	bn_modsub(&R->a, &P->y, &P->x, &ED_M);
	bn_modadd(&R->b, &P->y, &P->x, &ED_M);

	ed_modmul(&R->c, &P->x, &P->y);
	ed_modmul(&R->c, &R->c, &ED_2D);
}

void ed_normN(xyzt* R, const xyzt* P, u32 n) {
	R[0].t = P[0].z;
	for (u32 i = 1; i < n; i++)
		// `R[i].T ~= P[0].Z * ... * P[i].Z`
		ed_modmul(&R[i].t, &R[i-1].t, &P[i].z);

	bn_mut I, Zi;
	bn_modinv(&I, &R[n-1].t, &ED_M);

	for (u32 i = n-1; i+1 > 1; i--) {
		// `I * R[i-1].T * P[i].Z ~= 1`
		ed_modmul(&Zi, &I, &R[i-1].t);
		ed_xyzt_scale(R+i, &Zi, P+i);

		ed_modmul(&I, &I, &P[i].z);
	}

	ed_xyzt_scale(R, &I, P);
}

/////////////////////////////////////////////////

static void ed_add(
	xyzt* R,
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

static void ed_add_xyzt(
	xyzt* R,
	const xyzt* P, const xyzt* Q
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
	xyzt* R,
	const xyzt* P, const xy2d* Q
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

void ed_lut_step(xy2d* R, xyzt* G, u32 w) {
	const u32 N = 1024;
	xyzt P = ED_ID, T[N];

	for (u32 i = 0; i < (1<<w); i += N) {
		T[0] = P;
		for (u32 i = 1; i < N; i++)
			// `T[i] = P + i * G`
			ed_add_xyzt(T+i, T+i-1, G);

		// `P = P + N * G`
		ed_add_xyzt(&P, T+N-1, G);
		ed_normN(T, T, N);

		for (u32 j = 0; j < N; j++)
			ed_xy2d_init(R+i+j, T+j);
	}

	*G = P;
}

void ed_mul(xyzt* R, bn X, const ed_lut* L) {
	bn_mut X_ = *X; *R = ED_ID;

	for (u32 i = 0; i < 8; i++) {
		u32 j = X_.d[0] % (1<<21);
		ed_add_xy2d(R, R, &L->a[i][j]);
		ed_shrN(&X_, &X_, 21);
	}

	for (u32 i = 0; i < 4; i++) {
		u32 j = X_.d[0] % (1<<22);
		ed_add_xy2d(R, R, &L->b[i][j]);
		ed_shrN(&X_, &X_, 22);
	}
}

void ed_privkey(bn_mut* R, const u8* K) {
	u8 B[128] = {};
	for (u32 i = 0; i < 32; i++)
		B[i] = K[i];

	u8 H[64]; sha2_512(H, B, 32);
	H[0] &= 0xf8, H[31] &= 0x7f, H[31] |= 0x40;
	*R = *(bn_mut*)&H;
}

void ed_pubkey(bn_mut* R, const xyzt* P) {
	bn_bswap(R, &P->y);
	R->d[0] |= (P->x.d[0] & 1) << 7;
}

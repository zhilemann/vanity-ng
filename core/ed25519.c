#include "core.h"

const global xyzt ED25519_G = {
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

const global bn_mut ED25519_M = {
	0xffffffed, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff,
};

const global bn_mut ED25519_2D = {
	0x26b2f159, 0xebd69b94, 0x8283b156, 0x00e0149a,
	0xeef3d130, 0x198e80f2, 0x56dffce7, 0x2406d9dc,
};

static void ed_modmul(bn_mut* R, bn X, bn Y) {
	bn2_mut T; bn_mul512(&T, X, Y);

	// `2^256 ~= 38`
	u32 ext = bn_muladd(&T.l, &T.l, 38, &T.h);
	bn_add32(&T.l, &T.l, 38 * ext);

	if (bn_cmp(&T.l, &ED25519_M) > LT)
		bn_sub(&T.l, &T.l, &ED25519_M);

	*R = T.l;
}

static void ed_inv256(bn_mut* R, bn X, u32 st) {
	bn_mut T[256], I; T[0] = X[0];

	for (u32 i = 1; i < 256; i++)
		// `T[i] ~= X[0] * X[1] * ... * X[i]`
		ed_modmul(&T[i], &T[i-1], &X[st*i]);

	bn_modinv(&I, &T[255], &ED25519_M);
	for (u32 i = 255; i+1 > 1; i--) {
		// `I * T[i-1] * X[st*i] ~= 1`
		ed_modmul(&R[i], &I, &T[i-1]);
		ed_modmul(&I, &I, &X[st*i]);
	}

	R[0] = I;
}

static void ed_xyzt_comp(xyzt* R) {
	ed_modmul(&R->T, &R->X, &R->Y);
	bn_zero(&R->Z), R->Z.d[0] = 1;
}

static void ed_xyzt_id(xyzt* R) {
	bn_zero(&R->X);
	bn_zero(&R->Y), R->Y.d[0] = 1;
	ed_xyzt_comp(R);
}

static void ed_xyzt_copy(xyzt* R, const xyzt* P) {
	R->X = P->X, R->Y = P->Y;
	R->T = P->T, R->Z = P->Z;
}

static void ed_xy2d_init(xy2d* R, const xyzt* P) {
	bn_modsub(&R->A, &P->Y, &P->X, &ED25519_M);
	bn_modadd(&R->B, &P->Y, &P->X, &ED25519_M);
	ed_modmul(&R->C, &P->T, &ED25519_2D);
}

void ed_norm256(xyzt* R, const xyzt* P) {
	const bn Zs = &P[0].Z;
	bn_mut Z_inv[256]; ed_inv256(Z_inv, Zs, 4);

	for (u32 i = 0; i < 256; i++) {
		ed_modmul(&R[i].X, &P[i].X, &Z_inv[i]);
		ed_modmul(&R[i].Y, &P[i].Y, &Z_inv[i]);
		ed_xyzt_comp(&R[i]);
	}
}

static void ed_add(
	xyzt* R,
	const bn A, const bn B,
	const bn C, const bn D
) {
	bn_mut E, F, G, H;

	// `E = B - A`, `F = D - C`
	// `G = D + C`, `H = B + A`
	bn_modsub(&E, B, A, &ED25519_M);
	bn_modsub(&F, D, C, &ED25519_M);
	bn_modadd(&G, D, C, &ED25519_M);
	bn_modadd(&H, B, A, &ED25519_M);

	// `R->X = E * F`, `R->Y = G * H`
	// `R->T = E * H`, `R->Z = F * G`
	ed_modmul(&R->X, &E, &F);
	ed_modmul(&R->Y, &G, &H);
	ed_modmul(&R->T, &E, &H);
	ed_modmul(&R->Z, &F, &G);
}

static void ed_add_xyzt(
	xyzt* R,
	const xyzt* P, const xyzt* Q
) {
	bn_mut A, B, C, D;

	// `A = (P->Y - P->X) * (Q->Y - Q->X)`
	// using B as `Q->Y - Q->X`
	bn_modsub(&A, &P->Y, &P->X, &ED25519_M);
	bn_modsub(&B, &Q->Y, &Q->X, &ED25519_M);
	ed_modmul(&A, &A, &B);

	// `B = (P->Y + P->X) * (Q->Y + Q->X)`
	// using C as `Q->Y + Q->X`
	bn_modadd(&B, &P->Y, &P->X, &ED25519_M);
	bn_modadd(&C, &Q->Y, &Q->X, &ED25519_M);
	ed_modmul(&B, &B, &C);

	// C = Ed->_2D * P->T * Q->T
	ed_modmul(&C, &P->T, &Q->T);
	ed_modmul(&C, &C, &ED25519_2D);

	// D = 2 * P->Z * Q->Z
	ed_modmul(&D, &P->Z, &Q->Z);
	bn_modadd(&D, &D, &D, &ED25519_M);

	ed_add(R, &A, &B, &C, &D);
}

static void ed_add_xy2d(
	xyzt* R,
	const xyzt* P, const xy2d* Q
) {
	bn_mut A, B, C, D;

	// `A = (P->Y - P->X) * (Q->Y - Q->X)`
	bn_modsub(&A, &P->Y, &P->X, &ED25519_M);
	ed_modmul(&A, &A, &Q->A);

	// `B = (P->Y + P->X) * (Q->Y + Q->X)`
	bn_modadd(&B, &P->Y, &P->X, &ED25519_M);
	ed_modmul(&B, &B, &Q->B);

	// C = Ed->_2D * P->T * Q->T
	// D = 2 * P->Z * Q->Z
	ed_modmul(&C, &P->T, &Q->C);
	bn_modadd(&D, &P->Z, &P->Z, &ED25519_M);

	ed_add(R, &A, &B, &C, &D);
}

static void ed_comb1(xy2d* R, xyzt* P, const xyzt* G) {
	xyzt T[256];
	for (u32 i = 0; i < 65536; i += 256) {
		ed_xyzt_copy(&T[0], P);
		for (u32 i = 1; i < 256; i++)
			// `T[i] = P + i * Q`
			ed_add_xyzt(&T[i], &T[i-1], G);

		// `P = P + 256 * G`
		ed_add_xyzt(P, &T[255], G);
		ed_norm256(T, T);

		for (u32 j = 0; j < 256; j++)
			ed_xy2d_init(&R[i+j], &T[j]);
	}
}

void ed_comb16(ed_comb* R, const xyzt* G) {
	xyzt P, G_;
	ed_xyzt_copy(&G_, G);

	for (u32 i = 0; i < 16; i++) {
		ed_xyzt_id(&P);
		ed_comb1(R->p[i], &P, &G_);

		// `G = (2^16) * G`
		for (u32 j = 0; j < 16; j++)
			ed_add_xyzt(&G_, &G_, &G_);
	}
}

void ed_privkey(bn_mut* R, const u8* K) {
	u8 B[128] = {};
	for (u32 i = 0; i < 32; i++)
		B[i] = K[i];

	u64 H[8]; sha512(H, B, 32);
	*R = *(bn_mut*)&H;

	U8(R)[0] &= 0xf8;
	U8(R)[31] &= 0x7f;
	U8(R)[31] |= 0x40;
}

void ed_mul(xyzt* R, bn X, const ed_comb* P) {
	ed_xyzt_id(R);
	for (u32 i = 0; i < 8; i++) {
		u32 lo = X->d[i] & 0xffff;
		u32 hi = X->d[i] >> 16;

		ed_add_xy2d(R, R, &P->p[2*i][lo]);
		ed_add_xy2d(R, R, &P->p[2*i+1][hi]);
	}
}

void ed_pubkey(bn_mut* R, const xyzt* P) {
	bn_bswap(R, &P->Y);
	R->d[0] |= (P->X.d[0] & 1) << 7;
}

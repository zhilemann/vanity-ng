#include "core.h"

static const bn _25519 = {
	0xffffffed, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff,
};

static const bn CURVE_2D = {
	0x26b2f159, 0xebd69b94, 0x8283b156, 0x00e0149a,
	0xeef3d130, 0x198e80f2, 0x56dffce7, 0x2406d9dc,
};

static const bn CURVE_GX = {
	0x8f25d51a, 0xc9562d60, 0x9525a7b2, 0x692cc760,
	0xfdd6dc5c, 0xc0a4e231, 0xcd6e53fe, 0x216936d3,
};

static const bn CURVE_GY = {
	0x66666658, 0x66666666, 0x66666666, 0x66666666,
	0x66666666, 0x66666666, 0x66666666, 0x66666666,
};

static void ed_modmul(bn R, const bn X, const bn Y) {
	u32 T[16]; bn_mulw(T, X, Y);

	// `2^256 ~= 38`
	u32 ext = bn_muladd(T, 38, T+8);
	bn_add32(T, 38 * ext);

	if (bn_cmp(T, _25519) > LT)
		bn_sub(T, T, _25519);

	bn_copy(R, T);
}

static void ed_xyzt_comp(xyzt* R) {
	ed_modmul(R->T, R->X, R->Y);
	bn_zero(R->Z); R->Z[0] = 1;
}

static void ed_xyzt_id(xyzt* R) {
	bn_zero(R->X);
	bn_zero(R->Y); R->Y[0] = 1;
	ed_xyzt_comp(R);
}

static void ed_xyzt_copy(xyzt* R, const xyzt* P) {
	bn_copy(R->X, P->X); bn_copy(R->Y, P->Y);
	bn_copy(R->T, P->T); bn_copy(R->Z, P->Z);
}

static void ed_xy2d_init(xy2d* R, const xyzt* P) {
	bn_modsub(R->A, P->Y, P->X, _25519);
	bn_modadd(R->B, P->Y, P->X, _25519);
	ed_modmul(R->C, P->T, CURVE_2D);
}

static void ed_inv256(bn R[256], const bn X[], u32 st) {
	u32 T[256][8], I[8];
	bn_copy(T[0], X[0]);

	for (u32 i = 1; i < 256; i++)
		// `T[i] ~= X[0] * X[1] * ... * X[i]`
		ed_modmul(T[i], T[i-1], X[st*i]);

	bn_modinv(I, T[255], _25519);
	for (u32 i = 255; i+1 > 1; i--) {
		// `I * T[i-1] * X[st*i] ~= 1`
		ed_modmul(R[i], I, T[i-1]);
		ed_modmul(I, I, X[st*i]);
	}

	bn_copy(R[0], I);
}

void ed_norm256(xyzt R[256], const xyzt P[256]) {
	const u32 (*Zs)[8] = (void*)P[0].Z;
	u32 Z_inv[256][8];
	ed_inv256(Z_inv, Zs, 4);

	for (u32 i = 0; i < 256; i++) {
		ed_modmul(R[i].X, P[i].X, Z_inv[i]);
		ed_modmul(R[i].Y, P[i].Y, Z_inv[i]);
		ed_xyzt_comp(&R[i]);
	}
}

/////////////////////////////////////////////////

static void ed_add(
	xyzt* R,
	const u32 A[8], const u32 B[8],
	const u32 C[8], const u32 D[8]
) {
	u32 E[8], F[8], G[8], H[8];

	// `E = B - A`, `F = D - C`
	// `G = D + C`, `H = B + A`
	bn_modsub(E, B, A, _25519);
	bn_modsub(F, D, C, _25519);
	bn_modadd(G, D, C, _25519);
	bn_modadd(H, B, A, _25519);

	// `R->X = E * F`, `R->Y = G * H`
	// `R->T = E * H`, `R->Z = F * G`
	ed_modmul(R->X, E, F);
	ed_modmul(R->Y, G, H);
	ed_modmul(R->T, E, H);
	ed_modmul(R->Z, F, G);
}

static void ed_add_xyzt(
	xyzt* R,
	const xyzt* P, const xyzt* Q
) {
	u32 A[8], B[8], C[8], D[8];

	// `A = (P->Y - P->X) * (Q->Y - Q->X)`
	// using B as `Q->Y - Q->X`
	bn_modsub(A, P->Y, P->X, _25519);
	bn_modsub(B, Q->Y, Q->X, _25519);
	ed_modmul(A, A, B);

	// `B = (P->Y + P->X) * (Q->Y + Q->X)`
	// using C as `Q->Y + Q->X`
	bn_modadd(B, P->Y, P->X, _25519);
	bn_modadd(C, Q->Y, Q->X, _25519);
	ed_modmul(B, B, C);

	// C = Ed->_2D * P->T * Q->T
	ed_modmul(C, P->T, Q->T);
	ed_modmul(C, C, CURVE_2D);

	// D = 2 * P->Z * Q->Z
	ed_modmul(D, P->Z, Q->Z);
	bn_modadd(D, D, D, _25519);

	ed_add(R, A, B, C, D);
}

static void ed_add_xy2d(
	xyzt* R,
	const xyzt* P, const xy2d* Q
) {
	u32 A[8], B[8], C[8], D[8];

	// `A = (P->Y - P->X) * (Q->Y - Q->X)`
	bn_modsub(A, P->Y, P->X, _25519);
	ed_modmul(A, A, Q->A);

	// `B = (P->Y + P->X) * (Q->Y + Q->X)`
	bn_modadd(B, P->Y, P->X, _25519);
	ed_modmul(B, B, Q->B);

	// C = Ed->_2D * P->T * Q->T
	// D = 2 * P->Z * Q->Z
	ed_modmul(C, P->T, Q->C);
	bn_modadd(D, P->Z, P->Z, _25519);

	ed_add(R, A, B, C, D);
}

/////////////////////////////////////////////////

static void ed_precomp1(
	xy2d R[65536],
	xyzt* P, const xyzt* G
) {
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

void ed_comb16_gen(xy2d R[16][65536]) {
	xyzt P, G, T[256];

	bn_copy(G.X, CURVE_GX);
	bn_copy(G.Y, CURVE_GY);
	ed_xyzt_comp(&G);

	for (u32 i = 0; i < 16; i++) {
		ed_xyzt_id(&P);
		ed_precomp1(R[i], &P, &G);

		// `G = (2^16) * G`
		for (u32 j = 0; j < 16; j++)
			ed_add_xyzt(&G, &G, &G);
	}
}

/////////////////////////////////////////////////

void ed_pubkey(u32 R[8], const u8 Pr[32]) {
	u8 B[128] = {};
	for (u32 i = 0; i < 32; i++)
		B[i] = Pr[i];

	u64 H[8]; sha512(H, B, 32);
	for (u32 i = 0; i < 4; i++)
		U64(R)[i] = u64_bswap(H[i]);

	U8(R)[0] &= 0xf8;
	U8(R)[31] &= 0x7f;
	U8(R)[31] |= 0x40;
}

void ed_scalarmul(
	xyzt* R, const bn X,
	const xy2d P[16][65536]
) {
	ed_xyzt_id(R);
	for (u32 i = 0; i < 8; i++) {
		u32 lo = X[i] & 0xffff;
		u32 hi = X[i] >> 16;

		ed_add_xy2d(R, R, &P[2*i  ][lo]);
		ed_add_xy2d(R, R, &P[2*i+1][hi]);
	}
}

void ed_encode(u8 R[32], const xyzt* P) {
	bn_copy(U32(R), P->Y);
	R[31] |= (P->X[0] & 1) << 7;
	bn_bswap(U32(R), U32(R));
}

#include "core.h"
#include "const.h"

static void u256_shr16(u32 R[8], const u32 X[8]) {
	for (u32 i = 0; i < 7; i++)
		R[i] = (X[i] >> 16) | (X[i+1] << 16);

	R[7] = X[7] >> 16;
}

static void ed_xyzt_id(ed_xyzt* R, const ed_ctx* Ed) {
	u256_zero(R->X);
	u256_copy(R->Y, Ed->Mo._1);
	u256_zero(R->T);
	u256_copy(R->Z, Ed->Mo._1);
}

static void ed_xyzt_copy(ed_xyzt* R, const ed_xyzt* P) {
	u256_copy(R->X, P->X);
	u256_copy(R->Y, P->Y);
	u256_copy(R->T, P->T);
	u256_copy(R->Z, P->Z);
}

static void ed_xy2d_init(
	ed_xy2d* R, const ed_xyzt* P,
	const ed_ctx* Ed
) {
	const monty_ctx* Mo = &Ed->Mo;

	u256_modsub(R->A, P->Y, P->X, Mo->M);
	u256_modadd(R->B, P->Y, P->X, Mo->M);
	monty_mul(R->C, P->T, Ed->_2D, Mo);
}

void ed_init25519(ed_ctx* Ed) {
	monty_ctx* Mo = &Ed->Mo;
	monty_init(Mo, ED25519_M);

	monty_mul(Ed->G.X, ED25519_GX, Mo->R2, Mo);
	monty_mul(Ed->G.Y, ED25519_GY, Mo->R2, Mo);
	monty_mul(Ed->G.T, Ed->G.X, Ed->G.Y, Mo);
	u256_copy(Ed->G.Z, Mo->_1);

	monty_mul(Ed->_2D, ED25519_D, Mo->R2, Mo);
	u256_modadd(Ed->_2D, Ed->_2D, Ed->_2D, Mo->M);
}

void ed_norm256(
	ed_xyzt R[256],
	const ed_xyzt P[256],
	const ed_ctx* Ed
) {
	const monty_ctx* Mo = &Ed->Mo;
	const u32 (*Zs)[8] = (void*)P[0].Z;

	u32 Z_inv[256][8];
	monty_inv256(Z_inv, Zs, 4, Mo);

	for (u32 i = 0; i < 256; i++) {
		monty_mul(R[i].X, P[i].X, Z_inv[i], Mo);
		monty_mul(R[i].Y, P[i].Y, Z_inv[i], Mo);
		monty_mul(R[i].T, R[i].X, R[i].Y, Mo);
		u256_copy(R[i].Z, Mo->_1);
	}
}

/////////////////////////////////////////////////

static void ed_add(
	ed_xyzt* R,
	const u32 A[8], const u32 B[8],
	const u32 C[8], const u32 D[8],
	const ed_ctx* Ed
) {
	const monty_ctx* Mo = &Ed->Mo;

	u32 E[8], F[8], G[8], H[8];

	// `E = B - A`, `F = D - C`
	// `G = D + C`, `H = B + A`
	u256_modsub(E, B, A, Mo->M);
	u256_modsub(F, D, C, Mo->M);
	u256_modadd(G, D, C, Mo->M);
	u256_modadd(H, B, A, Mo->M);

	// `R->X = E * F`, `R->Y = G * H`
	// `R->T = E * H`, `R->Z = F * G`
	monty_mul(R->X, E, F, Mo);
	monty_mul(R->Y, G, H, Mo);
	monty_mul(R->T, E, H, Mo);
	monty_mul(R->Z, F, G, Mo);
}

void ed_add_xyzt(
	ed_xyzt* R,
	const ed_xyzt* P,
	const ed_xyzt* Q,
	const ed_ctx* Ed
) {
	const monty_ctx* Mo = &Ed->Mo;

	u32 A[8], B[8], C[8], D[8];

	// `A = (P->Y - P->X) * (Q->Y - Q->X)`
	// using B as `Q->Y - Q->X`
	u256_modsub(A, P->Y, P->X, Mo->M);
	u256_modsub(B, Q->Y, Q->X, Mo->M);
	monty_mul(A, A, B, Mo);

	// `B = (P->Y + P->X) * (Q->Y + Q->X)`
	// using C as `Q->Y + Q->X`
	u256_modadd(B, P->Y, P->X, Mo->M);
	u256_modadd(C, Q->Y, Q->X, Mo->M);
	monty_mul(B, B, C, Mo);

	// C = Ed->_2D * P->T * Q->T
	monty_mul(C, P->T, Q->T, Mo);
	monty_mul(C, C, Ed->_2D, Mo);

	// D = 2 * P->Z * Q->Z
	monty_mul(D, P->Z, Q->Z, Mo);
	u256_modadd(D, D, D, Mo->M);

	ed_add(R, A, B, C, D, Ed);
}

void ed_add_xy2d(
	ed_xyzt* R,
	const ed_xyzt* P,
	const ed_xy2d* Q,
	const ed_ctx* Ed
) {
	const monty_ctx* Mo = &Ed->Mo;

	u32 A[8], B[8], C[8], D[8];

	// `A = (P->Y - P->X) * (Q->Y - Q->X)`
	u256_modsub(A, P->Y, P->X, Mo->M);
	monty_mul(A, A, Q->A, Mo);

	// `B = (P->Y + P->X) * (Q->Y + Q->X)`
	u256_modadd(B, P->Y, P->X, Mo->M);
	monty_mul(B, B, Q->B, Mo);

	// C = Ed->_2D * P->T * Q->T
	// D = 2 * P->Z * Q->Z
	monty_mul(C, P->T, Q->C, Mo);
	u256_modadd(D, P->Z, P->Z, Mo->M);

	ed_add(R, A, B, C, D, Ed);
}

/////////////////////////////////////////////////

// `Rs[i] = P + i * Q` in Montgomery space
static void ed_cummul(
	ed_xyzt Rs[],
	const ed_xyzt* P,
	const ed_xyzt* Q,
	u32 n, const ed_ctx* Ed
) {
	ed_xyzt_copy(&Rs[0], P);
	for (u32 i = 1; i < n; i++)
		ed_add_xyzt(&Rs[i], &Rs[i-1], Q, Ed);
}

static void ed_precomp1(
	ed_xy2d R[65536],
	ed_xyzt* P, const ed_xyzt* G,
	const ed_ctx* Ed
) {
	ed_xyzt T[256];

	for (u32 j = 0; j < 65536; j += 256) {
		ed_cummul(T, P, G, 256, Ed);

		// `P = P + 256 * G`
		ed_add_xyzt(P, &T[255], G, Ed);
		ed_norm256(T, T, Ed);

		for (u32 k = 0; k < 256; k++)
			ed_xy2d_init(&R[j + k], &T[k], Ed);
	}
}

void ed_precomp16(
	ed_xy2d R[16][65536],
	const ed_ctx* Ed
) {
	ed_xyzt P, G, T[256];
	ed_xyzt_copy(&G, &Ed->G);

	for (u32 i = 0; i < 16; i++) {
		ed_xyzt_id(&P, Ed);
		ed_precomp1(R[i], &P, &G, Ed);

		// `G = (2^16) * G`
		for (u32 j = 0; j < 16; j++)
			ed_add_xyzt(&G, &G, &G, Ed);
	}
}

void ed_mul(
	ed_xyzt* R, const u32 X[8],
	const ed_xy2d P[16][65536],
	const ed_ctx* Ed
) {
	u32 X_[8]; u256_copy(X_, X);
	ed_xyzt_id(R, Ed);

	for (u32 i = 0; i < 16; i++) {
		u32 k = X_[0] & 0xffff;
		ed_add_xy2d(R, R, &P[i][k], Ed);
		u256_shr16(X_, X_);
	}
}

#include "core.h"

inline static
void u256_shr16(u32 R[8], const u32 X[8]) {
	for (u32 i = 0; i < 7; i++)
		R[i] = (X[i] >> 16) | (X[i+1] << 16);

	R[7] = X[7] >> 16;
}

inline static void point4_zero(point4* R) {
	u256_zero(R->X);
	u256_zero(R->Y);
	u256_zero(R->T);
	u256_zero(R->Z);
}

inline static
void point4_copy(point4* R, const point4* P) {
	u256_copy(R->X, P->X);
	u256_copy(R->Y, P->Y);
	u256_copy(R->T, P->T);
	u256_copy(R->Z, P->Z);
}

void edwards_init25519(edwards* Ed) {
	monty* Mo = &Ed->Mo;
	monty_init(Mo, ED25519_M);

	monty_inj(Ed->G.X, ED25519_GX, Mo);
	monty_inj(Ed->G.Y, ED25519_GY, Mo);
	monty_mul(Ed->G.T, Ed->G.X, Ed->G.Y, Mo);
	u256_copy(Ed->G.Z, Mo->_1);
}

void edwards_normN(
	point4 Rs[], const point4 Ps[],
	u32 n, const edwards* Ed
) {
	const monty* Mo = &Ed->Mo;
	const u32 (*Zs)[8] = (void*)Ps[0].Z;

	u32 Z_inv[n][8];
	monty_invN(Z_inv, Zs, n, 4, Mo);

	for (u32 i = 0; i < n; i++) {
		monty_mul(Rs[i].X, Ps[i].X, Z_inv[i], Mo);
		monty_mul(Rs[i].Y, Ps[i].Y, Z_inv[i], Mo);
		monty_mul(Rs[i].T, Rs[i].X, Rs[i].Y, Mo);
		u256_copy(Rs[i].Z, Mo->_1);
	}
}

/////////////////////////////////////////////////

void edwards_add(
	point4* R,
	const point4* P,
	const point4* Q,
	const edwards* Ed
) {
	const monty* Mo = &Ed->Mo;

	if (u256_is_zero(P->Z)) {
		point4_copy(R, Q);
		return;
	}

	u32 A[8], B[8], C[8], D[8];

	// `A = (P->Y - P->X) * (Q->Y + Q->X)`
	// using B as `Q->Y + Q->X`
	u256_modsub(A, P->Y, P->X, Mo->M);
	u256_modadd(B, Q->Y, Q->X, Mo->M);
	monty_mul(A, A, B, Mo);

	// `B = (P->Y + P->X) * (Q->Y - Q->X)`
	// using C as `Q->Y - Q->X`
	u256_modadd(B, P->Y, P->X, Mo->M);
	u256_modsub(C, Q->Y, Q->X, Mo->M);
	monty_mul(B, B, C, Mo);

	// `C = 2 * P->Z * Q->T`
	monty_mul(C, P->Z, Q->T, Mo);
	u256_modadd(C, C, C, Mo->M);

	// `D = 2 * P->T * Q->Z`
	if (u256_cmp(Q->Z, Mo->_1) == EQ)
		u256_modadd(D, P->T, P->T, Mo->M);
	else {
		monty_mul(D, P->T, Q->Z, Mo);
		u256_modadd(D, D, D, Mo->M);
	};

	u32 E[8], F[8], G[8], H[8];

	// `E = D+C`, `F = B-A`, `G = B+A`, `H = D-C`
	u256_modadd(E, D, C, Mo->M);
	u256_modsub(F, B, A, Mo->M);
	u256_modadd(G, B, A, Mo->M);
	u256_modsub(H, D, C, Mo->M);

	// `R->X = E*F`, `R->Y = G*H`
	// `R->T = E*H`, `R->Z = F*G`
	monty_mul(R->X, E, F, Mo);
	monty_mul(R->Y, G, H, Mo);
	monty_mul(R->T, E, H, Mo);
	monty_mul(R->Z, F, G, Mo);
}

/////////////////////////////////////////////////

void edwards_double(
	point4* R, const point4* P,
	const edwards* Ed
) {
	const monty* Mo = &Ed->Mo;

	u32 A[8], B[8], C[8], D[8];

	// `A = P->X^2`, `B = P->Y^2`
	monty_mul(A, P->X, P->X, Mo);
	monty_mul(B, P->Y, P->Y, Mo);

	// `C = 2 * P->Z^2`, `D = -A`
	monty_mul(C, P->Z, P->Z, Mo);
	u256_modadd(C, C, C, Mo->M);
	u256_sub(D, Mo->M, A);

	u32 E[8], F[8], G[8], H[8];

	// `E = (P->X+P->Y)^2 - A - B`
	u256_modadd(E, P->X, P->Y, Mo->M);
	monty_mul(E, E, E, Mo);

	u256_modsub(E, E, A, Mo->M);
	u256_modsub(E, E, B, Mo->M);

	// `G = D+B`, `F = G-C`, `H = D-B`
	u256_modadd(G, D, B, Mo->M);
	u256_modsub(F, G, C, Mo->M);
	u256_modsub(H, D, B, Mo->M);

	// `R->X = E*F`, `R->Y = G*H`
	// `R->T = E*H`, `R->Z = F*G`
	monty_mul(R->X, E, F, Mo);
	monty_mul(R->Y, G, H, Mo);
	monty_mul(R->T, E, H, Mo);
	monty_mul(R->Z, F, G, Mo);
}

/////////////////////////////////////////////////

void edwards_precomp16(
	point4 Rs[16][1<<16],
	const edwards* Ed
) {
	point4 B; point4_copy(&B, &Ed->G);

	for (u32 i = 0; i < 16; i++) {
		point4* Rs0 = Rs[i];

		point4_zero(&Rs0[0]);
		Rs0[0].Z[0] = 1;

		point4_copy(&Rs0[1], &B);
		edwards_double(&Rs0[2], &B, Ed);

		for (u32 j = 3; j < (1<<16); j++)
			edwards_add(&Rs0[j], &Rs0[j-1], &B, Ed);

		for (u32 j = 0; j < (1<<16); j += 64)
			edwards_normN(&Rs0[j], &Rs0[j], 64, Ed);

		for (u32 j = 0; j < 16; j++)
			edwards_double(&B, &B, Ed);
	}
}

void edwards_mul(
	point4* R, const u32 X[8],
	const point4 Ps[16][1<<16],
	const edwards* Ed
) {
	u32 X_[8]; u256_copy(X_, X);
	point4_zero(R);

	for (u32 i = 0; i < 16; i++) {
		u32 k = X_[0] & 0xffff;
		edwards_add(R, R, &Ps[i][k], Ed);
		u256_shr16(X_, X_);
	}
}

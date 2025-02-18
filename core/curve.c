#include "core.h"

const u32 SECP256K1_M[8] = {
	0xfffffc2f, 0xfffffffe, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};

const u32 SECP256K1_GX[8] = {
	0x16f81798, 0x59f2815b, 0x2dce28d9, 0x029bfcdb,
	0xce870b07, 0x55a06295, 0xf9dcbbac, 0x79be667e
};

const u32 SECP256K1_GY[8] = {
	0xfb10d4b8, 0x9c47d08f, 0xa6855419, 0xfd17b448,
	0x0e1108a8, 0x5da4fbfc, 0x26a3c465, 0x483ada77
};

void secp256k1_init(weiers* We) {
	monty* Mo = &We->Mo;
	monty_init(Mo, SECP256K1_M);

	u256_zero(We->A);
	u256_zero(We->B); We->B[0] = 7;
	monty_inj(We->B, We->B, Mo);

	monty_inj(We->G.X, SECP256K1_GX, Mo);
	monty_inj(We->G.Y, SECP256K1_GY, Mo);
}

const u32 ED25519_M[8] = {
	0xffffffed, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff
};

const u32 ED25519_D[8] = {
	0x135978a3, 0x75eb4dca, 0x4141d8ab, 0x00700a4d,
	0x7779e898, 0x8cc74079, 0x2b6ffe73, 0x52036cee
};

const u32 ED25519_GX[8] = {
	0x66666658, 0x66666666, 0x66666666, 0x66666666,
	0x66666666, 0x66666666, 0x66666666, 0x66666666
};

const u32 ED25519_GY[8] = {
	0x8f25d51a, 0xc9562d60, 0x9525a7b2, 0x692cc760,
	0xfdd6dc5c, 0xc0a4e231, 0xcd6e53fe, 0x216936d3
};

void ed25519_init(edwards* Ed) {
	monty* Mo = &Ed->Mo;
	monty_init(Mo, ED25519_M);

	u256_sub(Ed->A, Mo->M, Mo->_1);
	monty_inj(Ed->D, ED25519_D, Mo);

	monty_inj(Ed->G.X, ED25519_GX, Mo);
	monty_inj(Ed->G.Y, ED25519_GY, Mo);
}

/////////////////////////////////////////////////

// https://hyperelliptic.org/EFD/g1p/auto-shortw-projective.html

void weiers_add(
	point* R,
	const point* P,
	const point* Q,
	const weiers* We
) {
	const monty* Mo = &We->Mo;
}

void weiers_double(
	point* R, const point* P,
	const weiers* We
) {
	const monty* Mo = &We->Mo;
}

/////////////////////////////////////////////////

// https://hyperelliptic.org/EFD/g1p/auto-twisted-projective.html

void edwards_add(
	point3* R,
	const point3* P,
	const point* Q,
	const edwards* Ed
) {
	const monty* Mo = &Ed->Mo;
}

void edwards_double(
	point* R, const point* P,
	const edwards* Ed
) {
	const monty* Mo = &Ed->Mo;
}

void point_redc(
	point* R, const point3* P,
	const monty* Mo
) {
	u32 Z_inv[8];
	monty_inv(Z_inv, P->Z, Mo);
	monty_redc(Z_inv, Z_inv, Mo);

	monty_mul(R->X, P->X, Z_inv, Mo);
	monty_mul(R->Y, P->Y, Z_inv, Mo);
}

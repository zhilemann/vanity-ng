#if !defined(CORE_H)
#define CORE_H

typedef unsigned int u32;
u32 mul_hi(u32 x, u32 y);

#if defined(__OPENCL_VERSION__)
	typedef unsigned long u64;
#else
	typedef unsigned long long u64;
#endif

typedef enum { LT, EQ, GT } ord;

// Montgomery space
typedef struct {
	u32 M[8], inv32; // `M * inv32 ~= -1` (mod `M`)

	u32 _1[8]; // `_1 ~= 2^256` (mod `M`)
	u32 R2[8]; // `R2 ~= (2^256)^2` (mod `M`)
	u32 R3[8]; // `R3 ~= (2^256)^3` (mod `M`)
} monty;

void u256_zero(u32 R[8]);
void u256_copy(u32 R[8], const u32 X[8]);

u32 u256_is_zero(const u32 X[8]);
ord u256_cmp(const u32 X[8], const u32 Y[8]);

u32 u256_add(u32 R[8], const u32 X[8], const u32 Y[8]);
u32 u256_sub(u32 R[8], const u32 X[8], const u32 Y[8]);

// `R ~= X + Y` (mod M)
void u256_modadd(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8],
	const u32 M[8]
);

// `R ~= X - Y` (mod M)
void u256_modsub(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8],
	const u32 M[8]
);

void u256_mul(u32 R[8], const u32 X[8], const u32 Y[8]);
void u256_pow32(u32 R[8], const u32 X[8], u32 k);

void u256_divmod(
	u32 Rq[8], u32 Rr[8],
	const u32 X[8], const u32 Y[8]
);

// `R * X ~= 1` (mod M)
void u256_modinv(
	u32 R[8], const u32 X[8],
	const u32 M[8]
);

void monty_init(monty* Mo, const u32 M[8]);

// inject `X` into Montgomery space
void monty_inj(
	u32 R[8], const u32 X[8],
	const monty* Mo
);

// extract `X` from Montgomery space
void monty_redc(
	u32 R[8], const u32 X[8],
	const monty *Mo
);

// `R = X * Y` (mod M) in Montgomery space
void monty_mul(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8],
	const monty *Mo
);

// `Rs[i] * Xs[st*i] = 1` (mod M) in Montgomery space
// allocates `32*n` bytes on stack
void monty_invN(
	u32 Rs[][8], const u32 Xs[][8],
	u32 n, u32 st, const monty* Mo
);

/////////////////////////////////////////////////

static const u32 ED25519_M[8] = {
	0xffffffed, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff
};

static const u32 ED25519_GX[8] = {
	0x8f25d51a, 0xc9562d60, 0x9525a7b2, 0x692cc760,
	0xfdd6dc5c, 0xc0a4e231, 0xcd6e53fe, 0x216936d3
};

static const u32 ED25519_GY[8] = {
	0x66666658, 0x66666666, 0x66666666, 0x66666666,
	0x66666666, 0x66666666, 0x66666666, 0x66666666
};

// point in extended coordinates
typedef struct {
	u32 X[8]; // `X = x*Z`
	u32 Y[8]; // `Y = y*Z`
	u32 T[8]; // `T = x*y*Z`
	u32 Z[8];
} point4;

// Edwards curve in Montgomery space
// `(A*X^2 + Y^2)*Z^2 = Z^4 + D*(XY)^2`
// assume that `A = -1`
typedef struct {
	monty Mo; point4 G; // generator point
} edwards;

// init Ed25519 curve in Montgomery space
void edwards_init25519(edwards* Ed);

// `R[i] = P[i] / P[i].Z` in Montgomery space
// allocates `64*n` bytes on stack
void edwards_normN(
	point4 Rs[], const point4 Ps[],
	u32 n, const edwards* Ed
);

// `R = P + Q` in Montgomery space
void edwards_add(
	point4* R,
	const point4* P,
	const point4* Q,
	const edwards* Ed
);

// `R = P + P` in Montgomery space
void edwards_double(
	point4* R, const point4* P,
	const edwards* Ed
);

// precompute 16-bit combs of `Ed->G`
void edwards_precomp16(
	point4 Rs[16][1<<16],
	const edwards* Ed
);

// `R = X * Ed->G` in Montgomery space
// (`X` is a regular number)
void edwards_mul(
	point4* R, const u32 X[8],
	const point4 Ps[16][1<<16],
	const edwards* Ed
);

#endif

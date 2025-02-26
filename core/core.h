#if !defined(CORE_H)
#define CORE_H

typedef unsigned char u8;
typedef unsigned int u32;

#if defined(__OPENCL_VERSION__)
	typedef long i64;
	typedef unsigned long u64;
#else
	typedef long long i64;
	typedef unsigned long long u64;
#endif

typedef enum { LT, EQ, GT } ord;

// Montgomery space
typedef struct {
	u32 M[8], inv32; // `M * inv32 ~= -1` (mod `M`)

	u32 _1[8]; // `_1 ~= 2^256` (mod `M`)
	u32 R2[8]; // `R2 ~= (2^256)^2` (mod `M`)
	u32 R3[8]; // `R3 ~= (2^256)^3` (mod `M`)
} monty_ctx;

void u256_zero(u32 R[8]);
void u256_copy(u32 R[8], const u32 X[8]);

u32 u256_is_zero(const u32 X[8]);
ord u256_cmp(const u32 X[8], const u32 Y[8]);

u32 u256_add(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8]
);

u32 u256_sub(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8]
);

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

// `R = X * Y`
void u256_mul(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8]
);

// `R = X ^ k`
void u256_pow32(u32 R[8], const u32 X[8], u32 k);

// `Rq = X / Y`, `Rr = X % Y`
void u256_divmod(
	u32 Rq[8], u32 Rr[8],
	const u32 X[8], const u32 Y[8]
);

// `R * X ~= 1` (mod M)
void u256_modinv(
	u32 R[8], const u32 X[8],
	const u32 M[8]
);

void monty_init(monty_ctx* Mo, const u32 M[8]);

// extract `X` from Montgomery space
void monty_redc(
	u32 R[8], const u32 X[8],
	const monty_ctx* Mo
);

// `R = X * Y` (mod M) in Montgomery space
void monty_mul(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8],
	const monty_ctx* Mo
);

// `R[i] * X[st*i] = 1` (mod M) in Montgomery space
void monty_inv256(
	u32 R[256][8], const u32 X[][8],
	u32 st, const monty_ctx* Mo
);

/////////////////////////////////////////////////


// point in extended coordinates
typedef struct {
	u32 X[8]; // `X = x*Z`
	u32 Y[8]; // `Y = y*Z`
	u32 T[8]; // `T = x*y*Z`
	u32 Z[8];
} ed_xyzt;

typedef struct {
	u32 A[8]; // `A = Y - X`
	u32 B[8]; // `B = Y + X`
	u32 C[8]; // `C = (2*D) * X * Y`
} ed_xy2d;

// Edwards curve in Montgomery space
// `(A*X^2 + Y^2)*Z^2 = Z^4 + D*(XY)^2`
// assume that `A = -1`
typedef struct {
	monty_ctx Mo;
	ed_xyzt G; // generator point
	u32 _2D[8]; // `_2D = 2 * D`
} ed_ctx;

// init Ed25519 curve in Montgomery space
void ed_init25519(ed_ctx* Ed);

// `R[i] = P[i] / P[i].Z` in Montgomery space
void ed_norm256(
	ed_xyzt R[256],
	const ed_xyzt P[256],
	const ed_ctx* Ed
);

// `R = P + Q` in Montgomery space
void ed_add_xyzt(
	ed_xyzt* R,
	const ed_xyzt* P,
	const ed_xyzt* Q,
	const ed_ctx* Ed
);

// `R = P + Q` in Montgomery space
void ed_add_xy2d(
	ed_xyzt* R,
	const ed_xyzt* P,
	const ed_xy2d* Q,
	const ed_ctx* Ed
);

// precompute 16-bit combs of `Ed->G`
void ed_precomp16(
	ed_xy2d R[16][65536],
	const ed_ctx* Ed
);

// `R = X * P` in Montgomery space
// uses combs from `ed_precomp16`
void ed_mul(
	ed_xyzt* R, const u32 X[8],
	const ed_xy2d P[16][65536],
	const ed_ctx* Ed
);

/////////////////////////////////////////////////

// add `1` bit and length to block `R`
void sha512_pad(u8 R[128], u32 n);

// compute SHA-512 for block `X`
void sha512(u64 R[8], const u8 X[128]);

#endif

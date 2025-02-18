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

typedef struct {
	u32 M[8];
	u32 inv32; // `M * inv32 ~= -1` (mod `M`)
	u32 _1[8]; // `_1 ~= 2^256` (mod `M`)
	u32 R2[8]; // `R2 ~= (2^256)^2` (mod `M`)
	u32 R3[8]; // `R3 ~= (2^256)^3` (mod `M`)
} monty;

typedef struct {
	u32 X[8], Y[8];
} point;

typedef struct {
	u32 X[8], Y[8], Z[8];
} point3;

typedef struct {
	monty Mo;

	// Y^2*Z = X^3 + A*X*Z^2 + B*Z^3
	u32 A[8], B[8]; point G;
} weiers;

typedef struct {
	monty Mo;

	// (A*X^2 + Y^2)*Z^2 = Z^4 + D*(XY)^2
	u32 A[8], D[8]; point G;
} edwards;

/////////////////////////////////////////////////

void u256_zero(u32 R[8]);
void u256_copy(u32 R[8], const u32 X[8]);

u32 u256_is_zero(const u32 X[8]);
ord u256_cmp(const u32 X[8], const u32 Y[8]);

u32 u256_add(u32 R[8], const u32 X[8], const u32 Y[8]);
u32 u256_sub(u32 R[8], const u32 X[8], const u32 Y[8]);

void u256_modadd(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8],
	const u32 M[8]
);

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

void u256_modinv(
	u32 R[8], const u32 X[8],
	const u32 M[8]
);

void monty_init(monty* Mo, const u32 M[8]);

void monty_inj(
	u32 R[8], const u32 X[8],
	const monty* Mo
);

void monty_redc(
	u32 R[8], const u32 X[8],
	const monty *Mo
);

void monty_mul(
	u32 R[8],
	const u32 X[8],
	const u32 Y[8],
	const monty *Mo
);

void monty_inv(
	u32 R[8], const u32 X[8],
	const monty* Mo
);

/////////////////////////////////////////////////

void secp256k1_init(weiers* We);
void ed25519_init(edwards* Ed);

void weiers_add(
	point* R,
	const point* P,
	const point* Q,
	const weiers* We
);

void weiers_double(
	point* R, const point* P,
	const weiers* We
);

void edwards_add(
	point3* R,
	const point3* P,
	const point* Q,
	const edwards* Ed
);

void edwards_double(
	point* R, const point* P,
	const edwards* Ed
);

void point_redc(
	point* R, const point3* P,
	const monty* Mo
);

#endif

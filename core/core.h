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
	u32 M[8], M_inv32, R3[8];
} monty;

typedef struct { u32 x[8], y[8]; } point;

u32 u32_adc(u32 x, u32 y, u32 cf, u32 *cf1);
u32 u32_sbb(u32 x, u32 y, u32 bf, u32 *bf1);

void u256_fill(u32 A[8], u32 x);
void u256_copy(u32 A[8], const u32 X[8]);

ord u256_cmp(const u32 X[8], const u32 Y[8]);

u32 u256_add(u32 A[8], const u32 X[8], const u32 Y[8]);
u32 u256_sub(u32 A[8], const u32 X[8], const u32 Y[8]);

void u256_modadd(
	u32 A[8], const u32 X[8],
	const u32 Y[8], const u32 M[8]
);

void u256_modsub(
	u32 A[8], const u32 X[8],
	const u32 Y[8], const u32 M[8]
);

u32 u256_muladd(
	u32 A[8], const u32 X[8],
	const u32 Y[8], u32 k
);

u32 u256_mulsub(
	u32 A[8], const u32 X[8],
	const u32 Y[8], u32 k
);

void u256_modinv
(u32 A[8], const u32 X[8], const u32 M[8]);

void monty_redc
(u32 A[8], const u32 X[8], const monty *Mo);

void monty_mul(
	u32 A[8], const u32 X[8],
	const u32 Y[8], const monty *Mo
);

void u256_divmod(
	u32 A[8], u32 B[8],
	const u32 X[8], const u32 Y[8]
);

#endif

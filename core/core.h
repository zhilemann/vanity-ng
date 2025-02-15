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
	u32 R2[8]; // `R2 ~= (2^256)^2` (mod `M`)
	u32 R3[8]; // `R3 ~= (2^256)^3` (mod `M`)
} monty;

u32 u32_adc(u32 x, u32 y, u32 cf, u32 *cf1);
u32 u32_sbb(u32 x, u32 y, u32 bf, u32 *bf1);

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

#endif

#if !defined(CORE_H)
#define CORE_H

typedef unsigned char u8;

typedef unsigned int u32;
typedef u32 bn[8];

#if defined(__OPENCL_VERSION__)
	typedef long i64;
	typedef unsigned long u64;
#else
	typedef long long i64;
	typedef unsigned long long u64;
#endif

#define U8(x) ((u8*)x)
#define U32(x) ((u32*)x)
#define U64(x) ((u64*)x)

typedef enum { LT, EQ, GT } ord;

u32 u32_rol(u32 x, u32 n); u32 u32_ror(u32 x, u32 n);
u64 u64_rol(u64 x, u32 n); u64 u64_ror(u64 x, u32 n);

u32 u32_bswap(u32 x); u64 u64_bswap(u64 x);

void u256_zero(bn R);
void u256_copy(bn R, const bn X);
void u256_bswap(bn R, const bn X);

void bn_zero(bn R);
void bn_copy(bn R, const bn X);
void bn_bswap(bn R, const bn X);

u32 bn_is_zero(const bn X);
ord bn_cmp(const bn X, const bn Y);

u32 bn_add32(bn R, u32 x);
u32 bn_add(bn R, const bn X, const bn Y);
u32 bn_sub(bn R, const bn X, const bn Y);

void bn_modadd(bn R, const bn X, const bn Y, const bn M);
void bn_modsub(bn R, const bn X, const bn Y, const bn M);

u32 bn_muladd(bn R, u32 a, const bn X);

void bn_mulw(u32 R[16], const bn X, const bn Y);

void bn_divmod(bn Rq, bn Rr, const bn X, const bn Y);
void bn_modinv(bn R, const bn X, const bn M);

/////////////////////////////////////////////////

typedef struct {
	bn X; // `X = x*Z`
	bn Y; // `Y = y*Z`
	bn T; // `T = x*y*Z`
	bn Z;
} xyzt;

typedef struct {
	bn A; // `A = Y - X`
	bn B; // `B = Y + X`
	bn C; // `C = (2*D) * X * Y`
} xy2d;

void ed_norm256(xyzt R[256], const xyzt P[256]);

void ed_comb16_gen(xy2d R[16][65536]);

void ed_scalarmul(
	xyzt* R, const bn X,
	const xy2d P[16][65536]
);

/////////////////////////////////////////////////

// `SHA-512(X)`, assumes `n < 112`
void sha512(u64 R[8], const u8 X[], u32 n);

#endif

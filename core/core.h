#if !defined(CORE_H)
#define CORE_H

typedef unsigned char u8;
typedef unsigned int u32;

typedef long long i64;
typedef unsigned long long u64;

#define U8(x) ((u8*)x)
#define U32(x) ((u32*)x)
#define U64(x) ((u64*)x)

typedef u32 bn[8];
typedef u32 bn_1[9];
typedef u32 bn_2[16];

typedef enum { LT, EQ, GT } ord;

#define __WIDTH(x) (8*sizeof(x))
#define ROL(x, n) (x<<n | x>>(__WIDTH(x)-n))
#define ROR(x, n) (x>>n | x<<(__WIDTH(x)-n))

u32 u32_bswap(u32 x);
u64 u64_bswap(u64 x);

void u256_zero(bn R);
void u256_copy(bn R, const bn X);
void u256_bswap(bn R, const bn X);

void bn_zero(bn R);
void bn_copy(bn R, const bn X);
void bn_bswap(bn R, const bn X);

u32 bn_is_zero(const bn X);
ord bn_cmp(const bn X, const bn Y);

u32 bn_add32(bn R, const bn X, u32 y);
u32 bn_add(bn R, const bn X, const bn Y);
u32 bn_sub(bn R, const bn X, const bn Y);

void bn_modadd(bn R, const bn X, const bn Y, const bn M);
void bn_modsub(bn R, const bn X, const bn Y, const bn M);

u32 bn_muladd(bn R, const bn X, u32 a, const bn Y);
void bn_mul512(bn_2 R, const bn X, const bn Y);

void bn_divmod(bn Q, bn R, const bn X, const bn Y);
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

extern const xyzt ED25519_G;
typedef xy2d ed_comb[16][65536];

void ed_norm256(xyzt R[256], const xyzt P[256]);
void ed_comb16(ed_comb R, const xyzt* G);

void ed_privkey(u32 R[8], const u8 K[32]);
void ed_mul(xyzt* R, const bn X, const ed_comb P);
void ed_pubkey(bn R, const xyzt* P);

/////////////////////////////////////////////////

// `SHA-512(X)`, assumes `n < 112`
void sha512(u64 R[8], const u8 X[], u32 n);

#endif

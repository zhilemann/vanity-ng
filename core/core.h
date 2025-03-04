#if !defined(CORE_H)
#define CORE_H

typedef unsigned char u8;
typedef unsigned int u32;
typedef long long i64;
typedef unsigned long long u64;

#define U8(x) ((u8*)x)
#define U32(x) ((u32*)x)
#define U64(x) ((u64*)x)

#if !defined(__OPENCL_C_VERSION__)
	#define global
#endif

// fuck OpenCL :(
typedef struct { u32 d[8]; } bn_mut;
typedef const bn_mut* bn;

typedef union {
	u32 d[9];
	struct { bn_mut l; u32 h; };
} bn1_mut;

typedef union {
	u32 d[16];
	struct { bn_mut l, h; };
} bn2_mut;

typedef enum { LT, EQ, GT } ord;

#define __WIDTH(x) (8*sizeof(x))
#define ROL(x, n) (x<<n | x>>(__WIDTH(x)-n))
#define ROR(x, n) (x>>n | x<<(__WIDTH(x)-n))

u32 u32_bswap(u32 x);
u64 u64_bswap(u64 x);

void bn_zero(bn_mut* R);
void bn_bswap(bn_mut* R, bn X);

u32 bn_is_zero(bn X);
ord bn_cmp(bn X, bn Y);

u32 bn_add32(bn_mut* R, bn X, u32 y);
u32 bn_add(bn_mut* R, bn X, bn Y);
u32 bn_sub(bn_mut* R, bn X, bn Y);

void bn_modadd(bn_mut* R, bn X, bn Y, bn M);
void bn_modsub(bn_mut* R, bn X, bn Y, bn M);

u32 bn_muladd(bn_mut* R, bn X, u32 a, bn Y);
void bn_mul512(bn2_mut* R, bn X, bn Y);

void bn_divmod(bn_mut* Q, bn_mut* R, bn X, bn Y);
void bn_modinv(bn_mut* R, bn X, bn M);

/////////////////////////////////////////////////

typedef struct {
	bn_mut X; // `X = x*Z`
	bn_mut Y; // `Y = y*Z`
	bn_mut T; // `T = x*y*Z`
	bn_mut Z;
} xyzt;

typedef struct {
	bn_mut A; // `A = Y - X`
	bn_mut B; // `B = Y + X`
	bn_mut C; // `C = (2*D) * X * Y`
} xy2d;

extern const global xyzt ED25519_G;
typedef struct { xy2d p[16][65536]; } ed_comb;

void ed_norm256(xyzt* R, const xyzt* P);
void ed_comb16(ed_comb* R, const xyzt* G);

void ed_privkey(bn_mut* R, const u8* K);
void ed_mul(xyzt* R, bn X, const ed_comb* P);
void ed_pubkey(bn_mut* R, const xyzt* P);

/////////////////////////////////////////////////

// `SHA-512(X)`, assumes `n < 112`
void sha512(u64* R, const u8* X, u32 n);

// `K = A + B * i;`
typedef struct { bn_mut A, B; } vanity_rnd;

typedef struct {
	bn_mut P_lo, P_hi; // `P_lo <= X < P_hi`
	bn_mut S_m, S_eq; // `X ~= S_eq` (mod S_m)
} vanity_tgt;

#endif

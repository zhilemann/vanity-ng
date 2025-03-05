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

static const bn_mut BN_0 = {};

u32 u32_bswap(u32 x);
u64 u64_bswap(u64 x);
void bn_bswap(bn_mut* R, bn X);

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
	bn_mut x; // `X = x*Z`
	bn_mut y; // `Y = y*Z`
	bn_mut t; // `T = x*y*Z`
	bn_mut z;
} xyzt;

typedef struct {
	bn_mut a; // `A = Y - X`
	bn_mut b; // `B = Y + X`
	bn_mut c; // `C = (2*D) * X * Y`
} xy2d;

typedef struct {
	// `8*21 + 4*22` = 256
	xy2d a[8][1<<21], b[4][1<<22];
} ed_lut;

extern const xyzt ED_ID;
extern const global xyzt ED_G;

// uses `.t` and `.z` as scratch
void ed_normN(xyzt* R, const xyzt* P, u32 n);
void ed_lut_step(xy2d* R, xyzt* G, u32 w);
void ed_mul(xyzt* R, bn X, const ed_lut* L);

void ed_privkey(bn_mut* R, const u8* K);
void ed_pubkey(bn_mut* R, const xyzt* P);

/////////////////////////////////////////////////

// `SHA-512(X)`, assumes `n < 112`
void sha512(u64* R, const u8* X, u32 n);

typedef struct {
	u32 f; bn_mut S, K;
} vanity_res;

// `K = A + B * i;`
typedef struct { bn_mut A, B; } vanity_seed;

typedef struct {
	bn_mut Pl, Ph; // `Pl <= X <= Ph`
	bn_mut Sm, Sr; // `X ~= Sr` (mod Sm)
} vanity_tgt;

#endif

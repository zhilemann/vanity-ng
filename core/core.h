#if !defined(CORE_H)
#define CORE_H

#if defined(__OPENCL_C_VERSION__)
	typedef uchar u8;
	typedef uint u32;

	typedef long i64;
	typedef ulong u64;
#else
	#include <stdint.h>
	#include <inttypes.h>

	#define global
	#define constant

	typedef uint8_t u8;
	typedef uint32_t u32;

	typedef int64_t i64;
	typedef uint64_t u64;
#endif

#define U8(x) ((u8*)(x))
#define U32(x) ((u32*)(x))
#define U64(x) ((u64*)(x))
#define BN(x) ((bn_mut*)(x))

typedef struct { u8 b, m; } u8_pat;

typedef struct { u32 d[8]; } bn_mut;
typedef const bn_mut* bn;

typedef union {
	u32 d[9];
	struct { bn_mut l; u32 h; };
} bn1_mut;

typedef union {
	u32 d[16];
	struct { bn_mut l, h; };
	struct { u32 _; bn_mut l_32; };
} bn2_mut;

typedef enum { LT, EQ, GT } ord;

static const u32 BATCH = 256;
static const bn_mut BN_0 = {};

/////////////////////////////////////////////////

#define __WIDTH(x) (8*sizeof(x))
#define SWAP(x, y) ((x)^=(y), (y)^=(x), (x)^=(y))

#define ROL(x, n) ((x)<<(n) | (x)>>(__WIDTH(x)-(n)))
#define ROR(x, n) ((x)>>(n) | (x)<<(__WIDTH(x)-(n)))

void mem_copy(void* R, const void* X, u32 n);
void u8_base32(u8* R, const u8* X, u32 n);
u32 u8_pat_test(const u8* X, const u8_pat* P, u32 n);

u32 u32_bswap(u32 x);
u64 u64_bswap(u64 x);

void bn_neg(bn_mut* R, bn X);
void bn_bswap(bn_mut* R, bn X);

u64 bn_get64(bn X, u32 i);
ord bn_cmp(bn X, bn Y);
void bn_shrN(bn_mut* R, bn X, u32 n);

u32 bn_add64(bn_mut* R, bn X, u64 y);
u32 bn_add(bn_mut* R, bn X, bn Y);
u32 bn_sub(bn_mut* R, bn X, bn Y);

void bn_modadd(bn_mut* R, bn X, bn Y, bn M);
void bn_modsub(bn_mut* R, bn X, bn Y, bn M);

u32 bn_muladd(bn_mut* R, bn X, u32 a, bn Y);
void bn_mul512(bn2_mut* R, bn X, bn Y);

void bn_divmod(bn_mut* Qu, bn_mut* Re, bn X, bn Y);
void bn_modinv(bn_mut* R, bn X, bn M);

/////////////////////////////////////////////////

typedef struct { bn_mut x, y; } xy;
typedef struct { bn_mut x, y, t, z; } xytz;

typedef struct {
	bn_mut a; // `a = y - x`
	bn_mut b; // `b = y + x`
	bn_mut c; // `c = (2*D) * x * y`
} xy2d;

typedef struct {
	xy d[256]; // `d[i] = (2^i) * G`
} secp_lut_mul;

typedef struct {
	xy d[1<<24]; // `d[i] = (i+1) * G`
} secp_lut;

typedef struct {
	// `a[i][j] = (j << (21*i)) * G`
	xy2d a[8][1<<21];
	// `b[i][j] = (j << (22*i + 168)) * G`
	xy2d b[4][1<<22];
} ed_lut;

/////////////////////////////////////////////////

static const global xy SECP_G = {
{
	0x16f81798, 0x59f2815b, 0x2dce28d9, 0x029bfcdb,
	0xce870b07, 0x55a06295, 0xf9dcbbac, 0x79be667e, },
{
	0xfb10d4b8, 0x9c47d08f, 0xa6855419, 0xfd17b448,
	0x0e1108a8, 0x5da4fbfc, 0x26a3c465, 0x483ada77, } };

static const global xytz ED_ID = { {}, { 1 }, {}, { 1 } };

static const global xytz ED_G = {
{
	0x8f25d51a, 0xc9562d60, 0x9525a7b2, 0x692cc760,
	0xfdd6dc5c, 0xc0a4e231, 0xcd6e53fe, 0x216936d3, },
{
	0x66666658, 0x66666666, 0x66666666, 0x66666666,
	0x66666666, 0x66666666, 0x66666666, 0x66666666, },
{
	0xa5b7dda3, 0x6dde8ab3, 0x775152f5, 0x20f09f80,
	0x64abe37d, 0x66ea4e8e, 0xd78b7665, 0x67875f0f,
}, { 1 } };

// `R[i] = P + Q[i]`, returns 0 on error
u32 secp_addN(xy* R, const xy* P, const xy* Q, u32 n);
void secp_mul(xy* R, bn X, const secp_lut_mul* L);

// uses `.t` and `.z` as scratch
void ed_normN(xytz* R, const xytz* P, u32 n);
void ed_lut_step(xy2d* R, xytz* G, u32 n);
void ed_mul(xytz* R, bn X, const ed_lut* L);

void secp_pubkey33(u8* R, const xy* P);
void secp_pubkey64(u8* R, const xy* P);

void ed_privkey(bn_mut* R, bn S);
void ed_pubkey(bn_mut* R, const xytz* P);

/////////////////////////////////////////////////

typedef struct {
	u32 f; bn_mut s;
	union { bn_mut bn; u8 u8[40]; } k;
} vanity_res;

typedef struct {
	bn_mut x; xy p; // `p = x * SECP_G`
} secp_seed;

typedef struct {
	bn_mut l, h; // `l <= x <= h`
	bn_mut r, m; // `x ~= r` (mod m)
} bn_filt58;

void sha2_256(u8* R, const void* X, u32 n);
void sha2_512(u8* R, const void* X, u32 n);
void sha3_256(u8* R, const void* X, u32 n);

void ripemd160(u8* R, const void* X, u32 n);
void bech32(u8* R, u8* X, constant char hr[2], u8 wi);

#endif

#include "vanity.h"

static const str HEX = "0123456789abcdef";
static const str BECH32 =
	"qpzry9x8gf2tvdw0s3jn54khce6mua7l";

static const str BASE58 =
	"123456789ABCDEFGHJKLMNPQRSTUVWXYZ"
	"abcdefghijkmnopqrstuvwxyz";

static u32 vanity_read(void* R, str fp, u32 n) {
	FILE* F = fopen(fp, "rb");
	if (!F) return 0;

	u32 r = fread(R, n, 1, F);
	fclose(F); return r;
}

static u32 vanity_write(str fp, const void* X, u32 n) {
	FILE* F = fopen(fp, "wb");
	if (!F) return 0;

	u32 r = fwrite(X, n, 1, F);
	fclose(F); return r;
}

#if defined(WIN32)

#include <windows.h>
#include <ntsecapi.h>

void bn_rand(bn_mut* R) {
	ASSERT(RtlGenRandom(R, sizeof(bn_mut)));
}

#else

void bn_rand(bn_mut* R) {
	static FILE* F = NULL;
	if (!F) ASSERT(F = fopen("/dev/random", "rb"));
	ASSERT(fread(R, sizeof(bn_mut), 1, F));
}

#endif

/////////////////////////////////////////////////

static u32 str_find(char x, str X) {
	for (u32 i = 0; X[i] != 0; i++)
		if (X[i] == x) return i;

	VANITY_LOG("str_find: invalid char: %c\n", x);
	exit(-1);
}

void u8_print(const u8* X, u32 n) {
	printf("0x");
	for (u32 i = 0; i < n; i++)
		printf("%02x", X[i]);

	printf("\n");
}

void u8_print_bech32(const u8* X, u32 n) {
	for (u32 i = 0; i < n; i++)
		printf("%c", BECH32[X[i]]);

	printf("\n");
}

void bn_print(bn X) {
	printf("0x");
	for (u32 i = 7; i+1 > 0; i--)
		printf("%08x", X->d[i]);

	printf("\n");
}

void bn_print_base58(bn X, u32 z) {
	u8 R[64]; u32 n = 0;
	bn_mut X_ = *X, Y = { 58 }, T;

	for (
		u32 i = sizeof(bn_mut) - (z+1);
		U8(X)[i] == 0; i--
	) printf("1");

	while (bn_cmp(&X_, &BN_0) != EQ) {
		bn_divmod(&T, &X_, &X_, &Y);
		R[n] = BASE58[X_.d[0]], X_ = T, n++;
	}

	for (u32 i = n-1; i+1 > 0; i--)
		printf("%c", R[i]);

	printf("\n");
}

/////////////////////////////////////////////////

secp_lut* vanity_secp_lut(secp_lut_mul* Lm) {
	const str FP = "secp256k1.lut";

	Lm->d[0] = SECP_G;
	for (u32 i = 1; i < 256; i++) {
		xy* P = Lm->d + i;
		secp_addN(P, P-1, P-1, 1);
	}

	secp_lut* L = malloc(sizeof(secp_lut));
	if (!vanity_read(L, FP, sizeof(secp_lut))) {
		L->d[0] = SECP_G;
		for (u32 i = 0; i < 24; i++) {
			VANITY_LOG(
				"\r* build %s: %u/%u",
				FP, 1<<i, 1<<24);

			xy* P = L->d + (1<<i);
			secp_addN(P, P-1, L->d, 1<<i);
		}

		ASSERT(vanity_write(FP, L, sizeof(secp_lut)));
		VANITY_LOG("\r* build %s: OK\33[K\n", FP);
	}

	return L;
}

ed_lut* vanity_ed_lut() {
	const str FP = "Ed25519.lut";

	ed_lut* L = malloc(sizeof(ed_lut));
	if (!vanity_read(L, FP, sizeof(ed_lut))) {
		xytz G = ED_G;
		for (u32 i = 0; i < 12; i++) {
			VANITY_LOG(
				"\r* build %s: %u/256", FP,
				i < 8 ? 21*i : (22*i - 8));

			ed_lut_step(
				i < 8 ? L->a[i] : L->b[i-8], &G,
				i < 8 ? 1<<21 : 1<<22);
		}

		ASSERT(vanity_write(FP, L, sizeof(ed_lut)));
		VANITY_LOG("\r* build %s: OK\33[K\n", FP);
	}

	return L;
}

/////////////////////////////////////////////////

static void u8_pat_set4(u8_pat* P, u8 x, u32 i) {
	P[i/2].b |= (i % 2) ? x : (x << 4);
	P[i/2].m |= (i % 2) ? 0x0f : 0xf0;
}

u64 u8_pat_eth(u8_pat* R, str pr, str su) {
	u64 di = 1; const u32 N = 20;
	for (u32 i = 0; i < N; i++)
		R[i].b = R[i].m = 0;

	for (u32 i = 0; i < strlen(pr); i++) {
		u8 x = str_find(pr[i], HEX);
		u8_pat_set4(R, x, i), di *= 16;
	}

	u32 n = strlen(su);
	for (u32 i = 0; i < n; i++) {
		u8 x = str_find(su[i], HEX);
		u8_pat_set4(R, x, 2*N-n + i), di *= 16;
	}

	return di;
}

u64 u8_pat_bech32(u8_pat* R, str pr, str su) {
	u64 di = 1; const u32 N = 38;
	for (u32 i = 0; i < N; i++)
		R[i].b = R[i].m = 0;

	for (u32 i = 0; i < strlen(pr); i++) {
		R[i].b = str_find(pr[i], BECH32);
		R[i].m = -1, di *= 32;
	}

	u32 n = strlen(su);
	for (u32 i = 0; i < n; i++) {
		R[N-n + i].b = str_find(su[i], BECH32);
		R[N-n + i].m = -1, di *= 32;
	}

	return di;
}

/////////////////////////////////////////////////

static void bn_filt58_prefix(bn_filt58* F, str s, u32 z) {
	u32 n = 0; bn_mut T;
	while (s[n] == '1') n++;

	F->l = F->h = BN_0;
	for (u32 i = n; i < strlen(s); i++) {
		bn_muladd(&F->l, &F->l, 57, &F->l);
		bn_add64(&F->l, &F->l, str_find(s[i], BASE58));
	}

	while (
		bn_muladd(&T, &F->l, 57, &F->l) == 0 &&
		(n+z == 0 || U8(&T)[sizeof(bn_mut) - (n+z)] == 0)
	) {
		bn_muladd(&F->h, &F->h, 57, &F->h);
		bn_add64(&F->h, &F->h, 57), F->l = T;
	}
}

u64 bn_filt58_init(bn_filt58* F, str pr, u32 z, str su) {
	if (strlen(pr) > 0)
		bn_filt58_prefix(F, pr, z);
	else
		F->l = BN_0, bn_neg(&F->h, &BN_0);

	F->r = F->m = BN_0, F->m.d[0] = 1;
	for (u32 i = 0; i < strlen(su); i++) {
		bn_muladd(&F->r, &F->r, 57, &F->r);
		bn_muladd(&F->m, &F->m, 57, &F->m);
		bn_add64(&F->r, &F->r, str_find(su[i], BASE58));
	}

	bn_mut D = {};
	if (strlen(pr) > 0) {
		bn_mut N; bn_neg(&N, &BN_0);
		mem_copy(U8(&N+1) - z, &BN_0, z);
		bn_divmod(&D, &N, &N, &F->h);
	} else D.d[0] = 1;

	bn_add(&F->h, &F->l, &F->h);
	return bn_get64(&D, 0) * bn_get64(&F->m, 0);
}

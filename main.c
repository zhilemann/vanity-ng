#include "core/core.h"
#include "vanity.h"

#include <unistd.h>
#include <math.h>
#include <time.h>

typedef enum {
	VANITY_HELP,
	VANITY_ETH, VANITY_BTC_BASE58,
	VANITY_BTC_BECH32, VANITY_SOL
} vanity_mode;

static const str VANITY_MODE[5] = {
	"help", "eth", "btc_base58",
	"btc_bech32", "sol"
};

static vanity_mode vanity_get_mode(str x) {
	const u32 n = sizeof(VANITY_MODE) / sizeof(str);

	for (u32 i = 0; i < n; i++)
		if (strcmp(x, VANITY_MODE[i]) == 0)
			return i;

	return VANITY_HELP;
}

static void vanity_help() {
	VANITY_LOG(
		"usage: vanitygen2 -m ... "
		"[-p ...] [-s ...]\n");

	VANITY_LOG(
		"* -m        one of: eth, "
		"btc_base58, btc_bech32, sol\n");

	VANITY_LOG("* -p, -s    target prefix and suffix\n\n");
}

static str vanity_prefix(str x, str pr) {
	if (strlen(x) == 0) return x;

	for (u32 i = 0; i < strlen(pr); i++)
		if (x[i] != pr[i]) {
			VANITY_LOG(
				"vanity_prefix_test: "
				"prefix must start with %s\n", pr
			);

			exit(-1);
		}

	return x + strlen(pr);
}

/////////////////////////////////////////////////

static u64 vanity_config(
	cl2_ctx* Cl, vanity_mode mo,
	str pr, str su, secp_lut_mul* Lm
) {
	ed_lut* Le; secp_lut* Ls;
	u32 ed25519 = mo == VANITY_SOL;

	if (ed25519)
		Le = vanity_ed_lut();
	else
		Ls = vanity_secp_lut(Lm);

	u8_pat P[40]; bn_filt58 F;
	void* f; u32 n; u64 di;

	switch (mo) {
		case VANITY_ETH:
			di = u8_pat_eth(
				P, vanity_prefix(pr, "0x"), su);

			f = P, n = 2*20; break;

		case VANITY_BTC_BASE58:
			di = bn_filt58_init(
				&F, vanity_prefix(pr, "1"), 8, su);

			f = &F, n = sizeof(bn_filt58);
			break;

		case VANITY_BTC_BECH32:
			di = u8_pat_bech32(
				P, vanity_prefix(pr, "bc1q"), su);

			f = P, n = 2*38; break;

		case VANITY_SOL:
			di = bn_filt58_init(&F, pr, 0, su);
			f = &F, n = sizeof(bn_filt58);
			break;

		default: return -1;
	}

	if (ed25519)
		cl2_config(
			Cl, sizeof(bn_mut),
			f, n, Le, sizeof(ed_lut)
		), free(Le);
	else
		cl2_config(
			Cl, sizeof(secp_seed),
			f, n, Ls, sizeof(secp_lut)
		), free(Ls);

	return di;
}

/////////////////////////////////////////////////

static cl_event vanity_step(
	vanity_res* R, cl2_dev* D,
	const secp_lut_mul* Ls
) {
	secp_seed S; bn_rand(&S.x);
	if (Ls) secp_mul(&S.p, &S.x, Ls);
	else S.x.d[0] |= 1;

	cl_event ev = cl2_write(
		&D->S, D, (Ls ? &S : (void*)&S.x), NULL);

	ev = cl2_dispatch2(D, D->cu, D->wg, 1, D->wg, ev);
	return cl2_read(R, D, &D->R, ev);
}

static void vanity_run(
	vanity_res* R, cl2_ctx* Cl, u64 di,
	const secp_lut_mul* Ls
) {
	vanity_res Rt[Cl->n]; cl_event ev[Cl->n];
	VANITY_LOG("[??? key/s] just started...");

	u64 th = 0, n = 0, ts = clock();
	double p0 = 1 - 1./di, ph = log(0.5) / log(p0);

	for (u32 i = 0; i < Cl->n; i++) {
		cl2_dev* D = &Cl->D[i];
		th += D->cu * D->wg * BATCH;
	}

	u32 m = 0, dt;
	for (;;) {
		for (u32 i = 0; i < Cl->n; i++)
			ev[i] = vanity_step(&Rt[i], &Cl->D[i], Ls);

		clWaitForEvents(Cl->n, ev);
		for (u32 i = 0; i < Cl->n; i++)
			if (Rt[i].f) {
				*R = Rt[i];
				VANITY_LOG("\n\n"); return;
			}

		m++, dt = clock() - ts;
		if (dt > CLOCKS_PER_SEC) {
			u64 k = th * (u64)m;
			u64 v = k * CLOCKS_PER_SEC / dt;
			n += k, ts += dt, m = 0;

			double p, pn, et;
			p = 1 - pow(p0, n);
			pn = 1 - pow(2, -ceil(n / ph));
			et = (ph - fmod(n, ph)) / v;

			VANITY_LOG(
				"\r[%"PRIu64" key/s] P=%.2f%%, "
				"%.2f%% in %.2fs\33[K",
				v, 100*p, 100*pn, et);
		}
	}
}

/////////////////////////////////////////////////

int main(int argc, char** argv) {
	char op, *pr = "", *su = "";
	vanity_mode mo = VANITY_HELP;

	while ((op = getopt(argc, argv, "m:p:s:")) != -1) {
		switch (op) {
			case 'm':
				mo = vanity_get_mode(optarg); break;

			case 'p': pr = optarg; break;
			case 's': su = optarg; break;
		}
	}

	if (mo == VANITY_HELP) {
		vanity_help(); return 0;
	}

	secp_lut_mul Lm;
	u32 ed25519 = mo == VANITY_SOL;

	cl2_ctx* Cl = cl2_open();
	u64 di = vanity_config(Cl, mo, pr, su, &Lm);

	VANITY_LOG(
		"vanitygen2 in %s mode, diff=%"PRIu64"\n",
		VANITY_MODE[mo], di);

	char ke[32] = "vanity_";
	strcat(ke, VANITY_MODE[mo]);
	cl2_build(Cl, ke);

	vanity_res R;
	vanity_run(&R, Cl, di, ed25519 ? NULL : &Lm);

	printf(" key="); bn_print(&R.s);

	printf("addr=");
	switch (mo) {
		case VANITY_ETH:
			u8_print(R.k.u8, 20); break;

		case VANITY_BTC_BASE58:
			bn_print_base58(&R.k.bn, 7); break;

		case VANITY_BTC_BECH32:
			printf("bc1q");
			u8_print_bech32(R.k.u8, 38); break;

		case VANITY_SOL:
			bn_print_base58(&R.k.bn, 0); break;

		default: return -1;
	}

	return 0;
}

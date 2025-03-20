#include "core/core.h"
#include "vanity.h"

#include <unistd.h>
#include <math.h>
#include <time.h>

typedef enum {
	VANITY_ETH, VANITY_BTC_BASE58,
	VANITY_BTC_BECH32, VANITY_SOL
} vanity_mode;

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
	vanity_res* R, cl2_ctx* V, u64 di,
	const secp_lut_mul* Ls
) {
	vanity_res Rt[V->n]; cl_event ev[V->n];
	VANITY_LOG("[??? key/s] just started...");

	u64 th = 0, n = 0, ts = clock();
	double p0 = 1 - 1./di, ph = log(0.5) / log(p0);

	for (u32 i = 0; i < V->n; i++) {
		cl2_dev* D = &V->D[i];
		th += D->cu * D->wg * BATCH;
	}

	u32 m = 0, dt;
	for (;;) {
		for (u32 i = 0; i < V->n; i++)
			ev[i] = vanity_step(&Rt[i], &V->D[i], Ls);

		clWaitForEvents(V->n, ev);
		for (u32 i = 0; i < V->n; i++)
			if (Rt[i].f) {
				*R = Rt[i];
				VANITY_LOG("\n"); return;
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

int main(int argc, char** argv) {}

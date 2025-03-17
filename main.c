#include "core/core.h"
#include "vanity.h"
#include <CL/cl.h>

#include <immintrin.h>
#include <time.h>

void bn_print(bn x) {
	for (u32 i = 7; i+1 > 0; i--)
		printf("%08x", x->d[i]);
	printf("\n");
}


/*
void vanity_run(vanity_res* R, vanity_dev* D) {
	vanity_rng rng;
	vanity_seed(&rng);

	const u64 wrk_g[3] = { D->wg, D->cu, 8 };
	const u64 wrk_l[3] = { 256, 1, 1 };

	bn_mut S = {}; cl_event ev1, ev2;
	long st = clock(); long el = 0;
	do {:
		vanity_random(&rng, (void*)&S, sizeof(bn_mut));
		S.d[0] |= 1; // `gcd(S, 2^256) == 1`

		CL_ASSERT(clEnqueueWriteBuffer(
			D->q, D->buf.S, 0,
			0, sizeof(bn_mut),
			&S, 0, NULL, &ev1));

		CL_ASSERT(clEnqueueNDRangeKernel(
			D->q, D->ke, 3,
			NULL, wrk_g, wrk_l,
			1, &ev1, &ev2));

		CL_ASSERT(clEnqueueReadBuffer(
			D->q, D->buf.R, 0,
			0, sizeof(vanity_res), R,
			1, &ev2, NULL));

		CL_ASSERT(clFinish(D->q));
		el += clock() - st;
		st = clock();
		printf("%lu ticks\n", el);
	} while (!R->f);

	printf("gpu:\n");
	for (u32 i = 0; i < 32; i++)
		printf("%02x", U8(&R->s)[i]);
	printf("\n");
	bn_print(&R->k);
}
*/
int main(int argc, char** argv) {
	/* vanity_tmp_init();

	ed_lut* L = vanity_ed_lut();
	vanity_filt F = {
		{  0, 0, 0, 0, 0, 0, 0, 0x12345678 },
		{ -1,-1,-1,-1,-1,-1,-1, 0x12345678 } };

	vanity_res R;

	vanity_ctx* V = vanity_open();
	vanity_build(V, "vanity_solana");

	vanity_config(V, &F, L, sizeof(ed_lut)), free(L);
	vanity_run(&R, &V->D[0]);

	vanity_close(V); */
	return 0;
}

#include "core/core.h"
#include "vanity.h"
#include <CL/cl.h>
#include <time.h>

void bn_print(bn x) {
	for (u32 i = 7; i+1 > 0; i--)
		printf("%08x", x->d[i]);

	printf("\n");
}

void bn_rand(bn_mut* R) {
	do {
		for (u32 i = 0; i < 8; i++)
			_rdrand32_step(&R->d[i]);
	} while (bn_cmp(R, &BN_0) == EQ);
}

cl_event vanity_run(
	vanity_res* R, vanity_dev* D,
	const secp_lut_mul* L
) {
	vanity_xy S; cl_event ev;
	bn_rand(&S.x), secp_mul(&S.p, &S.x, L);

	ev = vanity_write(&D->S, D, &S, NULL);
	ev = vanity_dispatch2(
		D, D->cu, D->wg, 1, D->wg, ev);
	return vanity_read(R, D, &D->R, ev);
}

int main(int argc, char** argv) {
	secp_lut_mul Lm;
	secp_lut* L = vanity_secp_lut(&Lm);

	u8_pat P[20] = {
		{ 0xaa, 0xff }, { 0xaa, 0xff },
		{ 0xaa, 0xff }, { 0xaa, 0xff }, };

	vanity_ctx* V = vanity_open();
	vanity_alloc(&V->F, V, VANITY_IN, 20 * sizeof(u8_pat));
	vanity_alloc(&V->L, V, VANITY_IN, sizeof(secp_lut));

	vanity_write(&V->F, &V->D[0], P, NULL);
	vanity_write(&V->L, &V->D[0], L, NULL), free(L);

	for (u32 i = 0; i < V->n; i++) {
		vanity_dev* D = &V->D[i];

		vanity_alloc(
			&D->R, V, VANITY_OUT,
			sizeof(vanity_res));

		vanity_alloc(
			&D->S, V, VANITY_IN,
			sizeof(vanity_xy));
	}

	vanity_build(V, "vanity_eth");
	CL_ASSERT(clFinish(V->D[0].q));

	vanity_res R[V->n];
	cl_event ev[V->n];
	u32 n; u64 ts = clock();

	u32 sp = 0, m = 0;
	for (u32 i = 0; i < V->n; i++) {
		vanity_dev* D = &V->D[i];
		sp += D->cu * D->wg;
	}

	sp *= 256;
	for (;;) {
		for (u32 i = 0; i < V->n; i++)
			ev[i] = vanity_run(&R[i], &V->D[i], &Lm);

		clWaitForEvents(V->n, ev);
		for (u32 i = 0; i < V->n; i++)
			if (R[i].f) { n = i; goto exit; }

		m++;

		u32 dt = clock() - ts;
		if (dt > 1000) {
			ts += dt;
			VANITY_LOG("%u pt/s o_0\n", sp * m);
			m = 0;
		}
	}

exit:
	VANITY_LOG("search done!\n");
	printf("seed=0x"); bn_print(&R[n].s);

	printf("key=0x");
	for (u32 i = 0; i < 32; i++)
		printf("%02x", R[n].k[i]);
	printf("\n");

	return 0;
}

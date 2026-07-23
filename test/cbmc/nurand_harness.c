/*
 * CBMC harness for NURand (TPC-C non-uniform random distribution).
 *
 * Verifies that NURand(A, x, y, seed, C) always returns a value in [x, y],
 * regardless of seed or C value. This is the core TPC-C invariant: the
 * skewed random distribution must never produce out-of-range IDs.
 *
 * Build: cbmc --unwind 20 nurand_harness.c
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

extern uint32_t nurand(uint32_t A, uint32_t x, uint32_t y, unsigned int *seed, uint32_t C);

void verify_nurand_range(void)
{
    uint32_t A, x, y, C;
    unsigned int seed;

    /* CBMC nondeterministic inputs */
    A = nondet_uint();
    x = nondet_uint();
    y = nondet_uint();
    C = nondet_uint();
    seed = nondet_uint();

    /* Assume valid TPC-C ranges */
    __CPROVER_assume(x >= 1);
    __CPROVER_assume(y >= x);
    __CPROVER_assume(y - x < 100000);

    uint32_t result = nurand(A, x, y, &seed, C);

    /* Invariant: result must be in [x, y] */
    __CPROVER_assert(result >= x && result <= y,
                     "NURand result must be in [x, y]");
}

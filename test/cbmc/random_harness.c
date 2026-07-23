/*
 * CBMC harness for get_random_number utility.
 *
 * Verifies the random number helper always returns a value in [0, max].
 *
 * Build: cbmc --unwind 10 random_harness.c
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

extern uint32_t get_random_number(uint32_t max, unsigned int *seed);

void verify_random_range(void)
{
    uint32_t max;
    unsigned int seed;

    max = nondet_uint();
    seed = nondet_uint();

    __CPROVER_assume(max < 100000);

    uint32_t result = get_random_number(max, &seed);

    __CPROVER_assert(result <= max,
                     "get_random_number result must be in [0, max]");
}

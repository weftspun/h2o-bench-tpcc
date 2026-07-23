# CBMC verification harnesses

[CBMC](https://github.com/diffblue/cbmc) (C Bounded Model Checker) is used to
verify C-level invariants that can be checked without a running database or
HTTP server.

## Harnesses

| Harness | What it verifies |
|---------|-----------------|
| `nurand_harness.c` | NURand always returns a value in [x, y] |
| `random_harness.c` | get_random_number always returns [0, max] |

## Running

```bash
cbmc --unwind 20 test/cbmc/nurand_harness.c
cbmc --unwind 10 test/cbmc/random_harness.c
```

CBMC is bounded model checking — it explores all paths up to the unwind bound.
These harnesses use nondeterministic inputs for all parameters, so CBMC
exhaustively checks the invariant across the entire input space.

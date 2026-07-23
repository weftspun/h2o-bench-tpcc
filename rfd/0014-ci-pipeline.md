# RFD 14: CI/CD pipeline design

**State:** accepted

## Decision

GitHub Actions workflow that builds the project and checks the Lean 4
verification harness on every push and pull request.

## Pipeline stages

### 1. Build (ubuntu-24.04)

- Install build deps: cmake, gcc, libssl-dev, libyajl-dev, libz-dev
- Install FDB 7.3.79 C client from GitHub releases
- Clone and build H2O from source (no mruby)
- Build h2o-bench-tpcc with cmake
- Verify the binary exists

### 2. Lean verification (ubuntu-24.04, best-effort)

- Install Lean 4 v4.21.0 via elan
- `lake build` in test/verification/
- Best-effort: plausible-witness-dag dependency may fail if network
   access is restricted in CI

## What CI does NOT do

- **Run benchmarks.** No FDB server in CI. Benchmark runs require a
   dedicated machine with FDB deployed.
- **Run CBMC.** CBMC is not installed in CI yet. The harnesses are
   committed but not executed automatically. Future work: add a
   `verify-cbmc` job that installs CBMC and runs the harnesses.
- **Deploy.** No CD stage. Deployment is manual via Docker.

## Future improvements

- Matrix build: GCC + Clang
- CBMC verification job
- Docker image build and push to GitHub Container Registry
- Integration test: start FDB in a container, load data, run smoke test

import Lake
open Lake DSL

package h2o_bench_tpcc_verification

require plausible from git
  "https://github.com/leanprover-community/plausible.git" @ "main"

require PlausibleWitnessDag from git
  "https://github.com/fire/plausible-witness-dag.git" @ "main"

lean_lib TpccVerification where
  roots := #[TpccVerification]

lean_exe tpcc_verify where
  root := `Main

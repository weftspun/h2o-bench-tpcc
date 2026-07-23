import TpccVerification.Basic

def main (_args : List String) : IO Unit := do
  IO.println "h2o-bench-tpcc verification harness (plausible-witness-dag)"
  IO.println "Target: http://localhost:8080"
  IO.println ""
  TpccVerification.runVerification

#!/usr/bin/env bash
# Reproducible benchmark harness backing report Sec. 6.6.
#
# Usage: from the MiniCompiler/ directory, after `make`:
#   testcases/bench/bench.sh
#
# Runs two independent experiments against the actual built `minicompiler`
# binary and prints one CSV-ish line per data point (also see
# testcases/bench/results.txt for the last captured run).
#
#   A. gen_chain.py N  - one deeply *nested* expression (N terms), used to
#      test the Sec. 5.5 MAX_EXPR_DEPTH robustness guard: N<=3000 should
#      compile in near-constant time; N>3000 should be rejected cleanly
#      and near-instantly, never crash or hang.
#   B. gen_wide.py N   - N separate shallow statements, used to test the
#      Sec. 6.4/6.5 TAC-optimizer's O(m) claim without tripping the
#      depth guard (each statement's own expression is O(1) deep).

set -uo pipefail
# Deliberately no -e: minicompiler is *expected* to exit non-zero for the
# depth-guard-rejected data points, and that must not abort this script.
cd "$(dirname "$0")/../.."   # MiniCompiler/

if [ ! -x ./minicompiler ]; then
    echo "minicompiler not built - run 'make' first" >&2
    exit 1
fi

echo "=== Experiment A: gen_chain.py (depth-guard behavior, Sec. 5.5) ==="
for n in 500 1000 1500 2000 2500 3000 3001 4000 8000 32000 200000; do
    python3 testcases/bench/gen_chain.py "$n" > /tmp/mc_chain_$n.ml
    t0=$(date +%s.%N)
    ./minicompiler /tmp/mc_chain_$n.ml > /tmp/mc_chain_$n.out 2>&1
    ec=$?
    t1=$(date +%s.%N)
    dt=$(echo "$t1 - $t0" | bc)
    result=$(tail -1 /tmp/mc_chain_$n.out)
    printf "chain N=%-7d exit=%d time=%ss  -- %s\n" "$n" "$ec" "$dt" "$result"
    rm -f /tmp/mc_chain_$n.ml /tmp/mc_chain_$n.out
done

echo
echo "=== Experiment B: gen_wide.py (optimizer O(m) scaling, Sec. 6.4/6.5) ==="
for n in 2000 4000 8000 16000 32000 50000 100000 200000; do
    python3 testcases/bench/gen_wide.py "$n" > /tmp/mc_wide_$n.ml
    t0=$(date +%s.%N)
    ./minicompiler /tmp/mc_wide_$n.ml > /tmp/mc_wide_$n.out 2>&1
    ec=$?
    t1=$(date +%s.%N)
    dt=$(echo "$t1 - $t0" | bc)
    result=$(tail -1 /tmp/mc_wide_$n.out)
    printf "wide  N=%-7d exit=%d time=%ss  -- %s\n" "$n" "$ec" "$dt" "$result"
    rm -f /tmp/mc_wide_$n.ml /tmp/mc_wide_$n.out
done

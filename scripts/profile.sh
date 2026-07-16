#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build-prof"
profile_output="${repo_root}/results/profile_baseline.txt"
target="${1:-ring_benchmark}"

case "${target}" in
  ring_benchmark)
    binary="ring_benchmark"
    ;;
  afpacket_vs_dpdk_benchmark)
    binary="afpacket_vs_dpdk_benchmark"
    ;;
  *)
    echo "usage: $0 [ring_benchmark|afpacket_vs_dpdk_benchmark]" >&2
    exit 2
    ;;
esac

cmake -S "${repo_root}" -B "${build_dir}" \
  -G Ninja \
  -DTICKFORGE_WITH_DPDK=ON \
  -DCMAKE_BUILD_TYPE=RelWithProf \
  -DCMAKE_CXX_FLAGS_RELWITHPROF="-O3 -g -fno-omit-frame-pointer -march=native -fno-exceptions"

cmake --build "${build_dir}" --target "${binary}"

if ! command -v perf >/dev/null 2>&1; then
  cat > "${profile_output}" <<'EOF'
perf stat baseline unavailable in this environment.

Expected command:
  perf stat -e cache-references,cache-misses,branch-misses,instructions,cycles \
    --repeat 3 ./build-prof/ring_benchmark

Once perf is available, replace this file with the captured baseline output.
EOF
  echo "perf is not available; wrote ${profile_output}" >&2
  exit 0
fi

perf stat \
  -e cache-references,cache-misses,branch-misses,instructions,cycles \
  --repeat 3 \
  "${build_dir}/${binary}" \
  2>&1 | tee "${profile_output}"

if command -v perf >/dev/null 2>&1; then
  perf record -g --output="${build_dir}/profile.data" "${build_dir}/${binary}" >/dev/null 2>&1 || true
  perf report --stdio --input="${build_dir}/profile.data" | head -n 120
fi

#!/usr/bin/env bash
#
# G3.3 / G3.4 output-diff harness (spdlog-migration/F4_GUARDRAILS_PLAN.md).
#
# The only guard rail that catches a silently vanished log level
# (PATTERN_CATALOG.md §4.2): builds/checks/reports normalized before/after
# test output, per test, using the manifest gen-targets.sh produces.
#
# Usage:
#   outdiff.sh baseline <test>       capture+normalize+store as the baseline
#                                    for <test>. Refuses to overwrite an
#                                    existing baseline without --force
#                                    (F4_GUARDRAILS_PLAN.md finding 6: a
#                                    baseline is unrecoverable once the
#                                    corresponding file has been migrated).
#   outdiff.sh baseline --all-mf     baseline every test with has_mf_sites=1
#                                    in targets.tsv (regenerate that manifest
#                                    first with gen-targets.sh if it's stale)
#   outdiff.sh baseline --force <test|--all-mf>
#                                    same, allowing overwrite (use only when
#                                    you deliberately mean to re-baseline,
#                                    e.g. after a confirmed intentional output
#                                    change)
#   outdiff.sh check <test>          re-run, normalize, diff against the
#                                    stored baseline; exit non-zero + print a
#                                    unified diff on any difference
#   outdiff.sh check --all-mf        run `check` for every baselined test,
#                                    summarizing pass/fail per test
#   outdiff.sh levels <test>         GATE B step 2 support: count records by
#                                    mf severity level in baseline vs current,
#                                    fail if any level's count drops to zero
#                                    when it was non-zero before. See the
#                                    LIMITATION note below.
#   outdiff.sh report                summarize every test with a stored
#                                    baseline: matched / differing / missing
#
# Parallelism: `baseline --all-mf`, `check --all-mf`, and `report` each run
# many independent `ctest -V` invocations (one per test), which is the
# dominant cost and has no correctness reason to be serial -- each test is
# already isolated by ctest's own `-R "^<test>$"` selection. These three run
# their per-test work via `xargs -P` instead of a serial loop. Default
# parallelism is 4 concurrent tests; override with `OUTDIFF_JOBS=<N>` (a
# conservative default, not `nproc`, since this may run on a shared machine
# alongside other users' builds). The environment is activated exactly once,
# in this top-level process, BEFORE any parallel fan-out -- see
# activate_env()'s comment for why concurrent activation is avoided.
#
# LIMITATION (documented, not silently papered over): the resolved output
# pattern (OPEN_QUESTION_ANSWERS.md §1: bare `%v`, chosen specifically to ease
# before/after body comparison) means POST-migration spdlog output carries NO
# level indicator in the text at all -- unlike messagefacility's `%MSG-e/-w/-i`
# banners, which `levels` parses for the PRE-migration side. Once a test's
# code is fully migrated to LAR_LOG_*, `levels` cannot recover a per-level
# count from that test's own captured text, and says so explicitly rather
# than fabricating a number. In that situation the authoritative vanished-
# message detector is `check` (the full normalized body diff): a message that
# silently stopped being emitted shows up there as a missing line regardless
# of which level produced it. Run `check` first; treat `levels` as
# additional confirmation while a test is still on the messagefacility side
# of a partial migration, or as a way to see the pre-migration level
# breakdown baked into a stored baseline at any time.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"      # .../larcorealg/spdlog-migration/guardrails
MIGRATION_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"                    # .../larcorealg/spdlog-migration
LARCOREALG_DIR="$(cd "$MIGRATION_DIR/.." && pwd)"                # .../larcorealg
SRCS_DIR="$(cd "$LARCOREALG_DIR/.." && pwd)"                     # .../mpddev/srcs
MPDDEV_DIR="$(cd "$SRCS_DIR/.." && pwd)"                         # .../mpddev

BUILD_DIR="${BUILD_DIR:-$MPDDEV_DIR/build}"
LOCAL_DIR="${LOCAL_DIR:-$MPDDEV_DIR/local}"
ENV_SH="$SCRIPT_DIR/env.sh"
NORMALIZE_SED="$SCRIPT_DIR/normalize.sed"
TARGETS_TSV="$SCRIPT_DIR/targets.tsv"
BASELINE_DIR="$SCRIPT_DIR/baselines"

# Conservative default (not `nproc`): this may run on a shared build machine
# alongside other users' work. Override with OUTDIFF_JOBS=<N>.
OUTDIFF_JOBS="${OUTDIFF_JOBS:-4}"

mkdir -p "$BASELINE_DIR"

usage() { sed -n '2,45p' "${BASH_SOURCE[0]}"; }

# --- environment plumbing ----------------------------------------------------
# Activate once per PROCESS, gated on an EXPORTED environment variable rather
# than a plain shell variable: `baseline --all-mf` / `check --all-mf` /
# `report` fan out to worker subprocesses (see "Parallelism" above), each a
# fresh invocation of this same script file, so a plain shell variable would
# not survive across that fork/exec boundary and each worker would re-run
# `spack env activate` concurrently -- a real hazard, since `spack mpd
# select`/`env activate` reads and writes shared per-project state, and
# concurrent activation was exactly the kind of environment fragility
# BUILDING_WITH_SPACK_MPD.md and F4_GUARDRAILS_PLAN.md's G0 finding warn
# about. Activating once in the top-level process and exporting the real PATH
# / LD_LIBRARY_PATH / etc. environment variables it produces means every
# worker subprocess inherits a fully-activated environment for free and this
# sentinel lets each one detect that and skip re-activating.
activate_env() {
  [[ "${OUTDIFF_ENV_READY:-0}" == "1" ]] && return 0
  # shellcheck disable=SC1090
  source "$ENV_SH" >/dev/null 2>&1
  spack env activate "$LOCAL_DIR" >/dev/null 2>&1
  export OUTDIFF_ENV_READY=1
}

run_test_raw() {
  # Runs the named test via ctest -V and prints the per-line-prefixed raw
  # transcript (stdout+stderr merged, as ctest -V captures it) to stdout.
  # Uses ctest itself (not a hand-built command line) so the exact same
  # environment variables / working directory ctest normally provides are
  # honored -- see BUILDING_WITH_SPACK_MPD.md on ctest being the actual test
  # runner underneath `spack mpd test`.
  local test="$1"
  activate_env
  ctest --test-dir "$BUILD_DIR" -R "^${test}\$" -V 2>&1
}

extract_body() {
  # Strip ctest's own "<N>: " per-line test-id prefix, leaving only what the
  # test process itself printed. The prefix number is test-run-specific, so
  # it is matched generically (a leading run of digits then ": ") rather than
  # hardcoded, and only lines carrying that prefix are kept -- this also
  # drops ctest's own before/after summary lines (blank last-column padding,
  # "Start N: ...", pass/fail banner, timing summary), which are ctest
  # process noise, not test output.
  grep -E '^[0-9]+: ' | sed -E 's/^[0-9]+: //'
}

capture_and_normalize() {
  # Prints the fully normalized body for a test run to stdout. Also, as a
  # side effect, writes the extracted-but-not-yet-normalized body to
  # $2 (if given) so callers needing the pre-normalization text (e.g. the
  # `levels` command, which needs the %MSG banners normalize.sed rule 1 would
  # otherwise delete) can inspect it.
  local test="$1"
  local raw_out="${2:-}"
  local raw
  raw="$(run_test_raw "$test" | extract_body)"
  if [[ -n "$raw_out" ]]; then
    printf '%s\n' "$raw" > "$raw_out"
  fi
  printf '%s\n' "$raw" | sed -E -f "$NORMALIZE_SED"
}

extract_prefix_sidecar() {
  # Captures every stripped spdlog scoped-name prefix (normalize.sed rule 2)
  # to a sidecar, per PATTERN_CATALOG.md §2.4: "a WRONG prefix is a real
  # defect worth seeing." Uses the SAME exclusion list as normalize.sed rule 2
  # so it stays in lockstep with what that rule actually strips.
  local raw_file="$1"
  grep -oE '^[A-Za-z_][A-Za-z0-9_:<>~,() ]*: ' "$raw_file" 2>/dev/null \
    | grep -vE '^(Info in <|Environment variable|Working Directory:|Test timeout computed to be:|Test selection:|Geometry file:|Running on detector:|Comparing two wires|Returned wire would be:)' \
    | sort | uniq -c | sort -rn
}

baseline_path() { echo "$BASELINE_DIR/$1.txt"; }
raw_path()      { echo "$BASELINE_DIR/$1.raw.txt"; }
prefix_path()   { echo "$BASELINE_DIR/$1.prefixes.txt"; }

all_mf_tests() {
  [[ -f "$TARGETS_TSV" ]] || { echo "outdiff: $TARGETS_TSV not found -- run gen-targets.sh first" >&2; exit 2; }
  awk -F'\t' 'NR>1 && $5=="1" {print $1}' "$TARGETS_TSV"
}

do_baseline_one() {
  local test="$1"
  local force="$2"
  local bpath rpath ppath
  bpath="$(baseline_path "$test")"
  rpath="$(raw_path "$test")"
  ppath="$(prefix_path "$test")"

  if [[ -e "$bpath" && "$force" -ne 1 ]]; then
    echo "outdiff baseline: SKIP $test -- baseline already exists (use --force to overwrite)"
    return 0
  fi

  echo "outdiff baseline: capturing $test ..."
  local norm
  norm="$(capture_and_normalize "$test" "$rpath")"
  printf '%s\n' "$norm" > "$bpath"
  extract_prefix_sidecar "$rpath" > "$ppath"
  echo "outdiff baseline: stored $bpath ($(wc -l < "$bpath") lines)"
}

do_check_one() {
  local test="$1"
  local bpath rpath
  bpath="$(baseline_path "$test")"
  rpath="$(mktemp)"

  if [[ ! -e "$bpath" ]]; then
    echo "outdiff check: $test -- NO BASELINE (run: outdiff.sh baseline $test)"
    rm -f "$rpath"
    return 2
  fi

  local norm
  norm="$(capture_and_normalize "$test" "$rpath")"
  local cur
  cur="$(mktemp)"
  printf '%s\n' "$norm" > "$cur"

  local difftmp
  difftmp="$(mktemp)"
  if diff -u "$bpath" "$cur" > "$difftmp"; then
    echo "outdiff check: $test -- MATCH"
    rm -f "$cur" "$rpath" "$difftmp"
    return 0
  else
    echo "outdiff check: $test -- DIFFERS"
    cat "$difftmp"
    rm -f "$cur" "$rpath" "$difftmp"
    return 1
  fi
}

do_levels_one() {
  local test="$1"
  local braw="$(raw_path "$test")"
  if [[ ! -e "$braw" ]]; then
    echo "outdiff levels: $test -- NO BASELINE RAW CAPTURE (run: outdiff.sh baseline $test)"
    return 2
  fi

  echo "outdiff levels: $test"
  echo "  -- baseline level counts (from %MSG-<x> banners) --"
  local base_counts
  base_counts="$(grep -oE '^%MSG-[a-zA-Z] ' "$braw" | sort | uniq -c || true)"
  if [[ -z "$base_counts" ]]; then
    echo "     (no %MSG banners in baseline -- was this test already migrated when baselined?)"
  else
    echo "$base_counts" | sed 's/^/     /'
  fi

  local craw
  craw="$(mktemp)"
  run_test_raw "$test" | extract_body > "$craw"
  echo "  -- current level counts (from %MSG-<x> banners) --"
  local cur_counts
  cur_counts="$(grep -oE '^%MSG-[a-zA-Z] ' "$craw" | sort | uniq -c || true)"
  if [[ -z "$cur_counts" ]]; then
    echo "     none found."
    echo "     LIMITATION: under the resolved bare-%v pattern (OPEN_QUESTION_ANSWERS.md"
    echo "     §1), fully-migrated spdlog output carries no level indicator in the"
    echo "     text, so a per-level count cannot be recovered here. This is EXPECTED"
    echo "     once this test's code is migrated, not a failure of this check."
    echo "     Use 'outdiff.sh check $test' (the full body diff) as the authoritative"
    echo "     detector for a vanished message at any level: a message that stopped"
    echo "     being emitted shows up there as a missing line regardless of level."
    rm -f "$craw"
    return 0
  fi
  echo "$cur_counts" | sed 's/^/     /'

  # Compare: any baseline level with count>0 that is now 0 or absent is a FAIL.
  local fail=0
  while read -r cnt lvl; do
    [[ -z "$lvl" ]] && continue
    local cur_cnt
    cur_cnt="$(echo "$cur_counts" | awk -v l="$lvl" '$2==l {print $1}')"
    cur_cnt="${cur_cnt:-0}"
    if [[ "$cnt" -gt 0 && "$cur_cnt" -eq 0 ]]; then
      echo "  FAIL: level '$lvl' had $cnt record(s) in baseline, 0 now."
      fail=1
    fi
  done <<< "$base_counts"

  rm -f "$craw"
  if (( fail )); then
    echo "outdiff levels: $test -- FAIL (a level's records vanished)"
    return 1
  else
    echo "outdiff levels: $test -- PASS"
    return 0
  fi
}

list_baselined_tests() {
  local bpath test
  for bpath in "$BASELINE_DIR"/*.txt; do
    [[ -e "$bpath" ]] || continue
    [[ "$bpath" == *.raw.txt || "$bpath" == *.prefixes.txt ]] && continue
    test="$(basename "$bpath" .txt)"
    echo "$test"
  done
}

# --- parallel dispatch --------------------------------------------------
#
# `baseline --all-mf`, `check --all-mf`, and `report` all fan out per-test
# work via `xargs -P` (see "Parallelism" in the header comment). Each worker
# is a fresh re-exec of THIS SCRIPT (argv, no shell string interpolation --
# test names go through as plain argv arguments, never substituted into a
# `bash -c` string), so there is no in-process function to share; instead
# each worker writes its full stdout+stderr and exit code to files in a
# per-run scratch directory, and the parent reads those back AFTER `xargs`
# has waited for every worker to finish, printing them in stable, original
# test-list order. This keeps concurrent ctest runs from interleaving their
# output (xargs -P gives no ordering or atomicity guarantee across children's
# stdout) while still parallelizing the actual expensive work.
safe_name() {
  # Defensive filename sanitization for the scratch-file path -- ctest test
  # names observed in this tree are plain identifiers (letters, digits, `_`),
  # but this guards against a future test name containing a path separator
  # from escaping the scratch directory.
  printf '%s' "$1" | tr -c 'A-Za-z0-9_.-' '_'
}

make_scratch_dir() {
  mktemp -d
}

run_parallel_worker_subcommand() {
  # $1 = worker subcommand (e.g. __worker-check), remaining args appended
  # after each test name read from stdin (one test per line).
  local worker="$1"
  shift
  xargs -P "$OUTDIFF_JOBS" -I{} "$SCRIPT_DIR/outdiff.sh" "$worker" "$@" {}
}

do_report() {
  activate_env
  local scratch
  scratch="$(make_scratch_dir)"
  trap 'rm -rf "$scratch"' RETURN

  mapfile -t tests < <(list_baselined_tests)
  if [[ ${#tests[@]} -eq 0 ]]; then
    echo "outdiff report: matched=0 differing=0 missing=0 (no baselines found)"
    return 0
  fi

  printf '%s\n' "${tests[@]}" \
    | OUTDIFF_SCRATCH="$scratch" run_parallel_worker_subcommand __worker-check-tofile

  local matched=0 differing=0 missing=0
  local t safe rc
  for t in "${tests[@]}"; do
    safe="$(safe_name "$t")"
    rc="$(cat "$scratch/$safe.rc" 2>/dev/null || echo 3)"
    case "$rc" in
      0) matched=$((matched+1)) ;;
      2) missing=$((missing+1)) ;;
      *) differing=$((differing+1)) ;;
    esac
  done
  echo "outdiff report: matched=$matched differing=$differing missing=$missing"
}

# --- parallel "--all-mf" drivers for baseline / check --------------------
#
# Same scratch-directory pattern as do_report: fan out via xargs -P, have
# each worker capture its own output+exit code to a file, then the parent
# replays them in the ORIGINAL test-list order once every worker has
# finished. This avoids interleaved output from concurrent `ctest -V` runs
# while still doing the actual work in parallel.

do_baseline_all_mf() {
  local force="$1"
  activate_env
  local scratch
  scratch="$(make_scratch_dir)"
  trap 'rm -rf "$scratch"' RETURN

  mapfile -t tests < <(all_mf_tests)
  if [[ ${#tests[@]} -eq 0 ]]; then
    echo "outdiff baseline: no has_mf_sites=1 tests in $TARGETS_TSV"
    return 0
  fi

  printf '%s\n' "${tests[@]}" \
    | OUTDIFF_SCRATCH="$scratch" run_parallel_worker_subcommand __worker-baseline-tofile "$force"

  local t safe
  for t in "${tests[@]}"; do
    safe="$(safe_name "$t")"
    [[ -e "$scratch/$safe.out" ]] && cat "$scratch/$safe.out"
  done
}

do_check_all_mf() {
  activate_env
  local scratch
  scratch="$(make_scratch_dir)"
  trap 'rm -rf "$scratch"' RETURN

  mapfile -t tests < <(all_mf_tests)
  if [[ ${#tests[@]} -eq 0 ]]; then
    echo "outdiff check: no has_mf_sites=1 tests in $TARGETS_TSV"
    return 0
  fi

  printf '%s\n' "${tests[@]}" \
    | OUTDIFF_SCRATCH="$scratch" run_parallel_worker_subcommand __worker-check-tofile

  local fail=0
  local t safe rc
  for t in "${tests[@]}"; do
    safe="$(safe_name "$t")"
    [[ -e "$scratch/$safe.out" ]] && cat "$scratch/$safe.out"
    rc="$(cat "$scratch/$safe.rc" 2>/dev/null || echo 3)"
    [[ "$rc" != "0" ]] && fail=1
  done
  return $fail
}

# --- main ---------------------------------------------------------------

cmd="${1:-}"
[[ -z "$cmd" ]] && { usage; exit 2; }
shift || true

case "$cmd" in
  # Hidden internal worker subcommands, invoked only via xargs re-exec of
  # this same script from run_parallel_worker_subcommand -- not part of the
  # documented CLI. Each writes its captured output/exit-code pair to
  # $OUTDIFF_SCRATCH (inherited from the parent's environment) so the parent
  # can replay results in stable order after all workers finish.
  __worker-baseline-tofile)
    force="$1"; test="$2"
    safe="$(safe_name "$test")"
    out="$(do_baseline_one "$test" "$force" 2>&1)"
    rc=$?
    printf '%s\n' "$out" > "$OUTDIFF_SCRATCH/$safe.out"
    echo "$rc" > "$OUTDIFF_SCRATCH/$safe.rc"
    ;;
  __worker-check-tofile)
    test="$1"
    safe="$(safe_name "$test")"
    out="$(do_check_one "$test" 2>&1)"
    rc=$?
    printf '%s\n' "$out" > "$OUTDIFF_SCRATCH/$safe.out"
    echo "$rc" > "$OUTDIFF_SCRATCH/$safe.rc"
    ;;
  baseline)
    force=0
    if [[ "${1:-}" == "--force" ]]; then force=1; shift; fi
    target="${1:-}"
    [[ -z "$target" ]] && { echo "ERROR: baseline requires <test> or --all-mf" >&2; exit 2; }
    if [[ "$target" == "--all-mf" ]]; then
      do_baseline_all_mf "$force"
    else
      do_baseline_one "$target" "$force"
    fi
    ;;
  check)
    target="${1:-}"
    [[ -z "$target" ]] && { echo "ERROR: check requires <test> or --all-mf" >&2; exit 2; }
    if [[ "$target" == "--all-mf" ]]; then
      do_check_all_mf
      exit $?
    else
      do_check_one "$target"
    fi
    ;;
  levels)
    target="${1:-}"
    [[ -z "$target" ]] && { echo "ERROR: levels requires <test>" >&2; exit 2; }
    do_levels_one "$target"
    ;;
  report)
    do_report
    ;;
  -h|--help)
    usage
    ;;
  *)
    echo "ERROR: unknown command '$cmd'" >&2
    usage
    exit 2
    ;;
esac

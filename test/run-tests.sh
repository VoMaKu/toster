#!/bin/sh
# Runs the judge against the contest this repository ships and against a small
# contest built to exercise everything else, and checks what comes back.
#
#   sh test/run-tests.sh        or        make check
#
# Nothing here writes inside the repository except contest/log, which a run
# rewrites anyway; the small contest is copied somewhere temporary first.

set -u

root=$(cd "$(dirname "$0")/.." && pwd) || exit 1
cd "$root" || exit 1

pass=0
fail=0
skip=0

ok()      { printf '  ok    %s\n' "$1"; pass=$((pass + 1)); }
bad()     { printf '  FAIL  %s\n' "$1"; fail=$((fail + 1)); }
skipped() { printf '  skip  %s — %s\n' "$1" "$2"; skip=$((skip + 1)); }

same() { # same <what> <expected> <actual>
	if [ "$2" = "$3" ]; then
		ok "$1"
	else
		bad "$1"
		printf '        expected: %s\n        got:      %s\n' "$2" "$3"
	fi
}

work=$(mktemp -d) || exit 1
trap 'rm -rf "$work"' EXIT INT TERM

# A copy of the small contest, so a run never writes into the repository.
# "cfg" drops contest.json, leaving the .cfg files to describe it.
fixture() {
	rm -rf "$work/contest"
	cp -R test/contest "$work/contest" || exit 1
	mkdir -p "$work/contest/log" "$work/contest/tmp"
	if [ "${1:-cfg}" = cfg ]; then
		rm -f "$work/contest/contest.json"
	fi
}

row()   { grep "^$2 " "$1"; }
mark()  { row "$1" "$2" | cut -c $((21 + 4 * $3)); }   # mark <log> <student> <problem index>
total() { row "$1" "$2" | awk '{ print $NF }'; }
score() { row "$1" "$2" | awk -v c="$3" '{ print $(c + 2) }'; }   # same problem index as mark()
marks() { cat "$1/log/$2_$3.log"; }                                    # every mark of one submission

printf '\nBuilding\n'
if make clean >/dev/null 2>&1 && make all >"$work/build" 2>&1; then
	same "the build is free of warnings" 0 "$(grep -c warning "$work/build")"
else
	bad "the build succeeds"
	sed 's/^/        /' "$work/build"
	printf '\n%d passed, %d failed\n' "$pass" "$fail"
	exit 1
fi

printf '\nThe contest of 2019\n'
(cd bin && ./judge -j 0) >/dev/null 2>&1
for log in results.log scores.log; do
	if diff -u "test/expected/$log" "contest/log/$log" >"$work/diff" 2>&1; then
		ok "$log is what it was"
	else
		bad "$log is what it was"
		sed 's/^/        /' "$work/diff" | head -20
	fi
done

printf '\nLanguages\n'
fixture cfg
./bin/judge -j 0 "$work/contest" >/dev/null 2>"$work/err"
results="$work/contest/log/results.log"
# Everything below reads that scoreboard. If the judge would not mark the
# contest at all, say so once and stop, rather than failing every check that
# depends on it and leaving the reason in a file nobody printed.
if [ ! -s "$results" ] || [ "$(wc -l < "$results")" -lt 5 ]; then
	bad "the judge marks the small contest"
	sed 's/^/        /' "$work/err" | head -5
	printf '\n%d passed, %d failed, %d skipped\n\n' "$pass" "$fail" "$skip"
	exit 1
fi
same "a submission in C"      + "$(mark "$results" Alice 0)"
same "a submission in C++"    + "$(mark "$results" Bob 0)"
same "a submission in Python" + "$(mark "$results" Carol 0)"
same "a student who submitted nothing" X "$(mark "$results" Carol 1)"

printf '\nLimits\n'
same "a submission that never stops is cut off" x "$(mark "$results" Alice 1)"
same "one that stops in time is not"            + "$(mark "$results" Bob 1)"
if grep -q RLIMIT_AS "$work/err"; then
	skipped "a submission over its memory limit is cut off" "this system will not lower RLIMIT_AS"
else
	same "a submission over its memory limit is cut off" x "$(mark "$results" Alice 2)"
fi

printf '\nCheckers and scores\n'
same "a checker of the problem's own accepts"  + "$(mark "$results" Alice 3)"
same "and rejects"                             - "$(mark "$results" Bob 3)"
scores="$work/contest/log/scores.log"
same "a problem worth 50 pays 50 for both tests" 50 "$(score "$scores" Alice 3)"
same "and 25 for one of the two"                 25 "$(score "$scores" Bob 3)"
same "a total is the sum of them"               325 "$(total "$scores" Bob)"

printf '\nThe sandbox\n'
if [ -x /usr/bin/sandbox-exec ]; then
	# Mallory steals the answer to the first test and prints it for both, so
	# letting it out shows up as that one test turning from - into +.
	same "a submission cannot read the answers" -- "$(marks "$work/contest" Mallory A)"
	fixture cfg
	printf 'sandbox=off\n' > "$work/contest/contest.cfg"
	./bin/judge -j 0 "$work/contest" >/dev/null 2>&1
	same "and reads them once it is let out" +- "$(marks "$work/contest" Mallory A)"
	fixture cfg
	printf 'sandbox=on\n' > "$work/contest/contest.cfg"
	./bin/judge -j 0 "$work/contest" >/dev/null 2>&1
	same "sandbox=on runs where there is one" -- "$(marks "$work/contest" Mallory A)"
else
	skipped "the sandbox" "this system has no sandbox-exec"
fi

printf '\nThe two ways of describing a contest\n'
fixture cfg
./bin/judge -j 0 "$work/contest" >/dev/null 2>&1
cp "$work/contest/log/results.log" "$work/from-cfg"
fixture json
./bin/judge -j 0 "$work/contest" >/dev/null 2>&1
if diff -u "$work/from-cfg" "$work/contest/log/results.log" >"$work/diff" 2>&1; then
	ok "contest.json says the same as the .cfg files"
else
	bad "contest.json says the same as the .cfg files"
	sed 's/^/        /' "$work/diff"
fi

printf '\nRunning several at once\n'
fixture cfg
./bin/judge -j 1 "$work/contest" >/dev/null 2>&1
cp "$work/contest/log/results.log" "$work/one-at-a-time"
fixture cfg
./bin/judge -j 0 "$work/contest" >/dev/null 2>&1
if diff -u "$work/one-at-a-time" "$work/contest/log/results.log" >"$work/diff" 2>&1; then
	ok "-j 0 marks the same as -j 1"
else
	bad "-j 0 marks the same as -j 1"
	sed 's/^/        /' "$work/diff"
fi

printf '\nWhat it says when something is wrong\n'
says() { # says <what> <pattern> <command...>
	what=$1; pattern=$2; shift 2
	if "$@" 2>&1 >/dev/null | grep -q "$pattern"; then ok "$what"; else bad "$what"; fi
}
says "a contest that is not there"        "No such file"          ./bin/judge "$work/nowhere"
says "a problem the contest does not have" "no problem Z"          ./bin/test "$work/contest" Alice Z
says "the runner called with too little"   "usage:"                ./bin/test
says "the judge called with too much"      "usage:"                ./bin/judge -j 1 one two

fixture cfg
printf 'tests=2\nchecker=nonsense\n' > "$work/contest/tests/A/problem.cfg"
says "a checker that is not a program" "no such program" ./bin/judge "$work/contest"

fixture cfg
printf 'users=5\n1=Alice\n2=Bob\n3=Carol\n4=Mallory\n5=Nobody\n' > "$work/contest/code/user.cfg"
./bin/judge -j 0 "$work/contest" >/dev/null 2>&1
same "a student with no directory is a row of X, not a stopped run" \
	XXXX "$(row "$work/contest/log/results.log" Nobody | cut -c 21- | tr -d ' 0-9')"

fixture json
printf '{ "users": ["Alice"], "problems": [ { "letter": "A", tests: 2 } ] }\n' > "$work/contest/contest.json"
says "JSON that does not parse, with the place" "contest.json:1:" ./bin/judge "$work/contest"

printf '\n%d passed, %d failed, %d skipped\n\n' "$pass" "$fail" "$skip"
[ "$fail" -eq 0 ]

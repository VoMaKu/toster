# Tester

A contest judge written in C for a second-year systems programming course. Give
it a directory of submissions and a directory of tests, and it compiles every
submission, runs it against every test under a time limit and a sandbox, and
prints a scoreboard — the way an olympiad judge does, but small enough to read
in one sitting.

```
    users/problems  A   B   C   D   E   F   G   H   I   J     total
Aitassova           +   -   +   -   -   -   -   X   +   +       676
Albek               -   -   -   x   +   -   X   +   +   +       500
Alikhanov           +   -   +   -   -   -   -   +   +   +       776
```

## Quick start

```sh
make all
cd bin
./judge
```

That marks the contest in `contest/`, which is the one this repository ships:
eleven students, ten problems, from a real run in 2019. The scoreboard lands in
`contest/log/results.log`.

To mark a contest of your own, point the judge at it:

```sh
./bin/judge -j 0 path/to/contest
```

## Tests

```sh
make check
```

runs the judge twice over. Once against the contest of 2019, whose scoreboard
is kept in `test/expected/` and has to come back unchanged. Once against a
small contest in `test/contest/` written to exercise the rest: three
languages, a submission that never stops, a checker of a problem's own,
partial credit, a submission that tries to read the answers out of the
contest, `-j 0` against `-j 1`, `contest.json` against the `.cfg` files, and
what the judge says when a contest is malformed.

A check the system cannot support is skipped rather than failed — the memory
limit on macOS, say. Nothing is written inside the repository except
`contest/log`, which a run rewrites anyway.

## How a contest is laid out

```
contest/
  contest.cfg        settings for the contest as a whole (optional)
  lang.cfg           how each kind of submission is built and run
  code/
    user.cfg         who is taking part, and in what order
    Ivanov/A.c       one directory per student, one file per problem
  tests/
    problem.cfg      how many problems there are
    A/
      problem.cfg    how this problem is run and checked
      001.dat        the test
      001.ans        the answer it should produce
  checkers/          checkers of your own, for problems that name one
  log/               everything a run writes
  tmp/               binaries, and a scratch directory per submission
```

Problems are addressed by letter: the first problem is `A`, and a submission to
it is `code/<Student>/A.c`. A student who submitted nothing to a problem simply
has no file there.

`make` creates `log/` and `tmp/`; git ignores both.

## Running

```
judge [-j workers] [contest directory]
```

The contest directory defaults to `../contest`, so `./judge` from `bin/` marks
the contest in this repository. `-j` says how many submissions to run at once
and defaults to one; `-j 0` takes one per processor. The scoreboard does not
depend on the number — jobs finish in whatever order they finish in and are
written out in the order of the configuration.

To mark one submission by hand, without touching the scoreboard:

```
test <contest directory> <student> <problem letter>
```

```sh
$ ./bin/test contest Kozhemyak A
------
```

The judge looks for `test` beside itself, so neither program cares which
directory it was started from.

## What a run writes

`log/results.log` is the scoreboard: one row per student, in the order the
configuration lists them, with a mark per problem and a total.

| Mark | Meaning |
|---|---|
| `+` | every test passed |
| `-` | at least one test produced the wrong output |
| `x` | at least one test crashed, ran out of time, or had no test file to read |
| `X` | nothing was submitted for that problem, or it did not compile |

`x` outranks `-`: a submission that both crashes and answers wrongly is `x`,
because that is the more useful thing to be told.

`log/scores.log` is the same table in points. A problem is worth `points=` — a
hundred unless it says otherwise — shared out over its tests, so a submission
that passes five tests of six is worth 83 and one that passes none is worth 0.

```
    users/problems        A      B      C      D      E      F      G      H      I      J  total
Moldakhmetova            83     20      0     16     80      0      0    100    100    100    499
```

`log/<Student>_<Letter>.log` is one character per test of a single submission,
from the same four marks.

`log/results2.log` is the trace: when each submission started and stopped, its
per-test marks, how many tests it passed, and what it scored.

## Configuration

A contest is described either by the `.cfg` files below or by a single
`contest.json`. When `contest.json` is present it is the one that is read.

### The .cfg files

`code/user.cfg` — who is taking part, and in what order:

```
users=11
1=Aitassova
2=Albek
```

Each name has to be the name of a directory under `code/`. The list also fixes
the order of the scoreboard rows. A name with no directory behind it is not an
error: that row is filled with `X` and the run carries on.

`tests/problem.cfg` — how many problems there are:

```
problems=10
```

`tests/A/problem.cfg` — how one problem is run and checked:

```
tests=6
checker=checker_byte
time=2
memory=256
points=100
```

Only `tests` is required. `checker` defaults to `checker_byte`, `time` to two
seconds, `points` to a hundred, and `memory` — in megabytes — to no limit.

`contest.cfg` — what is true of the whole contest. It is optional, and today it
holds one key:

```
sandbox=auto
```

Keys are read by name, so lines may come in any order, and a `checker=` naming
something unknown stops the run rather than quietly falling back to a built-in
one. A line splits at its first `=`, so a value may hold more of them.

### contest.json

The same contest in one file:

```json
{
  "sandbox": "auto",
  "users": ["Aitassova", "Albek"],
  "languages": {
    "c":   { "compile": "gcc {src} -o {bin} -lm", "run": "{bin}" },
    "cpp": { "compile": "g++ -std=c++17 {src} -o {bin}", "run": "{bin}" },
    "py":  { "run": "python3 {src}" }
  },
  "problems": [
    { "letter": "A", "tests": 6, "checker": "checker_byte" },
    { "letter": "B", "tests": 5, "checker": "checker_int", "time": 1, "points": 200 }
  ]
}
```

`users` and `problems` are required, and inside a problem only `tests` is.
`letter` defaults to the position in the list, so the first problem is `A`. A
syntax error names the line and column it gave up on.

## Languages

`lang.cfg` says how each kind of submission is built and run:

```
c.compile=gcc {src} -o {bin} -lm
c.run={bin}
cpp.compile=g++ -std=c++17 {src} -o {bin}
cpp.run={bin}
py.run=python3 {src}
```

The key before the dot is the file extension. `{src}` is the submission, `{bin}`
the binary built from it. A language with no `.compile` line is run straight
from its source, which is what makes Python work. The judge tries the
extensions in the order they are declared and takes the first that exists, so
`Ivanov/A.py` and `Ivanov/A.c` are both submissions to problem `A`.

Commands are split on spaces and never see a shell, so no single argument may
contain one. Without a `lang.cfg` the judge knows C alone, built with plain
`gcc`.

Only a language that is built can fail to build. A Python file with a syntax
error is `x` on every test rather than `X`, because nothing looks at it until
it runs.

## Checkers

Two comparisons are compiled in, chosen per problem by the `checker=` line:

- `checker_byte` — compares output and answer word by word, so extra spaces and
  line breaks do not matter.
- `checker_int` — compares character by character, skipping spaces. Line breaks
  do matter here, and both sides have to end at the same place: an output that
  is a prefix of the answer is wrong, not right.

Any other name is a path under the contest directory to a program of your own:

```
checker=checkers/close_enough
```

It is run as `close_enough <test> <output> <answer>` and accepts the output by
exiting `0`. Anything it prints goes to stderr, because stdout is where the
marks are carried.

```sh
#!/bin/sh
# accepts any answer within one of the expected one
out=$(cat "$2"); ans=$(cat "$3")
[ "$out" -ge $((ans - 1)) ] && [ "$out" -le $((ans + 1)) ]
```

## Limits

Each test gets the seconds its problem asks for, two by default, as
`RLIMIT_CPU` on the submission itself. The limit is set in the child after the
`fork`, so it measures the submission rather than the work of starting it, and
a wall clock alarm twice as far out catches a submission that blocks instead of
computing. Either way the submission is killed and the test is `x`. Because the
real limit is processor time and not wall clock, a loaded machine — or a run
with `-j 0` — does not turn a correct submission into a timeout.

A problem may also ask for a memory limit in megabytes, applied as
`RLIMIT_AS`. That needs a system that enforces it: macOS gives `RLIMIT_AS` the
number of `RLIMIT_RSS` and refuses to lower it, and the judge says so rather
than pretending. Output is capped at 64 MB per test, core dumps at nothing.

## The sandbox

A submission is a program somebody else wrote, and it runs on the marker's
machine. Each one is confined to a profile that denies everything it is not
given: it cannot read the tests it is being marked against, write anywhere but
its own scratch directory under `tmp/`, reach the network, or fork. That
scratch directory is also its working directory, so a submission that writes a
file relative to itself lands there and nowhere near the contest.

`sandbox=` in `contest.cfg` chooses:

| Value | Meaning |
|---|---|
| `auto` | confine where the system offers it, and say once where it does not (the default) |
| `on` | confine, and refuse to run at all otherwise |
| `off` | do not confine |

The sandbox rests on `sandbox-exec`, so it is macOS today; elsewhere `auto`
runs unconfined and says so. It is worth what it says and no more: it stops a
submission reading the answers or phoning home, not a determined attack on the
kernel underneath it.

## Differences from the 2019 scoreboard

`contest/log-2019/` holds the logs of the 2019 run. Replaying the same contest
today does not reproduce them exactly — ten marks differ, for three reasons.

**Five marks were `-` and are now `x`.** They are the submissions that crash or
run out of time: `Albek/D`, `Selevenko/D`, `Zhanmukanbetova/B`, `/H` and `/I`.
The 2019 scoreboard had no way of saying so and folded them into the mark for a
wrong answer. Their per-test logs said `x` even then.

**Two differ because of how a failed build used to be recorded.** The judge
compiled a submission with plain `gcc` and no `-lm`. On the Linux of 2019 that
failed to link any submission that actually called a libm function, and exactly
two did: `Kozhemyak/E.c` calls `pow`, `Selevenko/E.c` calls `sqrt`. Every other
submission that includes `math.h` either calls nothing from it or only `fabs`,
which the compiler expands inline. The judge did not notice the failure — it
treated only exit code 88, the code its own `execlp` used, as a failure to
build, so an ordinary `gcc` exit of 1 passed for success. `execlp` then could
not find the binary that was never produced, and every test was recorded as
`x`, which read as a wrong answer rather than as a submission that does not
compile. macOS carries libm inside libSystem, so both link, run, and pass — and
the `lang.cfg` here passes `-lm`, so that failure cannot come back.

That path now reports `X`, which moves one further mark: `Albek/G` was `-` and
is now `X`. That submission uses two variables its function never declares and
has never compiled on any compiler.

**Two more differ because the submissions read memory they never wrote.** glibc
happened to leave it zeroed and macOS does not. `Selevenko/A.c` never
terminates its first buffer before handing it to `strcmp`, and `Koshkarov/I.c`
writes its terminator one byte past the end of the string, leaving the last
byte uninitialised.

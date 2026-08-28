# Tester

A contest judge written in C for a second-year systems programming course. It
compiles every student's submission, runs it against the prepared tests and
prints a scoreboard — the way an olympiad judge does, but small enough to read
in one sitting.

## Layout

```
src/       judge.c, test.c, reader_cfg.c — the sources
bin/       the two built programs, and the directory the judge runs from
contest/
  code/    one directory per student: contest/code/Ivanov/A.c
  tests/   one directory per problem: contest/tests/A/001.dat, 001.ans, ...
  log/     everything the run produces
tmp/       submissions compiled into binaries, named Student_Letter
```

## Configuration

Three kinds of `.cfg` file describe a contest. The judge reads them with the
tiny parser in `src/reader_cfg.c`, which understands `key=value` lines.

`contest/code/user.cfg` — who is taking part:

```
users=11
1=Aitassova
2=Albek
...
```

`contest/tests/problem.cfg` — how many problems there are:

```
problems=10
```

`contest/tests/A/problem.cfg` — how the problem is checked:

```
tests=6
checker=checker_byte
```

Problems are addressed by letter: problem number 0 is `A`, and a submission to
it is `contest/code/<Student>/A.c`.

## Building

```sh
make all
```

which is `gcc src/test.c -o bin/test` and `gcc src/judge.c src/reader_cfg.c -o
bin/judge`.

## Running

Every path inside both programs is relative to `bin/`, so start them from
there.

```sh
cd bin
./judge
```

checks every student against every problem and rewrites the logs. To check a
single submission:

```sh
cd bin
./test ../contest/code/Kozhemyak/A.c ../contest/tests/A
```

## What the marks mean

`contest/log/results.log` holds the scoreboard, one row per student:

| Mark | Meaning |
|---|---|
| `+` | every test passed |
| `-` | at least one test produced the wrong output |
| `X` | nothing was submitted for that problem, or it did not compile |

`contest/log/<Student>_<Letter>.log` holds one character per test of a single
submission: `+` passed, `-` wrong output, `x` crashed or ran out of time.

`contest/log/results2.log` is the trace: when each test started and stopped,
the per-test characters, and how many passed and failed.

## Checkers

Two comparisons are available, chosen per problem by the `checker=` line:

- `checker_byte` — compares output and answer word by word, so extra spaces and
  line breaks do not matter.
- `checker_int` — compares character by character, skipping spaces.

## Limits

Each run gets two seconds. `test` arms `alarm(2)` before starting the
submission and kills it with `SIGKILL` when the alarm fires; the test is then
recorded as `x`. The alarm is armed before the `fork`, so those two seconds
cover starting the process as well as running it.

## Differences from the 2019 scoreboard

`contest/log/` still holds the logs of the 2019 run. Replaying the same contest
today does not reproduce them exactly.

Two marks differ because of how a failed build used to be recorded. `test`
compiles a submission with plain `gcc` and no `-lm`. On the Linux of 2019 that
failed to link any submission that actually called a libm function, and exactly
two did: `Kozhemyak/E.c` calls `pow`, `Selevenko/E.c` calls `sqrt`. Every other
submission that includes `math.h` either calls nothing from it or only `fabs`,
which the compiler expands inline. The judge did not notice the failure — it
treated only exit code 88, the code its own `execlp` uses, as a failure to
build, so an ordinary `gcc` exit of 1 passed for success. `execlp` then could
not find the binary that was never produced, and every test was recorded as
`x`, which reads as a wrong answer rather than as a submission that does not
compile. macOS carries libm inside libSystem, so both link, run, and pass.

That path now reports `X`, which moves one further mark: `Albek_G` was `-` and
is now `X`. That submission uses two variables its function never declares and
has never compiled on any compiler.

Two more marks differ because the submissions themselves read memory they never
wrote — glibc happened to leave it zeroed and macOS does not. `Selevenko/A.c`
never terminates its first buffer before handing it to `strcmp`, and
`Koshkarov/I.c` writes its terminator one byte past the end of the string,
leaving the last byte uninitialised.

The scoreboard is also not perfectly repeatable, because the two-second limit
covers starting the process as well as running it. On a loaded machine, or the
first time a freshly compiled binary is executed, a correct submission can be
killed and recorded as `x`. That was seen once, on `Stoletniy_A`, in the first
of five runs.

## Known issues

- Rows in the scoreboard follow the order the filesystem returns directories in,
  not the order in `user.cfg`.

## Known limitations

- Submissions must be C: `test` compiles them with `gcc` and nothing detects
  the language.
- The judge only works when started from `bin/` — every path is relative to it.
- The two-second limit is compiled in, and nothing limits memory.
- `problem.cfg` is parsed positionally: the reader scans to the first `=`, takes
  the number of tests, then scans to the first `_` and looks at one character
  after it. That character alone picks the checker — `i` means `checker_int`,
  anything else falls back to `checker_byte` — so `tests=` must come first and a
  checker named otherwise is silently ignored.
- The pass and fail counters in `results2.log` are single characters, so a
  problem with more than nine tests prints punctuation instead of a count.
- A test that crashes or runs out of time is written to the per-submission log
  twice, so those logs hold two characters for one test. The scoreboard is
  unaffected: it reads the run's output, not the log.
- Only `.cfg` is understood. There is no reader for any other format.

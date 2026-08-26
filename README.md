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
recorded as `x`.

## Known issues

- **`./judge` crashes on the first student.** `usr_name` reads one directory
  entry before its loop and then skips as many entries as the student's index,
  so for index 0 the loop never runs and the function returns that first entry —
  `.` — as a name. The judge then tries to compile `../contest/code/./Aitassova`,
  a directory, and dies with SIGSEGV. The same off-by-one shifts every other
  student by one place and never reaches the last one. `usr_name` also allocates
  `strlen(name)` bytes with no room for the terminator.
- The binaries committed in `bin/` are Linux ELF built in 2019 and cannot run on
  macOS; `make all` rebuilds them for the current machine.
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
- Only `.cfg` is implemented. `judge.c` still has the branches for `user.xml`
  and `user.json`, with the readers commented out.

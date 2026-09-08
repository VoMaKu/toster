#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include "config.h"

/* Runs one submission against one problem and writes a mark per test, both to
   contest/log/<Student>_<Letter>.log and to stdout, which is what the judge
   reads. Nothing else may go to stdout. */

static pid_t child = 0;
static volatile sig_atomic_t killed = 0;

static void handler(int signo) { // the submission has been running for too long
	(void)signo;
	if (child != 0) {
		kill(child, SIGKILL);
		killed = 1;
	}
}

/* Everything the submission is not handed here is denied it: reading the tests
   it is being marked against, writing anywhere but its own scratch directory,
   reaching the network, and forking. */
static int write_profile(const char *path, const char *scratch, const char *tests) {
	if (strpbrk(scratch, "\"\\") != NULL || strpbrk(tests, "\"\\") != NULL) {
		fprintf(stderr, "a sandbox profile cannot carry a path with a quote or a backslash in it\n");
		return -1;
	}
	FILE *file = fopen(path, "w");
	if (file == NULL) {
		perror(path);
		return -1;
	}
	fprintf(file,
		"(version 1)\n"
		"(deny default)\n"
		"(allow process-exec*)\n"
		"(allow signal (target self))\n"
		"(allow sysctl-read)\n"
		"(allow mach-lookup)\n"
		"(allow file-read*)\n"
		"(deny file-read* (subpath \"%s\"))\n"
		"(allow file-write* (subpath \"%s\"))\n",
		tests, scratch);
	return fclose(file) == 0 ? 0 : -1;
}

static char **sandbox_wrap(char **command, const char *profile) {
	int count = 0;
	while (command[count] != NULL) {
		count++;
	}
	char **wrapped = calloc(count + 4, sizeof(char *));
	if (wrapped == NULL) {
		return NULL;
	}
	wrapped[0] = path_fmt("%s", SANDBOX_EXEC);
	wrapped[1] = path_fmt("-f");
	wrapped[2] = path_fmt("%s", profile);
	for (int i = 0; i < count; i++) {
		wrapped[3 + i] = path_fmt("%s", command[i]);
	}
	return wrapped;
}

/* Not every system enforces a limit on the address space: macOS gives
   RLIMIT_AS the number of RLIMIT_RSS and refuses to lower it. Asked once, so
   that a contest that wants a memory limit is told plainly it is not getting
   one. */
static int memory_limit_works(long memory) {
	static int asked = 0, works = 0;
	if (asked) {
		return works;
	}
	asked = 1;
	pid_t pid = fork();
	if (pid < 0) {
		return 0;
	}
	if (pid == 0) {
		struct rlimit space = { memory, memory };
		_exit(setrlimit(RLIMIT_AS, &space) == 0 ? 0 : 1);
	}
	int status;
	waitpid(pid, &status, 0);
	works = WIFEXITED(status) && WEXITSTATUS(status) == 0;
	if (!works) {
		fprintf(stderr, "this system will not lower RLIMIT_AS, so the memory limit is not applied\n");
	}
	return works;
}

/* Runs argv to completion. in and out are redirected onto the child's stdin
   and stdout unless they are -1. seconds and memory are the limits the child
   is given; 0 means none. Returns 0 when the child exited with 0. */
static int spawn(char **argv, int in, int out, const char *cwd, int seconds, long memory) {
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		return -1;
	}
	if (pid == 0) {
		if (cwd != NULL && chdir(cwd) != 0) { // a submission has no business in the directory the judge was started from
			perror(cwd);
			_exit(126);
		}
		if (in >= 0) {
			dup2(in, 0);
		}
		if (out >= 0) {
			dup2(out, 1);
		}
		if (seconds > 0) { // set here, in the child, so they measure the submission and not the run-up to it
			struct rlimit cpu = { seconds, seconds + 1 };
			setrlimit(RLIMIT_CPU, &cpu);
		}
		if (memory > 0) {
			struct rlimit space = { memory, memory };
			setrlimit(RLIMIT_AS, &space);
		}
		if (seconds > 0) { // the submission, rather than a command of the judge's own
			struct rlimit written = { 64L * 1024 * 1024, 64L * 1024 * 1024 };
			setrlimit(RLIMIT_FSIZE, &written);
			struct rlimit none = { 0, 0 };
			setrlimit(RLIMIT_CORE, &none);
			struct rlimit files = { 64, 64 };
			setrlimit(RLIMIT_NOFILE, &files);
		}
		execvp(argv[0], argv);
		perror(argv[0]);
		_exit(127);
	}
	child = pid;
	killed = 0;
	if (seconds > 0) { // armed after the fork: a wall clock backstop for a submission that blocks instead of computing
		alarm(seconds + 1);
	}
	int status;
	waitpid(pid, &status, 0);
	alarm(0);
	child = 0;
	if (killed || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		return 1;
	}
	return 0;
}

static char *get_word(int fd) {
	char *word = NULL, c = ' ';
	while (c == ' ' || c == '\n') {
		if (read(fd, &c, 1) <= 0) {
			return NULL;
		}
	}
	int i = 0;
	for (; c != ' ' && c != '\n'; i++) {
		word = realloc(word, (i + 1) * sizeof(char));
		word[i] = c;
		if (read(fd, &c, 1) <= 0) {
			break;
		}
	}
	word = realloc(word, (i + 1) * sizeof(char));
	word[i] = '\0';
	return word;
}

static char checker_byte(int fd, int ans) { // word by word, so spaces and line breaks do not matter
	char *word1, *word2;
	while (1) {
		word1 = get_word(fd);
		word2 = get_word(ans);
		if (!word1 || !word2) {
			break;
		}
		if (strcmp(word1, word2)) {
			free(word1);
			free(word2);
			return '-';
		}
		free(word1);
		free(word2);
	}
	if (word1) {
		free(word1);
		return '-';
	}
	if (word2) {
		free(word2);
		return '-';
	}
	return '+';
}

static int next_byte(int fd) { // the next byte that is not a space, or -1 at the end of the file
	char c;
	for (;;) {
		ssize_t got = read(fd, &c, 1);
		if (got < 0) {
			perror("read error");
			_exit(3);
		}
		if (got == 0) {
			return -1;
		}
		if (c != ' ') {
			return (unsigned char)c;
		}
	}
}

static char checker_int(int fd, int ans) {
	for (;;) { // both sides have to end together, so an output that is a prefix of the answer is wrong
		int expected = next_byte(ans);
		int got = next_byte(fd);
		if (expected != got) {
			return '-';
		}
		if (expected == -1) {
			return '+';
		}
	}
}

/* checker_byte and checker_int are compiled in; any other name is a program
   under the contest directory, given the test, the output and the answer, and
   exiting 0 when it accepts the output. */
static char check(Contest *contest, Problem *problem, char *in_path, char *out_path, char *ans_path) {
	if (strcmp(problem->checker, "checker_byte") == 0 || strcmp(problem->checker, "checker_int") == 0) {
		int out = open(out_path, O_RDONLY), ans = open(ans_path, O_RDONLY);
		if (out < 0 || ans < 0) {
			perror(out < 0 ? out_path : ans_path);
			if (out >= 0) {
				close(out);
			}
			if (ans >= 0) {
				close(ans);
			}
			return 'x';
		}
		char mark = strcmp(problem->checker, "checker_int") == 0 ? checker_int(out, ans) : checker_byte(out, ans);
		close(out);
		close(ans);
		return mark;
	}
	char *path = contest_path(contest, "%s", problem->checker);
	char *argv[] = { path, in_path, out_path, ans_path, NULL };
	int refused = spawn(argv, -1, 2, NULL, 0, 0); // its stdout joins stderr: stdout here belongs to the marks
	free(path);
	return refused == 0 ? '+' : '-';
}

static char *find_source(Contest *contest, const char *student, char letter, Language **language) {
	for (int i = 0; i < contest->languages_count; i++) { // absolute, because the submission runs from a directory of its own
		char *path = path_fmt("%s/code/%s/%c.%s", contest->root, student, letter, contest->languages[i].ext);
		if (path != NULL && access(path, R_OK) == 0) {
			*language = &contest->languages[i];
			return path;
		}
		free(path);
	}
	return NULL;
}

static void mark_write(int log, char mark) {
	if (write(log, &mark, 1) < 0) {
		perror("write error");
		_exit(5);
	}
	write(1, &mark, 1);
}

int main(int argc, char **argv) {
	signal(SIGALRM, handler);
	if (argc != 4) {
		fprintf(stderr, "usage: %s <contest directory> <student> <problem letter>\n", argv[0]);
		return -1;
	}
	Contest *contest = contest_load(argv[1]);
	if (contest == NULL) {
		return -1;
	}
	const char *student = argv[2];
	char letter = argv[3][0];
	Problem *problem = contest_problem(contest, letter);
	if (problem == NULL) {
		fprintf(stderr, "the contest has no problem %c\n", letter);
		contest_free(contest);
		return -1;
	}
	char *log_path = contest_path(contest, "log/%s_%c.log", student, letter);
	int log = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (log < 0) {
		perror(log_path);
		free(log_path);
		contest_free(contest);
		return -1;
	}
	free(log_path);

	int confined = contest->sandbox != SANDBOX_OFF && sandbox_available();
	if (contest->sandbox == SANDBOX_REQUIRED && !confined) {
		fprintf(stderr, "the contest asks for a sandbox and this system has none\n");
		contest_free(contest);
		return -1;
	}

	Language *language = NULL;
	char *src = find_source(contest, student, letter, &language);
	char *bin = path_fmt("%s/tmp/%s_%c", contest->root, student, letter);
	int built = src != NULL;
	if (built && language->compile != NULL) {
		char **command = argv_expand(language->compile, src, bin);
		built = command != NULL && spawn(command, -1, 2, NULL, 0, 0) == 0;
		argv_free(command);
	}
	if (!built) { // nothing submitted, or nothing that builds
		mark_write(log, 'X');
	} else {
		char **command = argv_expand(language->run, src, bin);
		char *scratch = path_fmt("%s/tmp/%s_%c.d", contest->root, student, letter);
		mkdir(scratch, 0755); // the one place the submission is allowed to write
		char *out_path = path_fmt("%s/output", scratch);
		long memory = problem->memory > 0 && memory_limit_works(problem->memory) ? problem->memory : 0;
		if (confined) {
			char *profile = path_fmt("%s/profile.sb", scratch);
			char *tests = path_fmt("%s/tests", contest->root);
			char **wrapped = write_profile(profile, scratch, tests) == 0 ? sandbox_wrap(command, profile) : NULL;
			free(profile);
			free(tests);
			if (wrapped == NULL) {
				fprintf(stderr, "%s %c: the sandbox could not be set up\n", student, letter);
				return -1;
			}
			argv_free(command);
			command = wrapped;
		}
		for (int i = 1; i <= problem->tests; i++) {
			char *in_path = contest_path(contest, "tests/%c/%03d.dat", letter, i);
			char *ans_path = contest_path(contest, "tests/%c/%03d.ans", letter, i);
			int in = open(in_path, O_RDONLY);
			int out = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			char mark;
			if (in < 0 || out < 0) { // a test that is not there is not a result the submission earned
				perror(in < 0 ? in_path : out_path);
				mark = 'x';
			} else if (spawn(command, in, out, scratch, problem->seconds, memory) != 0) {
				mark = 'x';
			} else {
				close(out);
				out = -1;
				mark = check(contest, problem, in_path, out_path, ans_path);
			}
			if (in >= 0) {
				close(in);
			}
			if (out >= 0) {
				close(out);
			}
			mark_write(log, mark);
			free(in_path);
			free(ans_path);
		}
		unlink(out_path);
		free(out_path);
		free(scratch);
		argv_free(command);
	}
	char newline = '\n';
	write(1, &newline, 1); // the judge reads the marks up to this
	close(log);
	free(src);
	free(bin);
	contest_free(contest);
	return 0;
}

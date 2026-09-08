#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include "config.h"

#define COLUMN 20 /* width of the name column in results.log */
#define CELL 4    /* width of one problem's column */

static void wr(int fd, const char *str) {
	write(fd, str, strlen(str));
}

static void wr_time(int fd) {
	time_t now = time(NULL);
	char *text = ctime(&now);
	write(fd, text, strlen(text) - 1); // ctime ends in a newline of its own
}

static void wr_cell(int fd, char mark) {
	char cell[CELL];
	memset(cell, ' ', CELL);
	cell[0] = mark;
	write(fd, cell, CELL);
}

/* One mark for a whole submission. A crash or a timeout outranks a wrong
   answer, so that results.log can tell the two apart. */
static char summarise(const char *marks, int count) {
	if (count == 0) {
		return 'X';
	}
	if (count == 1 && marks[0] == 'X') {
		return 'X';
	}
	char mark = '+';
	for (int i = 0; i < count; i++) {
		if (marks[i] == 'x') {
			return 'x';
		}
		if (marks[i] != '+') {
			mark = '-';
		}
	}
	return mark;
}

/* test lives next to the judge, whatever directory the judge was started
   from. A judge found on PATH leaves the same search to execvp. */
static char *runner_path(const char *argv0) {
	const char *slash = strrchr(argv0, '/');
	if (slash == NULL) {
		return path_fmt("test");
	}
	return path_fmt("%.*stest", (int)(slash - argv0 + 1), argv0);
}

static char *run_one(Contest *contest, const char *runner, const char *student, char letter, int *count) {
	*count = 0;
	int pipe_fd[2];
	if (pipe(pipe_fd) < 0) {
		perror("pipe failed");
		return NULL;
	}
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork failed");
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		return NULL;
	}
	if (pid == 0) {
		dup2(pipe_fd[1], 1);
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		char letter_text[2] = { letter, '\0' };
		char *args[] = { (char *)runner, contest->dir, (char *)student, letter_text, NULL };
		execvp(runner, args);
		perror(runner);
		_exit(1);
	}
	close(pipe_fd[1]);
	char *marks = malloc(1);
	char c;
	while (marks != NULL && read(pipe_fd[0], &c, 1) > 0 && c != '\n') {
		char *grown = realloc(marks, *count + 2);
		if (grown == NULL) {
			break;
		}
		marks = grown;
		marks[*count] = c;
		(*count)++;
	}
	if (marks != NULL) {
		marks[*count] = '\0';
	}
	close(pipe_fd[0]);
	waitpid(pid, NULL, 0); // reaped only once the pipe is drained, so a long output cannot deadlock the pair
	return marks;
}

static void trace(int log2, const char *student, char letter, const char *marks, int count, char mark) {
	char line[256];
	wr_time(log2);
	snprintf(line, sizeof(line), " user: %s, problem :%c, tested :%d\n", student, letter, count);
	wr(log2, line);
	wr_time(log2);
	wr(log2, " ");
	wr(log2, marks);
	wr(log2, "\n");
	wr_time(log2);
	if (count == 0) {
		wr(log2, " the runner produced no result\n");
	} else if (mark == 'X') {
		wr(log2, " not submitted or did not compile\n");
	} else {
		int accepted = 0, failed = 0;
		for (int i = 0; i < count; i++) {
			if (marks[i] == '+') { // only a pass is accepted: '-' is a wrong answer and 'x' a crash or a timeout
				accepted++;
			} else {
				failed++;
			}
		}
		snprintf(line, sizeof(line), " accepted: %d, failed: %d\n", accepted, failed);
		wr(log2, line);
	}
	wr_time(log2);
	wr(log2, " stop test\n");
}

static int run_user(Contest *contest, const char *runner, const char *student, int log, int log2) {
	char column[COLUMN];
	memset(column, ' ', COLUMN);
	size_t len = strlen(student);
	if (len > COLUMN) { // the column is a fixed width; a longer name would run off the end of the buffer
		len = COLUMN;
	}
	memcpy(column, student, len);
	write(log, column, COLUMN);

	char *dir = contest_path(contest, "code/%s", student);
	DIR *submitted = dir == NULL ? NULL : opendir(dir);
	if (submitted == NULL) { // named in the configuration but absent from the disk: the whole row is X and the run carries on
		perror(dir == NULL ? student : dir);
		free(dir);
		for (int i = 0; i < contest->problems_count; i++) {
			wr_cell(log, 'X');
		}
		wr(log, "\n");
		return 0;
	}
	closedir(submitted);
	free(dir);

	for (int i = 0; i < contest->problems_count; i++) {
		char letter = contest->problems[i].letter;
		wr_time(log2);
		wr(log2, " start test\n");
		int count = 0;
		char *marks = run_one(contest, runner, student, letter, &count);
		if (marks == NULL) {
			return -1;
		}
		char mark = summarise(marks, count);
		wr_cell(log, mark);
		trace(log2, student, letter, marks, count, mark);
		free(marks);
	}
	wr(log, "\n");
	return 0;
}

int main(int argc, char **argv) {
	if (argc > 2) {
		fprintf(stderr, "usage: %s [contest directory]\n", argv[0]);
		return -1;
	}
	Contest *contest = contest_load(argc == 2 ? argv[1] : "../contest");
	if (contest == NULL) {
		return -1;
	}
	char *runner = runner_path(argv[0]);
	char *results = contest_path(contest, "log/results.log");
	char *results2 = contest_path(contest, "log/results2.log");
	int log = results == NULL ? -1 : open(results, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	int log2 = results2 == NULL ? -1 : open(results2, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (log < 0 || log2 < 0) { // without this the whole run writes into fd -1 and reports nothing at all
		perror(log < 0 ? results : results2);
		if (log >= 0) {
			close(log);
		}
		if (log2 >= 0) {
			close(log2);
		}
		free(results);
		free(results2);
		free(runner);
		contest_free(contest);
		return -1;
	}
	free(results);
	free(results2);

	char header[COLUMN];
	memcpy(header, "    users/problems  ", COLUMN); // exactly the width of the name column, so it carries no terminator
	write(log, header, COLUMN);
	for (int i = 0; i < contest->problems_count; i++) {
		wr_cell(log, contest->problems[i].letter);
	}
	wr(log, "\n");

	int status = 0;
	for (int i = 0; i < contest->users_count && status == 0; i++) {
		status = run_user(contest, runner, contest->users[i], log, log2);
	}
	if (status != 0) {
		fprintf(stderr, "the run stopped early\n");
	}
	close(log);
	close(log2);
	free(runner);
	contest_free(contest);
	return status;
}

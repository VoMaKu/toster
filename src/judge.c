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

/* One submission: what came back from the runner, and when. Jobs are run in
   whatever order finishes first and written out in the order of the
   configuration, so the logs do not depend on how many run at once. */
typedef struct {
	int user;
	int problem;
	time_t started;
	time_t stopped;
	char *marks;
	int count;
	char mark;
} Job;

typedef struct {
	pid_t pid;
	int fd;
	int job;
} Slot;

static void wr(int fd, const char *str) {
	write(fd, str, strlen(str));
}

static void wr_time(int fd, time_t when) {
	char *text = ctime(&when);
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
	if (count == 0 || (count == 1 && marks[0] == 'X')) {
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

static int start_job(Contest *contest, const char *runner, Job *job, Slot *slot) {
	int pipe_fd[2];
	if (pipe(pipe_fd) < 0) {
		perror("pipe failed");
		return -1;
	}
	job->started = time(NULL);
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork failed");
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		return -1;
	}
	if (pid == 0) {
		dup2(pipe_fd[1], 1);
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		char letter[2] = { contest->problems[job->problem].letter, '\0' };
		char *args[] = { (char *)runner, contest->dir, contest->users[job->user], letter, NULL };
		execvp(runner, args);
		perror(runner);
		_exit(1);
	}
	close(pipe_fd[1]);
	slot->pid = pid;
	slot->fd = pipe_fd[0];
	return 0;
}

/* The marks are a handful of bytes, so they sit in the pipe until they are
   read; collecting them after the runner has exited cannot deadlock. */
static void collect(Job *job, int fd) {
	job->marks = malloc(1);
	job->count = 0;
	char c;
	while (job->marks != NULL && read(fd, &c, 1) > 0 && c != '\n') {
		char *grown = realloc(job->marks, job->count + 2);
		if (grown == NULL) {
			break;
		}
		job->marks = grown;
		job->marks[job->count] = c;
		job->count++;
	}
	if (job->marks != NULL) {
		job->marks[job->count] = '\0';
	}
	job->stopped = time(NULL);
	job->mark = summarise(job->marks == NULL ? "" : job->marks, job->count);
}

static int run_jobs(Contest *contest, const char *runner, Job *jobs, int count, int workers) {
	Slot *slots = calloc(workers, sizeof(Slot));
	if (slots == NULL) {
		return -1;
	}
	for (int i = 0; i < workers; i++) {
		slots[i].job = -1;
	}
	int next = 0, done = 0, running = 0, failed = 0;
	while (done < count && !failed) {
		while (next < count && running < workers) {
			int slot = 0;
			while (slots[slot].job >= 0) {
				slot++;
			}
			if (start_job(contest, runner, &jobs[next], &slots[slot]) != 0) {
				failed = 1;
				break;
			}
			slots[slot].job = next;
			next++;
			running++;
		}
		if (running == 0) {
			break;
		}
		pid_t pid = waitpid(-1, NULL, 0);
		if (pid < 0) {
			perror("waitpid failed");
			failed = 1;
			break;
		}
		for (int slot = 0; slot < workers; slot++) {
			if (slots[slot].job >= 0 && slots[slot].pid == pid) {
				collect(&jobs[slots[slot].job], slots[slot].fd);
				close(slots[slot].fd);
				slots[slot].job = -1;
				running--;
				done++;
				break;
			}
		}
	}
	free(slots);
	return failed ? -1 : 0;
}

static void write_trace(int log2, const char *student, char letter, const Job *job) {
	char line[256];
	wr_time(log2, job->started);
	wr(log2, " start test\n");
	wr_time(log2, job->started);
	snprintf(line, sizeof(line), " user: %s, problem :%c, tested :%d\n", student, letter, job->count);
	wr(log2, line);
	wr_time(log2, job->stopped);
	wr(log2, " ");
	wr(log2, job->marks == NULL ? "" : job->marks);
	wr(log2, "\n");
	wr_time(log2, job->stopped);
	if (job->count == 0) {
		wr(log2, " the runner produced no result\n");
	} else if (job->mark == 'X') {
		wr(log2, " not submitted or did not compile\n");
	} else {
		int accepted = 0, failed = 0;
		for (int i = 0; i < job->count; i++) {
			if (job->marks[i] == '+') { // only a pass is accepted: '-' is a wrong answer and 'x' a crash or a timeout
				accepted++;
			} else {
				failed++;
			}
		}
		snprintf(line, sizeof(line), " accepted: %d, failed: %d\n", accepted, failed);
		wr(log2, line);
	}
	wr_time(log2, job->stopped);
	wr(log2, " stop test\n");
}

static void write_name(int log, const char *student) {
	char column[COLUMN];
	memset(column, ' ', COLUMN);
	size_t len = strlen(student);
	if (len > COLUMN) { // the column is a fixed width; a longer name would run off the end of the buffer
		len = COLUMN;
	}
	memcpy(column, student, len);
	write(log, column, COLUMN);
}

static int submitted(Contest *contest, const char *student) {
	char *dir = contest_path(contest, "code/%s", student);
	DIR *found = dir == NULL ? NULL : opendir(dir);
	if (found == NULL) { // named in the configuration but absent from the disk: the whole row is X and the run carries on
		perror(dir == NULL ? student : dir);
		free(dir);
		return 0;
	}
	closedir(found);
	free(dir);
	return 1;
}

static int workers_wanted(const char *text) {
	int workers = atoi(text);
	if (workers < 0) {
		return -1;
	}
	if (workers == 0) { // as many as the machine has processors
		long cpus = sysconf(_SC_NPROCESSORS_ONLN);
		workers = cpus > 0 ? (int)cpus : 1;
	}
	return workers;
}

int main(int argc, char **argv) {
	const char *dir = "../contest";
	int workers = 1, named = 0;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
			workers = workers_wanted(argv[++i]);
		} else if (strncmp(argv[i], "-j", 2) == 0) {
			workers = workers_wanted(argv[i] + 2);
		} else if (!named) {
			dir = argv[i];
			named = 1;
		} else {
			workers = -1;
		}
		if (workers < 0) {
			fprintf(stderr, "usage: %s [-j workers] [contest directory]\n", argv[0]);
			return -1;
		}
	}
	Contest *contest = contest_load(dir);
	if (contest == NULL) {
		return -1;
	}
	if (contest->sandbox != SANDBOX_OFF && !sandbox_available()) { // said once here rather than by every runner in turn
		if (contest->sandbox == SANDBOX_REQUIRED) {
			fprintf(stderr, "the contest asks for a sandbox and this system has none\n");
			contest_free(contest);
			return -1;
		}
		fprintf(stderr, "this system has no sandbox: submissions run unconfined\n");
	}
	char *runner = runner_path(argv[0]);
	char *results = contest_path(contest, "log/results.log");
	char *results2 = contest_path(contest, "log/results2.log");
	int log = results == NULL ? -1 : open(results, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	int log2 = results2 == NULL ? -1 : open(results2, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (log < 0 || log2 < 0) { // without this the whole run writes into fd -1 and reports nothing at all
		perror(log < 0 ? results : results2);
		free(results);
		free(results2);
		free(runner);
		contest_free(contest);
		return -1;
	}
	free(results);
	free(results2);

	int *present = calloc(contest->users_count, sizeof(int));
	Job *jobs = calloc(contest->users_count * contest->problems_count, sizeof(Job));
	if (present == NULL || jobs == NULL) {
		return -1;
	}
	int count = 0;
	for (int user = 0; user < contest->users_count; user++) {
		present[user] = submitted(contest, contest->users[user]);
		for (int problem = 0; present[user] && problem < contest->problems_count; problem++) {
			jobs[count].user = user;
			jobs[count].problem = problem;
			count++;
		}
	}
	int status = run_jobs(contest, runner, jobs, count, workers);

	char header[COLUMN];
	memcpy(header, "    users/problems  ", COLUMN); // exactly the width of the name column, so it carries no terminator
	write(log, header, COLUMN);
	for (int i = 0; i < contest->problems_count; i++) {
		wr_cell(log, contest->problems[i].letter);
	}
	wr(log, "\n");
	int at = 0;
	for (int user = 0; user < contest->users_count; user++) {
		write_name(log, contest->users[user]);
		for (int problem = 0; problem < contest->problems_count; problem++) {
			if (!present[user]) {
				wr_cell(log, 'X');
				continue;
			}
			wr_cell(log, jobs[at].mark);
			write_trace(log2, contest->users[user], contest->problems[problem].letter, &jobs[at]);
			at++;
		}
		wr(log, "\n");
	}
	for (int i = 0; i < count; i++) {
		free(jobs[i].marks);
	}
	free(jobs);
	free(present);
	close(log);
	close(log2);
	free(runner);
	contest_free(contest);
	return status;
}

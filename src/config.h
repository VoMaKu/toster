#ifndef CONFIG_H
#define CONFIG_H

/* One language a submission may be written in. Both argv templates are
   NULL-terminated; the tokens {src} and {bin} stand for the submission and the
   binary built from it. compile is NULL for a language that is not built. */
typedef struct {
	char *ext;      /* "c", without the dot */
	char **compile;
	char **run;
} Language;

typedef struct {
	char letter;
	int tests;
	char *checker;  /* "checker_byte", "checker_int", or a path under the contest directory */
	int points;     /* what the whole problem is worth, shared out over its tests */
	int seconds;    /* time one test gets */
	long memory;    /* bytes one test gets; 0 leaves memory unlimited */
} Problem;

#define SANDBOX_EXEC "/usr/bin/sandbox-exec"

/* Whether a submission is confined while it runs. SANDBOX_REQUIRED stops a
   run that cannot have one rather than quietly going without. */
typedef enum {
	SANDBOX_OFF,
	SANDBOX_REQUIRED,
	SANDBOX_IF_AVAILABLE
} SandboxWish;

typedef struct {
	char *dir;      /* the contest directory every other path hangs off */
	char *root;     /* the same directory, resolved, for the sandbox profile */
	SandboxWish sandbox;
	char **users;   /* NULL-terminated, in the order of the scoreboard */
	int users_count;
	Problem *problems;
	int problems_count;
	Language *languages;
	int languages_count;
} Contest;

/* Reads dir/contest.json if it is there, and dir's .cfg files otherwise.
   Prints why and returns NULL when the contest cannot be read. */
Contest *contest_load(const char *dir);
void contest_free(Contest *contest);

/* Whether this system can confine a submission at all. */
int sandbox_available(void);

Problem *contest_problem(Contest *contest, char letter);
Language *contest_language(Contest *contest, const char *ext);

/* dir/<the formatted path>, freshly allocated. */
char *contest_path(Contest *contest, const char *fmt, ...);
char *path_fmt(const char *fmt, ...);

/* A copy of the template with {src} and {bin} filled in. */
char **argv_expand(char **template, const char *src, const char *bin);
void argv_free(char **argv);

#endif

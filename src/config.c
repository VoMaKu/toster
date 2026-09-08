#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "config.h"
#include "reader_cfg.h"
#include "reader_json.h"

static int contest_load_json(Contest *contest, const char *path);
char *cfg_lookup(char ***cfg, const char *key);

int sandbox_available(void) {
	return access(SANDBOX_EXEC, X_OK) == 0;
}

static int read_sandbox_wish(const char *value, SandboxWish *wish) {
	if (value == NULL || strcmp(value, "auto") == 0) {
		*wish = SANDBOX_IF_AVAILABLE;
	} else if (strcmp(value, "on") == 0) {
		*wish = SANDBOX_REQUIRED;
	} else if (strcmp(value, "off") == 0) {
		*wish = SANDBOX_OFF;
	} else {
		fprintf(stderr, "sandbox= takes on, off or auto, not %s\n", value);
		return -1;
	}
	return 0;
}

/* contest/contest.cfg holds what is true of the contest as a whole. It is
   optional; without it every setting keeps its default. */
static int load_settings(Contest *contest) {
	char *path = contest_path(contest, "contest.cfg");
	if (path == NULL) {
		return -1;
	}
	if (access(path, R_OK) != 0) {
		free(path);
		return 0;
	}
	char ***cfg = get_cfgs(path);
	free(path);
	if (cfg == NULL) {
		return -1;
	}
	int status = read_sandbox_wish(cfg_lookup(cfg, "sandbox"), &contest->sandbox);
	free_cfgs(cfg);
	return status;
}

char *path_fmt(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int size = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (size < 0) {
		return NULL;
	}
	char *out = malloc(size + 1);
	if (out == NULL) {
		return NULL;
	}
	va_start(ap, fmt);
	vsnprintf(out, size + 1, fmt, ap);
	va_end(ap);
	return out;
}

char *contest_path(Contest *contest, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int size = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (size < 0) {
		return NULL;
	}
	char *tail = malloc(size + 1);
	if (tail == NULL) {
		return NULL;
	}
	va_start(ap, fmt);
	vsnprintf(tail, size + 1, fmt, ap);
	va_end(ap);
	char *out = path_fmt("%s/%s", contest->dir, tail);
	free(tail);
	return out;
}

char *cfg_lookup(char ***cfg, const char *key) { // the value of a key=value line, or NULL if the file has no such key
	for (int i = 0; cfg != NULL && cfg[i] != NULL; i++) {
		if (cfg[i][0] != NULL && cfg[i][1] != NULL && strcmp(cfg[i][0], key) == 0) {
			return cfg[i][1];
		}
	}
	return NULL;
}

void argv_free(char **argv) {
	if (argv == NULL) {
		return;
	}
	for (int i = 0; argv[i] != NULL; i++) {
		free(argv[i]);
	}
	free(argv);
}

static char **argv_split(const char *line) { // a command line into its words; no quoting, so no word may contain a space
	char **argv = NULL;
	int count = 0;
	const char *p = line;
	while (*p != '\0') {
		while (*p == ' ' || *p == '\t') {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		const char *start = p;
		while (*p != '\0' && *p != ' ' && *p != '\t') {
			p++;
		}
		char **grown = realloc(argv, (count + 2) * sizeof(char *));
		if (grown == NULL) {
			argv_free(argv);
			return NULL;
		}
		argv = grown;
		argv[count] = path_fmt("%.*s", (int)(p - start), start);
		count++;
		argv[count] = NULL;
	}
	return argv;
}

static char *str_replace(const char *s, const char *what, const char *with) {
	size_t what_len = strlen(what), with_len = strlen(with);
	size_t size = strlen(s) + 1;
	for (const char *p = strstr(s, what); p != NULL; p = strstr(p + what_len, what)) {
		size += with_len - what_len;
	}
	char *out = malloc(size);
	if (out == NULL) {
		return NULL;
	}
	char *w = out;
	while (*s != '\0') {
		if (strncmp(s, what, what_len) == 0) {
			memcpy(w, with, with_len);
			w += with_len;
			s += what_len;
		} else {
			*w++ = *s++;
		}
	}
	*w = '\0';
	return out;
}

char **argv_expand(char **template, const char *src, const char *bin) {
	if (template == NULL) {
		return NULL;
	}
	int count = 0;
	while (template[count] != NULL) {
		count++;
	}
	char **argv = calloc(count + 1, sizeof(char *));
	if (argv == NULL) {
		return NULL;
	}
	for (int i = 0; i < count; i++) {
		char *once = str_replace(template[i], "{src}", src);
		argv[i] = str_replace(once, "{bin}", bin);
		free(once);
	}
	return argv;
}

Problem *contest_problem(Contest *contest, char letter) {
	for (int i = 0; i < contest->problems_count; i++) {
		if (contest->problems[i].letter == letter) {
			return &contest->problems[i];
		}
	}
	return NULL;
}

Language *contest_language(Contest *contest, const char *ext) {
	for (int i = 0; i < contest->languages_count; i++) {
		if (strcmp(contest->languages[i].ext, ext) == 0) {
			return &contest->languages[i];
		}
	}
	return NULL;
}

static int language_add(Contest *contest, const char *ext, const char *compile, const char *run) {
	Language *grown = realloc(contest->languages, (contest->languages_count + 1) * sizeof(Language));
	if (grown == NULL) {
		return -1;
	}
	contest->languages = grown;
	Language *lang = &contest->languages[contest->languages_count];
	lang->ext = path_fmt("%s", ext);
	lang->compile = compile == NULL ? NULL : argv_split(compile);
	lang->run = argv_split(run);
	if (lang->ext == NULL || lang->run == NULL || (compile != NULL && lang->compile == NULL)) {
		return -1;
	}
	contest->languages_count++;
	return 0;
}

/* contest/lang.cfg holds two lines per language:

       c.compile=gcc {src} -o {bin} -lm
       c.run={bin}

   A language with no compile line is run straight from its source. */
static int load_languages(Contest *contest) {
	char *path = contest_path(contest, "lang.cfg");
	if (path == NULL) {
		return -1;
	}
	if (access(path, R_OK) != 0) { // no table: C the way this judge always built it
		free(path);
		return language_add(contest, "c", "gcc {src} -o {bin}", "{bin}");
	}
	char ***cfg = get_cfgs(path);
	free(path);
	if (cfg == NULL) {
		return -1;
	}
	int status = 0;
	for (int i = 0; cfg[i] != NULL && status == 0; i++) {
		if (cfg[i][0] == NULL || cfg[i][1] == NULL) {
			continue;
		}
		char *dot = strchr(cfg[i][0], '.');
		if (dot == NULL || strcmp(dot, ".run") != 0) { // the run line is the one that declares a language
			continue;
		}
		char *ext = path_fmt("%.*s", (int)(dot - cfg[i][0]), cfg[i][0]);
		char *compile_key = path_fmt("%s.compile", ext);
		status = language_add(contest, ext, cfg_lookup(cfg, compile_key), cfg[i][1]);
		free(compile_key);
		free(ext);
	}
	free_cfgs(cfg);
	if (status == 0 && contest->languages_count == 0) {
		fprintf(stderr, "lang.cfg declares no language: every entry needs a <ext>.run line\n");
		return -1;
	}
	return status;
}

static int load_users(Contest *contest) {
	char *path = contest_path(contest, "code/user.cfg");
	char ***cfg = path == NULL ? NULL : get_cfgs(path);
	free(path);
	if (cfg == NULL) {
		return -1;
	}
	char *count = cfg_lookup(cfg, "users");
	if (count == NULL) {
		fprintf(stderr, "user.cfg carries no users= count\n");
		free_cfgs(cfg);
		return -1;
	}
	contest->users_count = atoi(count);
	contest->users = calloc(contest->users_count + 1, sizeof(char *));
	if (contest->users == NULL) {
		free_cfgs(cfg);
		return -1;
	}
	for (int i = 0; i < contest->users_count; i++) {
		char *key = path_fmt("%d", i + 1);
		char *name = cfg_lookup(cfg, key);
		free(key);
		if (name == NULL) {
			fprintf(stderr, "user.cfg carries no name numbered %d\n", i + 1);
			free_cfgs(cfg);
			return -1;
		}
		contest->users[i] = path_fmt("%s", name);
	}
	free_cfgs(cfg);
	return 0;
}

static int load_problems(Contest *contest) {
	char *path = contest_path(contest, "tests/problem.cfg");
	char ***cfg = path == NULL ? NULL : get_cfgs(path);
	free(path);
	if (cfg == NULL) {
		return -1;
	}
	char *count = cfg_lookup(cfg, "problems");
	if (count == NULL) {
		fprintf(stderr, "tests/problem.cfg carries no problems= count\n");
		free_cfgs(cfg);
		return -1;
	}
	contest->problems_count = atoi(count);
	free_cfgs(cfg);
	contest->problems = calloc(contest->problems_count > 0 ? contest->problems_count : 1, sizeof(Problem));
	if (contest->problems == NULL) {
		return -1;
	}
	for (int i = 0; i < contest->problems_count; i++) {
		Problem *problem = &contest->problems[i];
		problem->letter = 'A' + i;
		problem->seconds = 2;   // what the limit was when it was compiled in
		problem->memory = 0;    // no limit unless the problem asks for one
		char *problem_path = contest_path(contest, "tests/%c/problem.cfg", problem->letter);
		char ***one = problem_path == NULL ? NULL : get_cfgs(problem_path);
		if (one == NULL) {
			free(problem_path);
			return -1;
		}
		char *tests = cfg_lookup(one, "tests");
		char *checker = cfg_lookup(one, "checker");
		char *seconds = cfg_lookup(one, "time");
		char *memory = cfg_lookup(one, "memory");
		if (tests == NULL || atoi(tests) <= 0) {
			fprintf(stderr, "%s: no tests= count\n", problem_path);
			free_cfgs(one);
			free(problem_path);
			return -1;
		}
		problem->tests = atoi(tests);
		problem->checker = path_fmt("%s", checker == NULL ? "checker_byte" : checker);
		if (seconds != NULL) {
			problem->seconds = atoi(seconds);
		}
		if (memory != NULL) {
			problem->memory = atol(memory) * 1024 * 1024;
		}
		free_cfgs(one);
		free(problem_path);
	}
	return 0;
}

Contest *contest_load(const char *dir) {
	Contest *contest = calloc(1, sizeof(Contest));
	if (contest == NULL) {
		return NULL;
	}
	contest->dir = path_fmt("%s", dir);
	contest->sandbox = SANDBOX_IF_AVAILABLE;
	char resolved[PATH_MAX];
	contest->root = realpath(dir, resolved) == NULL ? NULL : path_fmt("%s", resolved);
	if (contest->dir == NULL || contest->root == NULL) {
		perror(dir);
		contest_free(contest);
		return NULL;
	}
	char *json = contest_path(contest, "contest.json");
	int found_json = json != NULL && access(json, R_OK) == 0;
	int status;
	if (found_json) { // one file describing the whole contest wins over the .cfg files beside it
		status = contest_load_json(contest, json);
	} else {
		status = load_users(contest);
		if (status == 0) {
			status = load_problems(contest);
		}
		if (status == 0) {
			status = load_languages(contest);
		}
		if (status == 0) {
			status = load_settings(contest);
		}
	}
	free(json);
	if (status != 0) {
		contest_free(contest);
		return NULL;
	}
	return contest;
}

void contest_free(Contest *contest) {
	if (contest == NULL) {
		return;
	}
	for (int i = 0; contest->users != NULL && contest->users[i] != NULL; i++) {
		free(contest->users[i]);
	}
	free(contest->users);
	for (int i = 0; i < contest->problems_count && contest->problems != NULL; i++) {
		free(contest->problems[i].checker);
	}
	free(contest->problems);
	for (int i = 0; i < contest->languages_count; i++) {
		free(contest->languages[i].ext);
		argv_free(contest->languages[i].compile);
		argv_free(contest->languages[i].run);
	}
	free(contest->languages);
	free(contest->root);
	free(contest->dir);
	free(contest);
}

/* contest.json says the same things as the .cfg files, in one file:

       {
         "users": ["Aitassova", "Albek"],
         "languages": { "c": { "compile": "gcc {src} -o {bin} -lm", "run": "{bin}" } },
         "problems": [ { "letter": "A", "tests": 6, "checker": "checker_byte",
                         "time": 2, "memory": 256 } ]
       }

   Only "users" and "problems" have to be there, and inside a problem only
   "tests". */
static int json_to_contest(Contest *contest, Json *root, const char *path) {
	Json *users = json_get(root, "users");
	Json *problems = json_get(root, "problems");
	Json *languages = json_get(root, "languages");
	Json *sandbox = json_get(root, "sandbox");
	if (sandbox != NULL && read_sandbox_wish(json_string(sandbox), &contest->sandbox) != 0) {
		return -1;
	}
	if (users == NULL || users->type != JSON_ARRAY) {
		fprintf(stderr, "%s: \"users\" has to be an array of names\n", path);
		return -1;
	}
	if (problems == NULL || problems->type != JSON_ARRAY) {
		fprintf(stderr, "%s: \"problems\" has to be an array\n", path);
		return -1;
	}
	contest->users_count = users->count;
	contest->users = calloc(users->count + 1, sizeof(char *));
	if (contest->users == NULL) {
		return -1;
	}
	for (int i = 0; i < users->count; i++) {
		const char *name = json_string(json_at(users, i));
		if (name == NULL) {
			fprintf(stderr, "%s: user %d is not a name\n", path, i + 1);
			return -1;
		}
		contest->users[i] = path_fmt("%s", name);
	}
	contest->problems_count = problems->count;
	contest->problems = calloc(problems->count > 0 ? problems->count : 1, sizeof(Problem));
	if (contest->problems == NULL) {
		return -1;
	}
	for (int i = 0; i < problems->count; i++) {
		Json *one = json_at(problems, i);
		Problem *problem = &contest->problems[i];
		const char *letter = json_string(json_get(one, "letter"));
		const char *checker = json_string(json_get(one, "checker"));
		problem->letter = letter == NULL ? 'A' + i : letter[0];
		problem->tests = json_int(json_get(one, "tests"), 0);
		problem->checker = path_fmt("%s", checker == NULL ? "checker_byte" : checker);
		problem->seconds = json_int(json_get(one, "time"), 2);
		problem->memory = json_int(json_get(one, "memory"), 0) * 1024 * 1024;
		if (problem->tests <= 0) {
			fprintf(stderr, "%s: problem %c carries no test count\n", path, problem->letter);
			return -1;
		}
	}
	if (languages == NULL) {
		return language_add(contest, "c", "gcc {src} -o {bin}", "{bin}");
	}
	if (languages->type != JSON_OBJECT) {
		fprintf(stderr, "%s: \"languages\" has to be an object\n", path);
		return -1;
	}
	for (int i = 0; i < languages->count; i++) {
		Json *one = languages->items[i];
		const char *run = json_string(json_get(one, "run"));
		if (run == NULL) {
			fprintf(stderr, "%s: language %s carries no \"run\" command\n", path, languages->keys[i]);
			return -1;
		}
		if (language_add(contest, languages->keys[i], json_string(json_get(one, "compile")), run) != 0) {
			return -1;
		}
	}
	if (contest->languages_count == 0) {
		fprintf(stderr, "%s: \"languages\" declares none\n", path);
		return -1;
	}
	return 0;
}

static int contest_load_json(Contest *contest, const char *path) {
	Json *root = json_parse_file(path);
	if (root == NULL) {
		return -1;
	}
	int status = json_to_contest(contest, root, path);
	json_free(root);
	return status;
}

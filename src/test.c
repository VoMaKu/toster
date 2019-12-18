#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>


int num = 0, mode = 0, flag = 0;
char *programm;
pid_t child;

void handler(int singno){ // если программа работает слишком долго то убивает ее
	if (child != 0) {
		kill(child, SIGKILL);
		flag = 1;
	}
}

int createprogramm(char *prog){ // компиляция программы в папке tmp/username_programmname
	char *name = malloc(6 * sizeof(char));
	name[0] = '.';
	name[1] = '.';
	name[2] = '/';
	name[3] = 't';
	name[4] = 'm';
	name[5] = 'p';
	int size = 6;
	for (int i = 0, fl = 0; fl != 4; i++) { // ищем username в  ../contest/code/username/programmname
		if (prog[i] == '/') {
			fl++;
		}
		if (fl == 3) {
			name = realloc(name, (++size) * sizeof(char));
			name[size - 1] = prog[i];
		}
	}
	name = realloc(name, (size + 3) * sizeof(char));
	name[size] = '_';
	name[size + 1] = prog[strlen(prog) - 3]; // ищем programmname
	name[size + 2] = '\0';
	size += 5;
	programm = malloc((size) * sizeof(char));
	programm[0] = '.';
	programm[1] = '/';
	for (int i = 2; i < size; i++) {
		programm[i] = name[i - 2];
	}
	pid_t pid;
	if ((pid = fork()) == 0) { //  компилируем программу 
		if (execlp("gcc", "gcc", prog, "-o", name, NULL) < 0) {
			perror("execlp error");
			free(name);
			_exit(88);
		}
	}
	else {
		int status;
		wait(&status);
		if (WEXITSTATUS(status) == 88) {
			return 1;
		}
		free(name);
	}
	return 0;
}

struct dirent *foundproblemcfg(DIR *test){ // поиск файла problem.cfg
	struct dirent *problem = readdir(test);
	while (strcmp(problem -> d_name, "problem.cfg") != 0) {
		problem = readdir(test);
		if (problem == NULL){
			perror("problem.cfg doesn't exit");
			_exit(2);
		}
	}
	return problem;
}

char *makecfg(char *ptr1, char *ptr2) {
	int size1 = strlen(ptr1);
	int size2 = strlen(ptr2);
	char *name = malloc((size1 + 1 + size2 + 1) * sizeof(char));
	sprintf(name, "../contest/tests/%c/%s\0", ptr2[size2 - 1], ptr1);
	return name;
}

void readcfg(int cfg){ // то что нужно поменять, считывает problem.cfg в каждой из папки программы 
	char buf = 'F'; //эта функция сама по себе КОСТЫЛЬ!!!
	while (buf != '=') {
		if (read(cfg, &buf, sizeof(char)) < 0){
			perror("read error");
			_exit(3);
		}
	}
	if (read(cfg, &buf, sizeof(char)) < 0){
		perror("read error");
		_exit(3);
	}
	while (buf >= '0' && buf <= '9') {
		num = num * 10 + buf - '0';
		if (read(cfg, &buf, sizeof(char)) < 0){
			perror("read error");
			_exit(3);
		} 
	}
	while (buf != '_') {
		if (read(cfg, &buf, sizeof(char)) < 0){
			perror("read error");
			_exit(3);
		}
	}
	if (read(cfg, &buf, sizeof(char)) < 0){
		perror("read error");
		_exit(3);
	}
	if (buf == 'i'){
		mode = 1;
	}
}

char *makelog(){ // создает файл типа log : contest/log/username_programmname.log
	int size = strlen(programm) - 6;
	char *name = calloc(size + 15 + 5, sizeof(char));
	sprintf(name, "../contest/log/%s.log\0", programm + 9);
	printf("%s\n", name);
	return name;
}

char *get_word(int fd) {
    char *word = NULL, c = ' ';
    while (c == ' ' || c == '\n') {
        if (read(fd, &c, 1) <= 0)
            return NULL;
    }
    int i = 0;
    for (; c != ' ' && c != '\n'; i++) {
        word = realloc(word, (i + 1) * sizeof(char));
        word[i] = c;
        if (read(fd, &c, 1) <= 0)
            break;
    }
    word = realloc(word, (i + 1) * sizeof(char));
    word[i] = '\0';
    return word;
}

char checker_byte(int fd, int ans) {
	char *word1, *word2;
    while (1) {
        word1 = get_word(fd);
        word2 = get_word(ans);
        if (!word1 || !word2)
            break;
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


char checker_int(int fd, int ans) {
	char buf1, buf2;
	int quit1 = 0, quit2 = 0;
	do {
		quit1 = read(ans, &buf1, sizeof(char));
		quit2 = read(fd, &buf2, sizeof(char));
		while(buf1 == ' ' && quit1 != 0) {
			quit1 = read(ans, &buf1, sizeof(char));
		}
		while(buf2 == ' ' && quit2 != 0) {
			quit2 = read(fd, &buf2, sizeof(char));
		}
		if (buf1 != buf2) {
			return '-';
		}
	} while (quit1 != 0 && quit2 != 0);
	return '+';
}

void checker(int log, char *argv) {
	for (int i = 0; i < num; i++) { // тестируем на всех тестах
		int fd[2]; // pipe 
		pipe(fd);
		char data[27], answer[27];	//на каком тесте
		memset(data, 0, 27);
		memset(answer, 0, 27);
		sprintf(data, "../contest/tests/%c/%03d.dat",programm[strlen(programm) - 1], i + 1);
		sprintf(answer, "../contest/tests/%c/%03d.ans", programm[strlen(programm) - 1], i + 1);
		int fdata = open(data, O_RDONLY), fanswer = open(answer, O_RDONLY);
		alarm(2);
		if ((child = fork()) == 0) {
			dup2(fdata, 0);
			dup2(fd[1], 1);
			close(fd[0]);
			close(fd[1]);
			if (execlp(programm, NULL) < 0) {
				perror("execlp error");
				_exit(4);
			}	
		} else {
			int status;
			wait(&status);
			if (WEXITSTATUS(status) != 0) {
				flag = 1;
			}
			alarm(0);
			close(fd[1]);
			char buf;
			if (flag == 0){
				if (mode == 0) {
					buf = checker_byte(fd[0], fanswer);
				} else {
					buf = checker_int(fd[0], fanswer);
				}
			} else {
				buf = 'x';
				if(write(log, &buf, sizeof(char)) < 0){
					perror("write error");
					_exit(5);
				}
				flag = 0;
			}
			write(log, &buf, sizeof(char));
			write(1, &buf, sizeof(char));
			close(fd[0]);
			close(fdata);
			close(fanswer);
		}
	}
	char symbol_enter = '\n';
	write(1, &symbol_enter, sizeof(char));
}

int main(int argc, char **argv){
	signal(SIGALRM, handler);
	if (argc != 3) {
		perror("argv error");
		return -1;
	}
	int compile_programm = createprogramm(argv[1]);
	if (compile_programm != 1) {	
		DIR *test = opendir(argv[2]);
		struct dirent *problem = foundproblemcfg(test); 
		closedir(test);
		int cfg = open(makecfg(problem -> d_name, argv[2]), O_RDONLY);
		readcfg(cfg);
		close(cfg);
	}
	char *name_log = makelog();
	int log = open(name_log, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (compile_programm != 1) {	
		checker(log, argv[2]);
		free(programm);
	}
	else {
		char buf = 'X';
		write(1, &buf, 1);
		write(log, &buf, 1);
	}
	close(log);
	free(name_log);
	return 0;
}

//mode 0 = checker_byte
//mode 1 = checker_int
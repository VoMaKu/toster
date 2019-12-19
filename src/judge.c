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
//#include "reader_xml.h"
//#include "reader_cfg.h"
//#include "reader_json.h"
//#include "logger.h"
//#include "writer_csv.h"

char ***read_conf(char* where, char ***setting) {
    DIR *_cfg = opendir(where);
    struct dirent *cfg = readdir(_cfg);
	while(cfg != NULL){
        if (strcmp(cfg -> d_name, "user.cfg") == 0
           || strcmp(cfg -> d_name, "problem.cfg") == 0) {
           	char *name = malloc(strlen(where) + 1 + strlen(cfg -> d_name) + 1);
            sprintf(name, "%s/%s\0", where, cfg -> d_name);
            setting = get_cfgs(name);
            free(name);
            break;
        } else if (strcmp(cfg -> d_name, "user.xml") == 0
        || strcmp(cfg -> d_name, "problem.xml") == 0 ){
//            setting = reader_xml(cfg -> d_name);
            break;
        } else if (strcmp(cfg -> d_name, "user.json") == 0
            || strcmp(cfg -> d_name, "problem.json") == 0 ){
//            setting = reader_json(cfg -> d_name);
            break;
        }
        cfg = readdir(_cfg);
    }
	closedir(_cfg);
	return setting;
}

char *usr_name(int users) {
    DIR *path = opendir("../contest/code");
    struct dirent *user = readdir(path);
    if (user == NULL) {
        perror("DIR code doesn't exist");
        return NULL;
    }
    int i = 0;
    while (i < users) {
        user = readdir(path);
        if (user == NULL) {
            perror("DIR user doesn't exist");
            return NULL;
        }
        if (user -> d_name[0] == '.' || strcmp(user -> d_name, "user.cfg") == 0) {
            continue;
        }
        i++;
    }   
    char *name = malloc(strlen(user -> d_name) * sizeof(char));
    sprintf(name, "%s", user -> d_name); 
    return name;
}

char *make_fst_argv(char *users, int problems){
	char *name = malloc(15 + 1 + strlen(users) + 1);
	sprintf(name, "../contest/code/%s", users);
	DIR *path = opendir(name);
	struct dirent *problem = readdir(path);
	if (problem == NULL) {
		perror("Dir user doesn't exist");
		free(name);
		return NULL;
	}
	while (problem -> d_name[0] != 'A' + problems) {
		problem = readdir(path);
		if (problem == NULL) {
			free(name);
			char *tmp = malloc(1);
			tmp[0] = '\0';
			return tmp;
		}
	}
	name = realloc(name, strlen(name) + 1 + strlen(problem -> d_name) + 1);
	sprintf(name, "../contest/code/%s/%s\0", users, problem -> d_name);
	printf("%s\n", name);
	return name;
}

char *make_snd_argv(int problems){
	char test = 'A' + problems;
	char *name = malloc(19 * sizeof(char));	
	sprintf(name, "../contest/tests/%c", test);
	DIR *path = opendir(name);
	if (path == NULL) {
		perror("some of test A B C ... doesn't exist");
		free(name);
		return NULL;
	}
	printf("%s\n", name);
	return name;
}

int test(char ***setting, int user, int prob, int fd, int fd2) {
    char *correct_usr = usr_name(user);
    char tmp_log_wr[20];
    memset(tmp_log_wr, 32, 20);
    memcpy(tmp_log_wr, correct_usr, strlen(correct_usr));
    write(fd, tmp_log_wr, 20);
    if (correct_usr == NULL){
        return -1;
    }
    for (int i = 0; i < prob; i++) {
    	memset(tmp_log_wr, 32, 4);
    	char *argv1 = make_fst_argv(correct_usr, i);
    	if (argv1 == NULL) {
    		return -1;
    	}
    	if (argv1[0] == '\0') {
            tmp_log_wr[0] = 'X';
            write(fd, tmp_log_wr, 4);
            free(argv1);
			continue;
    	}
    	char *argv2 = make_snd_argv(i);
    	if (argv2 == NULL) {
    		free(argv1);
    		return -1;
    	}
        int pipe_fd[2];
        pipe(pipe_fd);
        pid_t pid;
        if ((pid = fork()) == -1){
            perror("fork failed");
            free(argv1);
            free(argv2);
            return -1;
        } else if (pid == 0) {
            dup2(pipe_fd[1], 1);
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            close(fd);
            if (execlp("./test", "./test", argv1, argv2, NULL)) {
            	free(argv1);
            	free(argv2);
            	perror("exec failed : programm test is down");
            	_exit(1);
            }
        } else {
/*//        	int status;
            close(pipe_fd[1]);
        	wait(NULL);
//        	if (WEXITSTATUS(status)) {
//        		return 1;
//        	}
/*            long int ttime = time(NULL);
            char *tttime = ctime(&ttime);
            write(fd, tttime, sizeof(tttime));
            write(fd, " start test\n", 12);*/
           // wait(NULL);//wtf is this???
/*            char *info = NULL, ch;
            int i;
            write(fd, tttime, sizeof(tttime));
            write(fd, " user: ", 7);
            write(fd, setting[user][1], sizeof(setting[user][1]));
            write(fd, ", problem :", 11);
            char pr = 'A' + prob;
            write(fd, &pr, sizeof(char));
            write(fd, ", tested :", 10);
            write(fd, &i, sizeof(int));
            for(i = 0; read(pipe_fd[0], &ch, 1) > 0; i++) {
                info = (char *)realloc(info, i + 1);
                info[i] = ch;
            }
            write(fd, "\n", 1);
            ttime = time(NULL);
            tttime = ctime(&ttime);
            write(fd, tttime, sizeof(tttime));
            write(fd, info, sizeof(info));
            write(fd, "\n", 1);
            ttime = time(NULL);
            tttime = ctime(&ttime);
            write(fd, tttime, sizeof(tttime));
            int fail = 0, accept = 0;
            for(int j = 0; j < i; j++) {
                if(info[j] == 'x') {
                    fail++;
                } else {
                    accept++;
                }
            }
            write(fd, " accepted: ", 11);
            write(fd, &accept, sizeof(int));
            write(fd, ", failed: ", 10);
            write(fd, &fail, sizeof(int));
            write(fd, "\n", 1);
            ttime = time(NULL);
            tttime = ctime(&ttime);
            write(fd, tttime, sizeof(tttime));
            write(fd, " stop test", 10);*/
/*            char flag = '+', answer = '+';
            int done = read(pipe_fd[0], &flag, 1);
        	if (flag == 'X'){
        		answer = 'X';
            } else {
                while (done > 0 && flag != '\n') {
                	if (flag != '+') {
                		answer = '-';
                		while (read(pipe_fd[0], &flag, 1) > 0);
                		break;
                	}
                	done = read(pipe_fd[0], &flag, 1);
                }
            }
            tmp_log_wr[0] = answer;
            write(fd, tmp_log_wr, 4);
            close(pipe_fd[0]);
            free(argv1);
            free(argv2);*/
		// int status;
			close(pipe_fd[1]);
			// if (WEXITSTATUS(status)) {
			// return 1;
			// }
			long int ttime = time(NULL);
			char *tttime = ctime(&ttime);
			write(fd2, tttime, strlen(tttime) - 1);
			write(fd2, " start test\n", 12);
			wait(NULL);
			char *info = NULL;
			int j = 0;
			char flag = '+', answer = '+';
			int done = read(pipe_fd[0], &flag, 1);
			if (flag == 'X'){
				info = (char *)realloc(info, j + 1);
				info[j] = flag;
				j++;
				answer = 'X';
			} else {
				while (done > 0 && flag != '\n') {
					info = (char *)realloc(info, j + 1);
					info[j] = flag;
					j++;
					if (flag != '+') {
						answer = '-';
/*						while (read(pipe_fd[0], &flag, 1) > 0) {
							info = (char *)realloc(info, i + 1);
							info[i] = flag;
							i++;							
						}
						break;*/
					}
					done = read(pipe_fd[0], &flag, 1);
				}
			}
			info[j] = '\0';
			tmp_log_wr[0] = answer;
			write(fd, tmp_log_wr, 4);
			write(fd2, tttime, strlen(tttime) - 1);
			write(fd2, " user: ", 7);
			write(fd2, setting[user + 1][1], strlen(setting[user + 1][1]));
			write(fd2, ", problem :", 11);
			char pr = 'A' + i;
			write(fd2, &pr, sizeof(char));
			write(fd2, ", tested :", 10);
			char ch = '0' + j;
			write(fd2, &ch, sizeof(char));
/*			for(i = 0; read(, &ch, 1) > 0; i++) {
			info = (char *)realloc(info, i + 1);
			info[i] = ch;
			}*/
			write(fd2, "\n", 1);
			ttime = time(NULL);
			tttime = ctime(&ttime);
			write(fd2, tttime, strlen(tttime) - 1);
			write(fd2, " ", 1);
			write(fd2, info, strlen(info));
			write(fd2, "\n", 1);
			ttime = time(NULL);
			tttime = ctime(&ttime);
			write(fd2, tttime, strlen(tttime) - 1);
			char fail = '0', accept = '0';
			if (j > 1) {
			for(int k = 0; k < j; k++) {
				if(info[k] == 'x') {
					fail++;
				} else {
					accept++;
				}
			}
			write(fd2, " accepted: ", 11);
			write(fd2, &accept, sizeof(char));
			write(fd2, ", failed: ", 10);
			write(fd2, &fail, sizeof(char));
			write(fd2, "\n", 1);
			} else {
			write(fd2, "program doesn't exist\n", 22);
			}
			ttime = time(NULL);
			tttime = ctime(&ttime);
			write(fd2, tttime, strlen(tttime) - 1);
			write(fd2, " stop test\n", 11);
			close(pipe_fd[0]);
			free(argv1);
			free(argv2);
		}
    }
    free(correct_usr);
    tmp_log_wr[0] = '\n';
    write(fd, tmp_log_wr, 1);
    return 0;
}

int main(int argc, char** argv) {
    char ***setting = NULL, ***problems = NULL;
    setting = read_conf("../contest/code", setting);
    problems = read_conf("../contest/tests", problems);
    int problem = atoi(problems[0][1]), users = atoi(setting[0][1]);
    int log = open("../contest/log/results.log", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    int log2 = open("../contest/log/results2.log", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    char tmp_log_wr[20] = "    users/problems  ";
    write(log, tmp_log_wr, 20 * sizeof(char));
    for (int i = 0; i < problem; i++) {
        tmp_log_wr[0] = 'A' + i;
        write(log, tmp_log_wr, sizeof(char) * 4);
    }
    tmp_log_wr[0] = '\n';
    write(log, tmp_log_wr, 1);
    for (int i = 0; i != users; i++) {
        if (test(setting, i, problem, log, log2) == -1) {
            char printerror[20] = "ERROR WITH usr/tests";
            write(1, printerror, sizeof(char) * 20);
            return -69;
        }
    }
    close(log);
    return 0;
}

/*struct dirent {
       ulong_t  d_ino;                  inode number of entry
       ushort_t d_reclen;               length of this record
       ushort_t d_namlen;               length of string in d_name
       char     d_name[MAXNAMLEN + 1];  maximum name length
};*/

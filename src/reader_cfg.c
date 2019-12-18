#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

char *get_word(char *end, int fd) {
    char *word = NULL;
    int i = 0;
    while(*end != '\n' && *end != '=') {
        if(read(fd, end, 1) == 0) {
            *end = '|';
            break;
        }
        if (*end == '=' || *end == '\n') {
            break;
        }
        word = (char *)realloc(word, i + 2);
        word[i] = *end;
        i++;
    }
    if (word != NULL) {
        word[i] = '\0';
    }
    return word;
}

void free_cfgs(char ***cfgs) {
    if (cfgs == NULL) {
        return;
    }
    for (int i = 0; cfgs[i] != NULL; i++) {
        for (int j = 0; cfgs[i][j] != NULL; j++) {
            free(cfgs[i][j]);
        }
        free(cfgs[i]);
    }
    free(cfgs);
}

char **get_list(char *end, int fd) {
    char **words = NULL;
    *end = '8';
    int i;
    for (i = 0; *end != '\n'; i++) {
        words = (char **)realloc(words, (i + 2) * sizeof(char *));
        if (words == NULL) {
            perror("realloc");
            return NULL;
        }
        words[i] = get_word(end, fd);
        if (*end == '=') {
            *end = ' ';
        }
        if (*end == '|' || *end == '\n') {
            if (*end != '|'){
                *end = ' ';
            }
            i++;
            words[i] = NULL;
            break;
        }
        if (words[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(words[j]);
            }
            free(words);
            return NULL;
        }
    }
    return words;
}


char ***get_cfgs(const char *path) {
    char  ***cfgs = NULL, end = '\0';
    int i = 0, fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("file failed to open");
        return NULL;
    }
    while (end != '|') {
        cfgs = (char ***)realloc(cfgs, (i + 2) * sizeof(char **));
        if (cfgs == NULL) {
            perror("realloc char***");
            return NULL;
        }
        cfgs[i] = get_list(&end, fd);
        i++;
    }
    if (cfgs[i] != NULL) {
        cfgs[i] = NULL;
    }
    return cfgs;
}
/*
void print(char ***cmds) {
    int i, j;
    for (i = 0; cmds[i] != NULL; i++) {
        for (j = 0; cmds[i][j] != NULL; j++) {
            printf("%s ", cmds[i][j]);
        }
        printf("\n");
    }
}

int main(int argc, char **argv) {
    char ***cfg = NULL;
    cfg = get_cfgs(argv[1]);
    print(cfg);
    free_cfgs(cfg);
    return 0;
}*/

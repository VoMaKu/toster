#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "reader_cfg.h"

/* A line splits at its first '=' only, so a value is free to hold more of
   them — a compiler flag such as -std=c++17, say. */
static char *get_word(char *end, int fd, int split_here) {
    char *word = NULL;
    int i = 0;
    while(*end != '\n' && !(split_here && *end == '=')) {
        if(read(fd, end, 1) == 0) {
            *end = '|';
            break;
        }
        if ((split_here && *end == '=') || *end == '\n') {
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

static char **get_list(char *end, int fd) {
    char **words = NULL;
    *end = '8';
    int i;
    for (i = 0; *end != '\n'; i++) {
        char **grown = (char **)realloc(words, (i + 2) * sizeof(char *));
        if (grown == NULL) { // assigning straight back to words would have lost the rows already read
            perror("realloc");
            for (int j = 0; j < i; j++) {
                free(words[j]);
            }
            free(words);
            return NULL;
        }
        words = grown;
        words[i] = get_word(end, fd, i == 0);
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
        perror(path);
        return NULL;
    }
    while (end != '|') {
        char ***grown = (char ***)realloc(cfgs, (i + 2) * sizeof(char **));
        if (grown == NULL) {
            perror("realloc char***");
            cfgs[i] = NULL;
            free_cfgs(cfgs);
            close(fd);
            return NULL;
        }
        cfgs = grown;
        cfgs[i] = get_list(&end, fd);
        i++;
    }
    cfgs[i] = NULL; // unconditionally: the slot has never been written, so testing it first read uninitialised memory
    close(fd);
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

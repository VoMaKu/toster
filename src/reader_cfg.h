#ifndef READER_CFG_H
#define READER_CFG_H

/* Reads a .cfg file into a NULL-terminated array of NULL-terminated string
   arrays: one row per line, one string per "key=value" side. */
char ***get_cfgs(const char *path);

/* Frees what get_cfgs returned. */
void free_cfgs(char ***cfgs);

#endif

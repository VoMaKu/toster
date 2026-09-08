#ifndef READER_JSON_H
#define READER_JSON_H

typedef enum {
	JSON_NULL,
	JSON_BOOL,
	JSON_NUMBER,
	JSON_STRING,
	JSON_ARRAY,
	JSON_OBJECT
} JsonType;

/* Members keep the order they were written in, so an object can describe a
   list whose order matters. */
typedef struct Json {
	JsonType type;
	double number;      /* JSON_NUMBER, and 0 or 1 for JSON_BOOL */
	char *string;       /* JSON_STRING */
	struct Json **items;
	char **keys;        /* JSON_OBJECT only, one per item */
	int count;
} Json;

/* Reads and parses a whole file. Prints where it gave up and returns NULL. */
Json *json_parse_file(const char *path);
void json_free(Json *json);

Json *json_get(Json *object, const char *key);   /* NULL when absent */
Json *json_at(Json *array, int index);           /* NULL when out of range */
const char *json_string(Json *json);             /* NULL unless a string */
long json_int(Json *json, long fallback);

#endif

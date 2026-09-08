#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reader_json.h"

typedef struct {
	const char *text;
	const char *p;
	const char *path;
} Parser;

static Json *parse_value(Parser *parser);

static void fail(Parser *parser, const char *what) {
	int line = 1, column = 1;
	for (const char *c = parser->text; c < parser->p; c++) {
		if (*c == '\n') {
			line++;
			column = 1;
		} else {
			column++;
		}
	}
	fprintf(stderr, "%s:%d:%d: %s\n", parser->path, line, column, what);
}

static void skip_space(Parser *parser) {
	while (*parser->p == ' ' || *parser->p == '\t' || *parser->p == '\n' || *parser->p == '\r') {
		parser->p++;
	}
}

static Json *json_new(JsonType type) {
	Json *json = calloc(1, sizeof(Json));
	if (json != NULL) {
		json->type = type;
	}
	return json;
}

void json_free(Json *json) {
	if (json == NULL) {
		return;
	}
	for (int i = 0; i < json->count; i++) {
		json_free(json->items[i]);
		if (json->keys != NULL) {
			free(json->keys[i]);
		}
	}
	free(json->items);
	free(json->keys);
	free(json->string);
	free(json);
}

static int append(Json *parent, char *key, Json *child) {
	Json **items = realloc(parent->items, (parent->count + 1) * sizeof(Json *));
	if (items == NULL) {
		return -1;
	}
	parent->items = items;
	if (parent->type == JSON_OBJECT) {
		char **keys = realloc(parent->keys, (parent->count + 1) * sizeof(char *));
		if (keys == NULL) {
			return -1;
		}
		parent->keys = keys;
		parent->keys[parent->count] = key;
	}
	parent->items[parent->count] = child;
	parent->count++;
	return 0;
}

static int put_utf8(char **write, unsigned long code) { // one code point in the encoding the rest of the file is written in
	unsigned char *w = (unsigned char *)*write;
	if (code < 0x80) {
		*w++ = code;
	} else if (code < 0x800) {
		*w++ = 0xC0 | (code >> 6);
		*w++ = 0x80 | (code & 0x3F);
	} else if (code < 0x10000) {
		*w++ = 0xE0 | (code >> 12);
		*w++ = 0x80 | ((code >> 6) & 0x3F);
		*w++ = 0x80 | (code & 0x3F);
	} else {
		*w++ = 0xF0 | (code >> 18);
		*w++ = 0x80 | ((code >> 12) & 0x3F);
		*w++ = 0x80 | ((code >> 6) & 0x3F);
		*w++ = 0x80 | (code & 0x3F);
	}
	*write = (char *)w;
	return 0;
}

static int hex4(Parser *parser, unsigned long *out) {
	*out = 0;
	for (int i = 0; i < 4; i++) {
		char c = parser->p[i];
		int digit;
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			digit = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			digit = c - 'A' + 10;
		} else {
			return -1;
		}
		*out = *out * 16 + digit;
	}
	parser->p += 4;
	return 0;
}

static char *parse_string(Parser *parser) {
	if (*parser->p != '"') {
		fail(parser, "expected a string");
		return NULL;
	}
	parser->p++;
	/* No escape ever grows: \uXXXX is six bytes in and at most four out. */
	char *out = malloc(strlen(parser->p) + 1);
	if (out == NULL) {
		return NULL;
	}
	char *write = out;
	while (*parser->p != '"') {
		if (*parser->p == '\0') {
			fail(parser, "the string is never closed");
			free(out);
			return NULL;
		}
		if (*parser->p != '\\') {
			*write++ = *parser->p++;
			continue;
		}
		parser->p++;
		char escape = *parser->p++;
		switch (escape) {
		case '"': *write++ = '"'; break;
		case '\\': *write++ = '\\'; break;
		case '/': *write++ = '/'; break;
		case 'b': *write++ = '\b'; break;
		case 'f': *write++ = '\f'; break;
		case 'n': *write++ = '\n'; break;
		case 'r': *write++ = '\r'; break;
		case 't': *write++ = '\t'; break;
		case 'u': {
			unsigned long code;
			if (hex4(parser, &code) != 0) {
				fail(parser, "\\u needs four hexadecimal digits");
				free(out);
				return NULL;
			}
			if (code >= 0xD800 && code <= 0xDBFF && parser->p[0] == '\\' && parser->p[1] == 'u') {
				unsigned long low;
				parser->p += 2;
				if (hex4(parser, &low) != 0 || low < 0xDC00 || low > 0xDFFF) {
					fail(parser, "a high surrogate needs a low one after it");
					free(out);
					return NULL;
				}
				code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
			}
			put_utf8(&write, code);
			break;
		}
		default:
			fail(parser, "unknown escape");
			free(out);
			return NULL;
		}
	}
	parser->p++;
	*write = '\0';
	return out;
}

static Json *parse_object(Parser *parser) {
	Json *object = json_new(JSON_OBJECT);
	if (object == NULL) {
		return NULL;
	}
	parser->p++;
	skip_space(parser);
	if (*parser->p == '}') {
		parser->p++;
		return object;
	}
	for (;;) {
		skip_space(parser);
		char *key = parse_string(parser);
		if (key == NULL) {
			json_free(object);
			return NULL;
		}
		skip_space(parser);
		if (*parser->p != ':') {
			fail(parser, "expected ':' after the name");
			free(key);
			json_free(object);
			return NULL;
		}
		parser->p++;
		Json *value = parse_value(parser);
		if (value == NULL || append(object, key, value) != 0) {
			free(key);
			json_free(value);
			json_free(object);
			return NULL;
		}
		skip_space(parser);
		if (*parser->p == ',') {
			parser->p++;
			continue;
		}
		if (*parser->p == '}') {
			parser->p++;
			return object;
		}
		fail(parser, "expected ',' or '}'");
		json_free(object);
		return NULL;
	}
}

static Json *parse_array(Parser *parser) {
	Json *array = json_new(JSON_ARRAY);
	if (array == NULL) {
		return NULL;
	}
	parser->p++;
	skip_space(parser);
	if (*parser->p == ']') {
		parser->p++;
		return array;
	}
	for (;;) {
		Json *value = parse_value(parser);
		if (value == NULL || append(array, NULL, value) != 0) {
			json_free(value);
			json_free(array);
			return NULL;
		}
		skip_space(parser);
		if (*parser->p == ',') {
			parser->p++;
			continue;
		}
		if (*parser->p == ']') {
			parser->p++;
			return array;
		}
		fail(parser, "expected ',' or ']'");
		json_free(array);
		return NULL;
	}
}

static Json *parse_value(Parser *parser) {
	skip_space(parser);
	if (*parser->p == '{') {
		return parse_object(parser);
	}
	if (*parser->p == '[') {
		return parse_array(parser);
	}
	if (*parser->p == '"') {
		Json *json = json_new(JSON_STRING);
		if (json == NULL) {
			return NULL;
		}
		json->string = parse_string(parser);
		if (json->string == NULL) {
			json_free(json);
			return NULL;
		}
		return json;
	}
	if (strncmp(parser->p, "true", 4) == 0 || strncmp(parser->p, "false", 5) == 0) {
		Json *json = json_new(JSON_BOOL);
		if (json != NULL) {
			json->number = *parser->p == 't' ? 1 : 0;
			parser->p += *parser->p == 't' ? 4 : 5;
		}
		return json;
	}
	if (strncmp(parser->p, "null", 4) == 0) {
		parser->p += 4;
		return json_new(JSON_NULL);
	}
	char *end = NULL;
	double number = strtod(parser->p, &end);
	if (end == parser->p) {
		fail(parser, "expected a value");
		return NULL;
	}
	parser->p = end;
	Json *json = json_new(JSON_NUMBER);
	if (json != NULL) {
		json->number = number;
	}
	return json;
}

Json *json_parse_file(const char *path) {
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		perror(path);
		return NULL;
	}
	char *text = NULL;
	size_t size = 0;
	for (;;) {
		char *grown = realloc(text, size + 4097);
		if (grown == NULL) {
			free(text);
			fclose(file);
			return NULL;
		}
		text = grown;
		size_t got = fread(text + size, 1, 4096, file);
		size += got;
		if (got < 4096) {
			break;
		}
	}
	fclose(file);
	text[size] = '\0';
	Parser parser = { text, text, path };
	Json *json = parse_value(&parser);
	if (json != NULL) {
		skip_space(&parser);
		if (*parser.p != '\0') {
			fail(&parser, "trailing text after the value");
			json_free(json);
			json = NULL;
		}
	}
	free(text);
	return json;
}

Json *json_get(Json *object, const char *key) {
	if (object == NULL || object->type != JSON_OBJECT) {
		return NULL;
	}
	for (int i = 0; i < object->count; i++) {
		if (strcmp(object->keys[i], key) == 0) {
			return object->items[i];
		}
	}
	return NULL;
}

Json *json_at(Json *array, int index) {
	if (array == NULL || index < 0 || index >= array->count) {
		return NULL;
	}
	return array->items[index];
}

const char *json_string(Json *json) {
	return json != NULL && json->type == JSON_STRING ? json->string : NULL;
}

long json_int(Json *json, long fallback) {
	return json != NULL && json->type == JSON_NUMBER ? (long)json->number : fallback;
}

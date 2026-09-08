CC = gcc
CFLAGS = -Wall -Wextra -O2
SOURCES = src/config.c src/reader_cfg.c src/reader_json.c
HEADERS = src/config.h src/reader_cfg.h src/reader_json.h

all: bin/test bin/judge

bin/test: src/test.c $(SOURCES) $(HEADERS)
	mkdir -p bin contest/log contest/tmp
	$(CC) $(CFLAGS) src/test.c $(SOURCES) -o bin/test

bin/judge: src/judge.c $(SOURCES) $(HEADERS)
	mkdir -p bin contest/log contest/tmp
	$(CC) $(CFLAGS) src/judge.c $(SOURCES) -o bin/judge

check: all
	sh test/run-tests.sh

clean:
	rm -rf bin contest/tmp

.PHONY: all check clean

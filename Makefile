all:
	mkdir -p bin tmp
	gcc src/test.c -o bin/test
	gcc src/judge.c src/reader_cfg.c -o bin/judge

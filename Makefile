
CFLAGS=

cat: cat.c

clean:
	rm -rf cat *.gcno *.gcda *.gcov

test: cat
	python3 ./characterization/test.py

coverage: CFLAGS += --coverage
coverage: clean cat test
	gcov cat
	
cat.c: Makefile
	touch cat.c
# LAWD Makefile

CFLAGS = -g -std=c99 -pedantic -Wconversion -Wall -I include
CC = gcc

# error.h
build/lawd/error.o : source/lawd/error.c include/lawd/error.h
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_error : tests/lawd/error.c \
	build/lawd/error.o \
	lib/libselc.a
	$(CC) $(CFLAGS) -o $@ $^
grind_test_error : bin/test_error
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# safemem.h 
build/lawd/safemem.o : source/lawd/safemem.c include/lawd/safemem.h
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_safemem : tests/lawd/safemem.c \
	build/lawd/safemem.o \
	build/lawd/error.o \
	lib/libselc.a
	$(CC) $(CFLAGS) -o $@ $^
grind_test_safemem : bin/test_safemem
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# coroutine.h 
build/lawd/cor_x86_64.o : source/lawd/cor_x86_64.c include/lawd/coroutine.h 
	$(CC) $(CFLAGS) -c -o $@ $<
build/lawd/cor_x86_64s.o : source/lawd/cor_x86_64.s source/lawd/cor_x86_64.c
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_coroutine : tests/lawd/coroutine.c \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/safemem.o \
	lib/libselc.a
	$(CC) $(CFLAGS) -o $@ $^
grind_test_coroutine : bin/test_coroutine
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# server.h
build/lawd/server.o : source/lawd/server.c include/lawd/server.h
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_server : tests/lawd/server.c \
	build/lawd/error.o \
	build/lawd/server.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/safemem.o \
	lib/libselc.a 
	$(CC) $(CFLAGS) -o $@ $^
grind_test_server : bin/test_server
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null
bin/test_server_echo: tests/lawd/server_echo.c \
	build/lawd/error.o \
	build/lawd/server.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/safemem.o \
	lib/libselc.a 
	$(CC) $(CFLAGS) -o $@ $^

# util.h
build/lawd/util.o : source/lawd/util.c include/lawd/util.h
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_util : tests/lawd/util.c \
	build/lawd/util.o \
	lib/libselc.a \
	lib/libpgenc.a 
	$(CC) $(CFLAGS) -o $@ $^
grind_test_util : bin/test_util
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# uri.h 
tmp/lawd/uri_parsers.c : grammar/uri.g 
	bin/pgenc -g $< -s $@ -d law_uri_parsers
build/lawd/uri.o : source/lawd/uri.c 
	$(CC) $(CFLAGS) -c -o $@ $<
build/lawd/uri_parsers.o : tmp/lawd/uri_parsers.c 
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_uri : tests/lawd/uri.c \
	build/lawd/uri_parsers.o \
	build/lawd/uri.o \
	lib/libselc.a \
	lib/libpgenc.a 
	$(CC) $(CFLAGS) -o $@ $^

# http.h
tmp/lawd/http_parsers.c : grammar/http.g 
	bin/pgenc -g $< -s $@ -d law_http_parsers
build/lawd/http_parsers.o : tmp/lawd/http_parsers.c
	$(CC) $(CFLAGS) -c -o $@ $<

# test suite
suite: \
	grind_test_error \
	grind_test_safemem \
	grind_test_coroutine \
	grind_test_server \
	grind_test_util

clean:
	rm build/lawd/*.o || true
	rm bin/test_* || true 
	rm tmp/lawd/uri_parsers.c || true 
	rm tmp/lawd/http_parsers.c || true


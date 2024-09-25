# LAWD Makefile

CFLAGS = -g -std=c99 -pedantic -Wconversion -Wall -I include
CC = gcc


../selc/lib/libselc.a: 
	make -C ../selc lib/libselc.a 

lib/libselc.a: ../selc/lib/libselc.a 
	ln --force -s ../$< $@ 

../pgenc/lib/libpgenc.a: 
	make -C ../pgenc lib/libpgenc.a 

lib/libpgenc.a: ../pgenc/lib/libpgenc.a  
	ln --force -s ../$< $@

include/selc: 
	ln --force -s ../../selc/include/selc $@

include/pgenc: 
	ln --force -s ../../pgenc/include/pgenc $@

../pgenc/bin/pgenc: 
	make -C ../pgenc bin/pgenc 

bin/pgenc: ../pgenc/bin/pgenc 
	ln --force -s ../$< $@

includes: include/selc include/pgenc 

# error.h
build/lawd/error.o : source/lawd/error.c include/lawd/error.h includes
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

# time.h
build/lawd/time.o: source/lawd/time.c include/lawd/time.h 
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_time: tests/lawd/time.c \
	build/lawd/time.o
	$(CC) $(CFLAGS) -o $@ $^

# log.h
build/lawd/log.o: source/lawd/log.c include/lawd/log.h 
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_log: tests/lawd/log.c \
	build/lawd/log.o \
	build/lawd/time.o \
	lib/libselc.a
	$(CC) $(CFLAGS) -o $@ $^
	
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

# echo server 
bin/echo: tests/lawd/echo.c \
	build/lawd/error.o \
	build/lawd/server.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/safemem.o \
	lib/libselc.a 
	$(CC) $(CFLAGS) -o $@ $^


# uri.h 
tmp/lawd/uri_parsers.c : grammar/uri.g bin/pgenc
	bin/pgenc -g $< -s $@ -d law_uri_parsers
build/lawd/uri.o : source/lawd/uri.c include/lawd/uri.h includes
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
tmp/lawd/http_parsers.c : grammar/http.g bin/pgenc
	bin/pgenc -g $< -s $@ -d law_ht_parsers
build/lawd/http_parsers.o : tmp/lawd/http_parsers.c includes
	$(CC) $(CFLAGS) -c -o $@ $<
build/lawd/http.o : source/lawd/http.c 
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_http: tests/lawd/http.c \
	build/lawd/http_parsers.o \
	build/lawd/uri_parsers.o \
	build/lawd/http.o \
	build/lawd/uri.o \
	build/lawd/safemem.o \
	lib/libselc.a \
	lib/libpgenc.a
	$(CC) $(CFLAGS) -o $@ $^

lib/liblawd.a : \
	build/lawd/error.o \
	build/lawd/safemem.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/server.o \
	build/lawd/uri_parsers.o \
	build/lawd/uri.o \
	build/lawd/http_parsers.o \
	build/lawd/http.o 
	ar -crs $@ $^

# hello world http server
bin/hello : tests/lawd/hello.c \
	lib/liblawd.a \
	lib/libpgenc.a \
	lib/libselc.a 
	$(CC) $(CFLAGS) -o $@ $^ -lssl -lcrypto

# test suite
suite: \
	grind_test_error \
	grind_test_safemem \
	grind_test_coroutine \
	grind_test_server \
	grind_test_util

tmp/key.pem:
	openssl genrsa -out $@

tmp/csr.pem: tmp/key.pem
	openssl req -new -key $< -out $@

tmp/cert.pem:  tmp/csr.pem
	openssl x509 -req -days 365 -in $< -signkey tmp/key.pem -out $@

clean:
	rm build/lawd/*.o || true
	rm bin/test_* || true 
	rm tmp/lawd/uri_parsers.c || true 
	rm tmp/lawd/http_parsers.c || true
	rm lib/liblawd.a || true
	rm lib/libselc.a || true 
	rm lib/libpgenc.a || true 
	rm include/pgenc || true 
	rm include/selc || true
	rm bin/pgenc || true
	rm bin/echo || true 
	rm bin/hello || true
	

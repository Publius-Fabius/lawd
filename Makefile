# LAWD Makefile

CFLAGS = -g -std=c11 -pedantic -Wconversion -Wall -I include
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
build/lawd/log.o: source/lawd/log.c include/lawd/log.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_log: tests/lawd/log.c \
	build/lawd/log.o \
	build/lawd/time.o \
	lib/libselc.a
	$(CC) $(CFLAGS) -o $@ $^

# cqueue.h
build/lawd/cqueue.o: source/lawd/cqueue.c include/lawd/cqueue.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_cqueue: tests/lawd/cqueue.c \
	build/lawd/cqueue.o \
	lib/libselc.a
	$(CC) $(CFLAGS) -o $@ $^
grind_test_cqueue : bin/test_cqueue
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# priority.h 
build/lawd/priority.o: source/lawd/priority.c include/lawd/priority.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_priority: tests/lawd/priority.c \
	build/lawd/priority.o \
	lib/libselc.a
	$(CC) $(CFLAGS) -o $@ $^
grind_test_priority : bin/test_priority
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# pqueue.h
build/lawd/pqueue.o: source/lawd/pqueue.c include/lawd/pqueue.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_pqueue: tests/lawd/pqueue.c \
	build/lawd/pqueue.o \
	lib/libselc.a
	$(CC) $(CFLAGS) -o $@ $^
grind_test_pqueue : bin/test_pqueue
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# server.h
build/lawd/server.o : source/lawd/server.c include/lawd/server.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_server : tests/lawd/server.c \
	build/lawd/error.o \
	build/lawd/server.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/safemem.o \
	build/lawd/pqueue.o \
	build/lawd/cqueue.o \
	build/lawd/time.o \
	build/lawd/log.o \
	lib/libselc.a 
	$(CC) $(CFLAGS) -o $@ $^
grind_test_server : bin/test_server
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# ping server 
bin/ping: tests/lawd/ping.c \
	build/lawd/error.o \
	build/lawd/server.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/safemem.o \
	build/lawd/pqueue.o \
	build/lawd/time.o \
	build/lawd/log.o \
	lib/libpgenc.a \
	lib/libselc.a 
	$(CC) $(CFLAGS) -o $@ $^ -lssl

# uri.h 
tmp/lawd/uri_parsers.c : grammar/uri.g bin/pgenc
	bin/pgenc -g $< -s $@ -d law_uri_par
build/lawd/uri.o : source/lawd/uri.c include/lawd/uri.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
build/lawd/uri_parsers.o : tmp/lawd/uri_parsers.c 
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_uri : tests/lawd/uri.c \
	build/lawd/uri_parsers.o \
	build/lawd/uri.o \
	lib/libselc.a \
	lib/libpgenc.a 
	$(CC) $(CFLAGS) -o $@ $^ -lssl

# http/parse.h
build/lawd/http_parse.o: source/lawd/http/parse.c \
	include/lawd/http/parse.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
tmp/lawd/http_parsers.c : grammar/http.g bin/pgenc
	bin/pgenc -g $< -s $@ -d law_htp
build/lawd/http_parsers.o: tmp/lawd/http_parsers.c
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_http_parse: tests/lawd/http/parse.c \
	build/lawd/http_parse.o \
	build/lawd/http_parsers.o \
	build/lawd/uri_parsers.o \
	build/lawd/uri.o \
	lib/libselc.a \
	lib/libpgenc.a 
	$(CC) $(CFLAGS) -o $@ $^ -lssl

# http/headers.h
build/lawd/http_headers.o: source/lawd/http/headers.c \
	include/lawd/http/headers.h 
	$(CC) $(CFLAGS) -c -o $@ $<

# http/conn.h
build/lawd/http_conn.o: source/lawd/http/conn.c \
	include/lawd/http/conn.h 
	$(CC) $(CFLAGS) -c -o $@ $<

# http/server.h
build/lawd/http_server.o: source/lawd/http/server.c \
	include/lawd/http/server.h 
	$(CC) $(CFLAGS) -c -o $@ $<

# webd.h
build/lawd/webd.o: source/lawd/webd.c includes
	$(CC) $(CFLAGS) -c -o $@ $<

# websock.h 
build/lawd/websock.o: source/lawd/websock.c includes 
	$(CC) $(CFLAGS) -c -o $@ $<

# static lib
lib/liblawd.a : \
	build/lawd/error.o \
	build/lawd/safemem.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/pqueue.o \
	build/lawd/server.o \
	build/lawd/uri_parsers.o \
	build/lawd/uri.o \
	build/lawd/http_parse.o \
	build/lawd/http_parsers.o \
	build/lawd/http_server.o \
	build/lawd/http_conn.o \
	build/lawd/http_headers.o \
	build/lawd/time.o \
	build/lawd/log.o \
	build/lawd/webd.o \
	build/lawd/websock.o
	ar -crs $@ $^

bin/echo : tests/lawd/echo.c \
	lib/liblawd.a \
	lib/libpgenc.a \
	lib/libselc.a 
	$(CC) $(CFLAGS) -o $@ $^ -lssl -lcrypto
	
bin/test_websock: tests/lawd/websock.c \
	lib/liblawd.a \
	lib/libpgenc.a \
	lib/libselc.a 
	$(CC) $(CFLAGS) -o $@ $^ -lssl -lcrypto

# test suite
suite: \
	grind_test_error \
	grind_test_safemem \
	grind_test_coroutine \
	grind_test_cqueue \
	grind_test_pqueue \
	grind_test_server 

tmp/key.pem:
	openssl genrsa -out $@

tmp/pub.pem:
	openssl rsa -in tmp/key.pem -pubout -out tmp/pub.pem

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
	rm bin/ping || true 
	rm bin/hello || true
	

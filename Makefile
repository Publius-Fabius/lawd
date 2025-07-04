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

../pubmt/libpubmt.a:
	make -C ../pubmt libpubmt.a 

lib/libpubmt.a: ../pubmt/libpubmt.a 
	ln --force -s ../$< $@

include/selc: 
	ln --force -s ../../selc/include/selc $@

include/pgenc: 
	ln --force -s ../../pgenc/include/pgenc $@

include/pubmt:
	ln --force -s ../../pubmt/include/pubmt $@

../pgenc/bin/pgenc: 
	make -C ../pgenc bin/pgenc 

bin/pgenc: ../pgenc/bin/pgenc 
	ln --force -s ../$< $@

includes: include/selc include/pgenc include/pubmt

# error.h
build/lawd/error.o : source/lawd/error.c include/lawd/error.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_error : tests/lawd/error.c \
	build/lawd/error.o \
	lib/libselc.a
	$(CC) $(CFLAGS) -o $@ $^
run_test_error : bin/test_error
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# safemem.h 
build/lawd/safemem.o : source/lawd/safemem.c include/lawd/safemem.h
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_safemem : tests/lawd/safemem.c \
	build/lawd/safemem.o \
	build/lawd/error.o \
	lib/libselc.a
	$(CC) $(CFLAGS) -o $@ $^
run_test_safemem : bin/test_safemem
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
run_test_coroutine : bin/test_coroutine
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# time.h
build/lawd/time.o: source/lawd/time.c include/lawd/time.h 
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_time: tests/lawd/time.c \
	build/lawd/time.o \
	lib/libpubmt.a 
	$(CC) $(CFLAGS) -o $@ $^
run_test_time : bin/test_time
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# table.h
build/lawd/table.o: source/lawd/table.c include/lawd/table.h 
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_table: tests/lawd/table.c \
	build/lawd/table.o \
	lib/libpubmt.a 
	$(CC) $(CFLAGS) -o $@ $^
run_test_table : bin/test_table
	valgrind -q --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# log.h
build/lawd/log.o: source/lawd/log.c include/lawd/log.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_log: tests/lawd/log.c \
	build/lawd/log.o \
	build/lawd/time.o \
	build/lawd/error.o \
	lib/libselc.a \
	lib/libpubmt.a
	$(CC) $(CFLAGS) -o $@ $^ -lssl -lcrypto

# event.h 
build/lawd/event.o: source/lawd/event.c include/lawd/event.h includes
	$(CC) $(CFLAGS) -c -o $@ $<

# server.h
build/lawd/server.o : source/lawd/server.c include/lawd/server.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_server : tests/lawd/server.c \
	build/lawd/error.o \
	build/lawd/server.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/safemem.o \
	build/lawd/table.o \
	build/lawd/time.o \
	build/lawd/log.o \
	build/lawd/event.o \
	lib/libselc.a \
	lib/libpubmt.a
	$(CC) $(CFLAGS) -o $@ $^
run_test_server : bin/test_server
	valgrind -q --track-fds=yes --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# ping server 
bin/ping: tests/lawd/ping.c \
	build/lawd/error.o \
	build/lawd/server.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/safemem.o \
	build/lawd/table.o \
	build/lawd/time.o \
	build/lawd/event.o \
	lib/libselc.a \
	lib/libpubmt.a
	$(CC) $(CFLAGS) -o $@ $^ 

# uri.h 
tmp/lawd/uri_parsers.c : grammar/uri.g bin/pgenc
	bin/pgenc $@ law_uri_par $< 
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

# http_parser.h
build/lawd/http_parser.o: source/lawd/http_parser.c \
	include/lawd/http_parser.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
tmp/lawd/http_parsers.c : grammar/http.g bin/pgenc
	bin/pgenc $@ law_htp $< 
build/lawd/http_parsers.o: tmp/lawd/http_parsers.c
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_http_parser: tests/lawd/http_parser.c \
	build/lawd/http_parser.o \
	build/lawd/http_parsers.o \
	build/lawd/uri_parsers.o \
	build/lawd/uri.o \
	lib/libselc.a \
	lib/libpgenc.a 
	$(CC) $(CFLAGS) -o $@ $^

# http_headers.h
build/lawd/http_headers.o: source/lawd/http_headers.c includes \
	include/lawd/http_headers.h 
	$(CC) $(CFLAGS) -c -o $@ $<

# buffer.h
build/lawd/buffer.o : source/lawd/buffer.c include/lawd/buffer.h includes
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_buffer: tests/lawd/buffer.c \
	build/lawd/buffer.o \
	lib/libselc.a \
	lib/libpgenc.a 
	$(CC) $(CFLAGS) -o $@ $^ -lssl -lcrypto
run_test_buffer : bin/test_buffer
	valgrind -q --track-fds=yes --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# http_connection.h
build/lawd/http_conn.o: source/lawd/http_conn.c includes \
	include/lawd/http_conn.h
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_http_conn: tests/lawd/http_conn.c \
	build/lawd/http_conn.o \
	build/lawd/buffer.o \
	build/lawd/server.o \
	build/lawd/error.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/safemem.o \
	build/lawd/table.o \
	build/lawd/time.o \
	build/lawd/event.o \
	lib/libselc.a \
	lib/libpgenc.a \
	lib/libpubmt.a
	$(CC) $(CFLAGS) -o $@ $^ -lssl -lcrypto
run_test_http_conn : bin/test_http_conn
	valgrind -q --track-fds=yes --error-exitcode=1 --leak-check=full $^ 1>/dev/null

# http/server.h
build/lawd/http_server.o: source/lawd/http_server.c includes \
	include/lawd/http_server.h 
	$(CC) $(CFLAGS) -c -o $@ $<
bin/test_http_server: tests/lawd/http_server.c \
	build/lawd/http_server.o \
	build/lawd/http_conn.o \
	build/lawd/uri.o \
	build/lawd/uri_parsers.o \
	build/lawd/http_parser.o \
	build/lawd/http_parsers.o \
	build/lawd/buffer.o \
	build/lawd/server.o \
	build/lawd/error.o \
	build/lawd/cor_x86_64.o \
	build/lawd/cor_x86_64s.o \
	build/lawd/safemem.o \
	build/lawd/table.o \
	build/lawd/time.o \
	build/lawd/event.o \
	lib/libselc.a \
	lib/libpgenc.a \
	lib/libpubmt.a
	$(CC) $(CFLAGS) -o $@ $^ -lssl -lcrypto
run_test_http_server : bin/test_http_server
	valgrind -q --track-fds=yes --error-exitcode=1 --leak-check=full $^ 1>/dev/null

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
	run_test_error \
	run_test_safemem \
	run_test_coroutine \
	grind_test_cqueue \
	grind_test_pqueue \
	run_test_server 

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
	rm lib/libpubmt.a || true
	rm include/pgenc || true 
	rm include/selc || true
	rm include/pubmt || true
	rm bin/pgenc || true
	rm bin/ping || true 
	rm bin/hello || true
	

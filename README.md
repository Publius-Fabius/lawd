# LAWD - Lightweight Asynchronous Web Development

A toolkit for developing lightweight web applications.  This package uses libpgenc (also hosted on this account) for expression parsing. 

- lawd/error.h - Error codes and utility functions.
- lawd/coroutine.h - Launching and resuming coroutines.
- lawd/safemem.h - Wrapper for mmapping guarded memory.
- lawd/server.h - A protocol agnostic non-blocking server with coroutines.
- lawd/uri.h - URI parsing and data extraction.
- lawd/http.h - HTTP message parsing and client/server functionality.

An example of a basic HTTP server is provided.  The source code can be found at tests/lawd/hello.c.

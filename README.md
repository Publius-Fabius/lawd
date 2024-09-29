# LAWD - Lightweight API for Web Development

A toolkit for developing lightweight web applications.  This package uses libpgenc (also hosted on this account) for expression parsing. 

**Headers**

- lawd/error.h - Error codes and utility functions.
- lawd/coroutine.h - Launching and resuming coroutines.
- lawd/safemem.h - Wrapper for mmapping guarded memory.
- lawd/cqueue.h - A thread safe queue for network events.
- lawd/pqueue.h - A priority queue for timer events.
- lawd/log.h - Simple logging procedures.
- lawd/time.h - Simple time procedures.
- lawd/server.h - A multi-threaded server with non-blocking IO and coroutines.
- lawd/uri.h - URI parsing and data extraction.
- lawd/http.h - HTTP message parsing and client/server functionality.

**Server Architecture**

The server uses a multi-threaded model with coroutines.  A single thread 
accepts new socket connections, enqueueing a thread safe channel with a task.  
Any waiting worker threads are notified, dequeueing the task and dispatching 
it within a new coroutine environment with its own execution stack.  Each 
worker thread has its own internal scheduler for managing multiple coroutines 
at once. For socket event notification, the epoll system call is used, and
timeouts are managed with an efficient binary heap based priority queue.

**Example**

Host a simple ping server over TCP.  The code is available at tests/lawd/ping.c.

```
mkdir ~/clones
cd ~/clones

git clone https://github.com/Publius-Fabius/selc.git
git clone https://github.com/Publius-Fabius/pgenc.git
git clone https://github.com/Publius-Fabius/lawd.git

cd lawd
make bin/ping
sudo bin/ping
```

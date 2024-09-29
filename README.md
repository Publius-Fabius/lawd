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
- lawd/server.h - A protocol agnostic non-blocking server with coroutines.
- lawd/uri.h - URI parsing and data extraction.
- lawd/http.h - HTTP message parsing and client/server functionality.

**Server Architecture**

The server uses a multi-threaded model with coroutines.  A single thread
accepts new socket connections, enqueueing a thread safe channel with a 
task.  Any waiting worker threads are notified, dequeueing the task and
dispatching it within a new coroutine environment with its own execution 
stack. For socket event notification, the epoll system call is used. 
Timeouts are managed with an efficient binary heap based priority queue.

**Example**

An example of a simple ping server over TCP.

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

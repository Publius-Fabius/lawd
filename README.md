# LAWD - Lightweight API for Web Development

A toolkit for developing lightweight web applications.  This package uses 
libpgenc (also hosted on this account) for expression parsing.  Please be 
advised that this project is in its initial stages of development and 
probably won't work.

**Server Architecture**

The server uses a multi-threaded model with coroutines.  A single thread 
accepts new socket connections, using round robin load balancing to select 
a worker thread.  Once a waiting worker thread is notified, it dispatches 
the task within a new coroutine environment with its own execution stack. 
Each worker thread has its own internal scheduler for managing multiple 
coroutine environments at once. For socket event notification, the epoll 
system call is used, and timer events are handled with a binary heap.

**Memory Management**

All memory is allocated up front and pooled.  The server scales by its 
maximum number of concurrent tasks, which is determined by taking 
workers * worker_tasks.

**Example**

A simple ping test.

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

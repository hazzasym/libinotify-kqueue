#ifndef __WORKER_H__
#define __WORKER_H__

#include <pthread.h>
#include <stdint.h>
#include <sys/queue.h>
#include "worker-thread.h"
#include "worker-sets.h"


#define INOTIFY_FD 0
#define KQUEUE_FD  1


typedef struct {
    int kq;           /* kqueue descriptor */
    int io[2];        /* a socket pair */
    pthread_t thread; /* worker thread */
    worker_sets sets; /* kqueue events, filenames, etc */

    SIMPLEQ_HEAD(operations_queue, worker_cmd) queue;
    pthread_mutex_t queue_mutex;
} worker;


worker* worker_create        ();
void    worker_free          (worker *wrk);

int     worker_add_or_modify (worker *wrk, const char *path, uint32_t flags);
int     worker_remove_many   (worker *wrk, int *ids, int count);


#endif /* __WORKER_H__ */
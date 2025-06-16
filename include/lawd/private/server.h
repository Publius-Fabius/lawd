#ifndef LAWD_PRIVATE_SERVER_H
#define LAWD_PRIVATE_SERVER_H

#include "lawd/data.h"
#include <pthread.h>

typedef struct law_task law_task_t;

/** Message */
typedef struct law_msg {
        int type;
        law_data_t data;
} law_msg_t;

/** Message Queue */
typedef struct law_msg_queue {
        int fds[2];
} law_msg_queue_t;

/** Task Queue */
typedef struct law_task_queue {
        int fds[2];
} law_task_queue_t;

/** Task Ready Set */
typedef struct law_ready_set {
        law_task_t *list;
} law_ready_set_t;

/** Task Pool */
typedef struct law_task_pool {
        pthread_mutex_t lock;
        law_task_t *list;
        size_t capacity, size;
} law_task_pool_t;

typedef struct law_slot {
        law_id_t id;
        int8_t data;
} law_slot_t;

#endif
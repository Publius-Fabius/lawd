#ifndef LAWD_CQUEUE_H
#define LAWD_CQUEUE_H

#include "lawd/error.h"
#include <stdlib.h>
#include <pthread.h>

struct law_cq_node {
        void *value;
        struct law_cq_node *next;
};

struct law_cqueue {
        struct law_cq_node *head;
        struct law_cq_node *tail;
        pthread_mutex_t mutex;
        size_t size;
};

struct law_cqueue *law_cq_create() 
{
        struct law_cqueue *queue = malloc(sizeof(struct law_cqueue));
        if(!queue) {
                return NULL;
        }
        queue->head = queue->tail = NULL;
        queue->size = 0;
        pthread_mutex_init(&queue->mutex, NULL);
        return queue;
}

void law_cq_destroy(struct law_cqueue *queue) 
{
        if(!queue) {
                return;
        }
        for(struct law_cq_node *i = queue->head; i;) {
                struct law_cq_node *tmp = i;
                i = i->next;
                free(tmp);
        }
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
}

size_t law_cq_size(struct law_cqueue *queue)
{
        return queue->size;
}

struct law_cqueue *law_cq_enq(struct law_cqueue *queue, void *value) 
{
        SEL_ASSERT(queue);

        struct law_cq_node *new_node = malloc(sizeof(struct law_cq_node));
        if(!new_node) {
                return NULL;
        }

        new_node->value = value;
        new_node->next = NULL;

        pthread_mutex_lock(&queue->mutex);

        if(queue->tail) {
                queue->tail->next = new_node;
        } else {
                queue->head = new_node;
        }
        queue->tail = new_node;
        queue->size += 1;

        pthread_mutex_unlock(&queue->mutex);

        return queue;
}

struct law_cqueue *law_cq_deq(struct law_cqueue *queue, void **value) 
{
        SEL_ASSERT(queue);

        pthread_mutex_lock(&queue->mutex);

        if(queue->head == NULL) {
                pthread_mutex_unlock(&queue->mutex);
                return NULL;  
        }

        struct law_cq_node *node = queue->head;
        *value = node->value;

        if(queue->head == queue->tail) {
                queue->head = queue->tail = NULL;
        } else {
                queue->head = node->next;
        }
       
        queue->size -= 1;

        pthread_mutex_unlock(&queue->mutex);

        free(node);
        return queue;
}

#endif
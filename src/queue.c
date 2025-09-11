#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Initialize the priority queue
void queue_init(queue_t *q) {
    q->head = NULL;
    q->size = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

// Destroy the queue and free all memory
void queue_destroy(queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    queue_node_t *curr = q->head;
    while (curr) {
        queue_node_t *tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    q->head = NULL;
    q->size = 0;
    pthread_mutex_unlock(&q->mutex);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

// Insert a request into the queue in ascending order of filesize
void queue_push(queue_t *q, sha256_request_t *req, off_t filesize) {
    queue_node_t *node = malloc(sizeof(queue_node_t));
    if (!node) {
        fprintf(stderr, "[QUEUE] Error: malloc failed in queue_push\n");
        return;
    }
    node->req = *req;
    node->filesize = filesize;
    node->next = NULL;

    pthread_mutex_lock(&q->mutex);
    // Ordered insertion by filesize (smaller first)
    if (!q->head || filesize < q->head->filesize) {
        node->next = q->head;
        q->head = node;
    } else {
        queue_node_t *curr = q->head;
        while (curr->next && curr->next->filesize <= filesize) {
            curr = curr->next;
        }
        node->next = curr->next;
        curr->next = node;
    }
    q->size++;
    pthread_cond_signal(&q->cond); // Notify a waiting thread
    pthread_mutex_unlock(&q->mutex);
}

// Extract the request with the smallest filesize from the queue (blocking if empty)
int queue_pop(queue_t *q, sha256_request_t *req, off_t *filesize) {
    pthread_mutex_lock(&q->mutex);
    while (q->size == 0) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    queue_node_t *node = q->head;
    if (!node) {
        // This should not happen due to blocking wait, but handle for robustness
        fprintf(stderr, "[QUEUE] Warning: queue_pop called but queue is empty after wait\n");
        pthread_mutex_unlock(&q->mutex);
        return 0;
    }
    q->head = node->next;
    q->size--;
    *req = node->req;
    *filesize = node->filesize;
    free(node);
    pthread_mutex_unlock(&q->mutex);
    return 1;
}

// Returns 1 if the queue is empty, 0 otherwise
int queue_is_empty(queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    int empty = (q->size == 0);
    pthread_mutex_unlock(&q->mutex);
    return empty;
} 
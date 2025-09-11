#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "common.h"


// Queue node: stores a request, its filesize, and pointer to next node
typedef struct queue_node {
    sha256_request_t req;         // Request data
    off_t filesize;               // Size of the file for priority
    struct queue_node *next;      // Pointer to next node in the queue
} queue_node_t;


// Priority queue: linked list of requests, protected by mutex and condition variable
typedef struct {
    queue_node_t *head;           // Head of the linked list
    pthread_mutex_t mutex;        // Mutex for thread safety
    pthread_cond_t cond;          // Condition variable for waiting threads
    int size;                     // Number of elements in the queue
} queue_t;


// Initialize the queue
void init(queue_t *q);
// Destroy the queue and free all memory
void destroy(queue_t *q);
// Insert a request into the queue, ordered by filesize (ascending)
void push(queue_t *q, sha256_request_t *req, off_t filesize);
// Pop the request with the smallest filesize from the queue (blocking if empty)
int pop(queue_t *q, sha256_request_t *req, off_t *filesize);
// Returns 1 if the queue is empty, 0 otherwise
int is_empty(queue_t *q);

#endif // QUEUE_H 
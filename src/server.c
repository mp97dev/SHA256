#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>

#include "common.h"
#include "queue.h"
#include "cache.h"
#include "sha256_utils.h"


// Number of threads in the pool (can be changed)
#define THREAD_POOL_SIZE 4
// Sleep time in microseconds when no request is received
#define SERVER_IDLE_SLEEP_US 10000


// Global structures
queue_t request_queue; // Priority queue for requests
cache_t cache;         // Cache for file digests

// Function executed by each thread in the pool
void *worker_thread(void *arg) {
    while (1) {
        sha256_request_t req;
        off_t filesize;
        // Extract a request from the queue (blocking)
        queue_pop(&request_queue, &req, &filesize);

        // Handle cache query command
        if (strcmp(req.filepath, CACHE_QUERY_CMD) == 0) {
            // Print the cache to stdout (could also send via response FIFO)
            printf("[SERVER] Cache query from client %d\n", req.pid);
            cache_print(&cache);
            continue;
        }

        // Handle cache: look up or insert entry
        int is_new;
        cache_entry_t *entry = cache_lookup_or_insert(&cache, req.filepath, &is_new);

        if (entry == NULL) {
            // Failed to allocate or find cache entry, log error and skip
            fprintf(stderr, "[SERVER] Error: cache_lookup_or_insert returned NULL for file '%s'\n", req.filepath);
            continue;
        }

        pthread_mutex_lock(&entry->mutex);
        if (!is_new) {
            // Entry already present: if ready, respond immediately; if in progress, wait
            while (!entry->ready) {
                pthread_cond_wait(&entry->cond, &entry->mutex);
            }
            // Digest ready: send response (cache hit)
            pthread_mutex_unlock(&entry->mutex);
            char resp_fifo[PATH_MAX_LEN];
            snprintf(resp_fifo, sizeof(resp_fifo), FIFO_RESP_PREFIX "%d_fifo", req.pid);
            int fd_resp = open(resp_fifo, O_WRONLY);
            if (fd_resp == -1) {
                fprintf(stderr, "[SERVER] Error: could not open response FIFO '%s' for client %d\n", resp_fifo, req.pid);
            } else {
                sha256_response_t resp;
                strncpy(resp.digest, entry->digest, DIGEST_LEN);
                resp.status = 3; // cache hit
                ssize_t w = write(fd_resp, &resp, sizeof(resp));
                if (w != sizeof(resp)) {
                    fprintf(stderr, "[SERVER] Error: failed to write full response to client %d\n", req.pid);
                }
                close(fd_resp);
            }
            continue;
        }
        pthread_mutex_unlock(&entry->mutex);

        // New entry: compute digest
        char digest[DIGEST_LEN];
        int status = sha256_digest_file(req.filepath, digest);
        // Update the cache and notify any waiting threads
        cache_set_digest(entry, (status == 0) ? digest : "ERROR");

        // Send response to client
        char resp_fifo[PATH_MAX_LEN];
        snprintf(resp_fifo, sizeof(resp_fifo), FIFO_RESP_PREFIX "%d_fifo", req.pid);
        int fd_resp = open(resp_fifo, O_WRONLY);
        if (fd_resp == -1) {
            fprintf(stderr, "[SERVER] Error: could not open response FIFO '%s' for client %d\n", resp_fifo, req.pid);
        } else {
            sha256_response_t resp;
            strncpy(resp.digest, (status == 0) ? digest : "ERROR", DIGEST_LEN);
            resp.status = (status == 0) ? 0 : 1; // 0: success, 1: error
            ssize_t w = write(fd_resp, &resp, sizeof(resp));
            if (w != sizeof(resp)) {
                fprintf(stderr, "[SERVER] Error: failed to write full response to client %d\n", req.pid);
            }
            close(fd_resp);
        }
    }
    return NULL;
}

// Utility function: get the size of a file
static off_t get_filesize(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return st.st_size;
    return 0;
}

int main() {
    // Initialize structures
    queue_init(&request_queue);
    cache_init(&cache);

    // Create global request FIFO
    unlink(FIFO_REQ); // Remove if already exists
    if (mkfifo(FIFO_REQ, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo request");
        exit(1);
    }
    int fd_req = open(FIFO_REQ, O_RDONLY | O_NONBLOCK);
    if (fd_req == -1) {
        perror("open FIFO request");
        exit(1);
    }

    // Create thread pool
    pthread_t threads[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        int ret = pthread_create(&threads[i], NULL, worker_thread, NULL);
        if (ret != 0) {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(ret));
            exit(1);
        }
    }

    printf("[SERVER] Listening on %s\n", FIFO_REQ);

    // Main loop: receive requests and insert them into the queue
    while (1) {
        sha256_request_t req;
        ssize_t n = read(fd_req, &req, sizeof(req));
        if (n == sizeof(req)) {
            // Scheduling: calculate file size (if not a special command)
            off_t filesize = 0;
            if (strcmp(req.filepath, CACHE_QUERY_CMD) != 0) {
                filesize = get_filesize(req.filepath);
            }
            // Insert the request into the priority queue
            queue_push(&request_queue, &req, filesize);
        } else {
            // No request: wait briefly
            usleep(SERVER_IDLE_SLEEP_US); // 10 ms
        }
    }

    // Cleanup (never reached in practice)
    // NOTE: The following code is unreachable unless the server loop is exited.
    close(fd_req);
    queue_destroy(&request_queue);
    cache_destroy(&cache);
    unlink(FIFO_REQ);
    return 0;
} 
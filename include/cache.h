#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include "common.h"


/**
 * Cache entry node: stores file path, digest, status, and synchronization primitives
 */
typedef struct cache_entry {
    char filepath[PATH_MAX_LEN];      // Path to the file
    char digest[DIGEST_LEN];          // SHA-256 digest as hex string
    int ready;                        // 0 = calculation in progress, 1 = digest ready
    pthread_cond_t cond;              // Condition variable for waiting threads
    pthread_mutex_t mutex;            // Mutex for this entry
    struct cache_entry *next;         // Pointer to next entry in the list
} cache_entry_t;


/**
 * Global cache: linked list of entries, protected by a mutex
 */
typedef struct {
    cache_entry_t *head;              // Head of the linked list
    pthread_mutex_t mutex;            // Mutex for the cache
} cache_t;


// Initialize the cache
void cache_init(cache_t *cache);
// Destroy the cache and free all memory
void cache_destroy(cache_t *cache);
// Look up or insert an entry for the given file path; sets is_new=1 if created, 0 if found
cache_entry_t *cache_lookup_or_insert(cache_t *cache, const char *filepath, int *is_new);
// Set the digest for an entry and notify waiting threads
void cache_set_digest(cache_entry_t *entry, const char *digest);
// Get the digest for a file if ready; returns 1 if found and ready, 0 otherwise
int cache_get_digest(cache_t *cache, const char *filepath, char *digest_out);
// Print the state of the cache
void cache_print(cache_t *cache);

#endif
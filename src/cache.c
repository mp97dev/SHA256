#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Initialize the cache
void cache_init(cache_t *cache) {
    cache->head = NULL;
    pthread_mutex_init(&cache->mutex, NULL);
}

// Destroy the cache and free all memory
void cache_destroy(cache_t *cache) {
    pthread_mutex_lock(&cache->mutex);
    cache_entry_t *curr = cache->head;
    while (curr) {
        cache_entry_t *tmp = curr;
        curr = curr->next;
        pthread_mutex_destroy(&tmp->mutex);
        pthread_cond_destroy(&tmp->cond);
        free(tmp);
    }
    cache->head = NULL;
    pthread_mutex_unlock(&cache->mutex);
    pthread_mutex_destroy(&cache->mutex);
}

// Search for or insert an entry for filepath. If not found, create it (ready=0). is_new=1 if created now, 0 if already present.
cache_entry_t *cache_lookup_or_insert(cache_t *cache, const char *filepath, int *is_new) {
    pthread_mutex_lock(&cache->mutex);
    cache_entry_t *curr = cache->head;
    while (curr) {
        if (strcmp(curr->filepath, filepath) == 0) {
            *is_new = 0;
            pthread_mutex_unlock(&cache->mutex);
            return curr;
        }
        curr = curr->next;
    }
    // Not found: create new entry
    cache_entry_t *entry = malloc(sizeof(cache_entry_t));
    if (!entry) {
        fprintf(stderr, "[CACHE] Error: malloc failed in cache_lookup_or_insert\n");
        pthread_mutex_unlock(&cache->mutex);
        return NULL;
    }
    strncpy(entry->filepath, filepath, PATH_MAX_LEN - 1);
    entry->filepath[PATH_MAX_LEN - 1] = '\0'; // Ensure null-termination
    entry->digest[0] = '\0';
    entry->ready = 0;
    pthread_mutex_init(&entry->mutex, NULL);
    pthread_cond_init(&entry->cond, NULL);
    entry->next = cache->head;
    cache->head = entry;
    *is_new = 1;
    pthread_mutex_unlock(&cache->mutex);
    return entry;
}

// Set the digest and notify all threads waiting on this entry
void cache_set_digest(cache_entry_t *entry, const char *digest) {
    pthread_mutex_lock(&entry->mutex);
    strncpy(entry->digest, digest, DIGEST_LEN - 1);
    entry->digest[DIGEST_LEN - 1] = '\0'; // Ensure null-termination
    entry->ready = 1;
    pthread_cond_broadcast(&entry->cond); // Wake up all waiting threads
    pthread_mutex_unlock(&entry->mutex);
}

// Search for the digest in the cache. If found and ready, copy to digest_out and return 1. Otherwise 0.
int cache_get_digest(cache_t *cache, const char *filepath, char *digest_out) {
    pthread_mutex_lock(&cache->mutex);
    cache_entry_t *curr = cache->head;
    while (curr) {
        if (strcmp(curr->filepath, filepath) == 0) {
            pthread_mutex_lock(&curr->mutex);
            if (curr->ready) {
                strncpy(digest_out, curr->digest, DIGEST_LEN - 1);
                digest_out[DIGEST_LEN - 1] = '\0'; // Ensure null-termination
                pthread_mutex_unlock(&curr->mutex);
                pthread_mutex_unlock(&cache->mutex);
                return 1;
            }
            pthread_mutex_unlock(&curr->mutex);
            break;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&cache->mutex);
    return 0;
}

// Print the state of the cache (for queries)
void cache_print(cache_t *cache) {
    pthread_mutex_lock(&cache->mutex);
    printf("--- CACHE ---\n");
    cache_entry_t *curr = cache->head;
    while (curr) {
        printf("%s : %s [%s]\n", curr->filepath, curr->digest, curr->ready ? "READY" : "PENDING");
        curr = curr->next;
    }
    printf("--------------\n");
    pthread_mutex_unlock(&cache->mutex);
} 
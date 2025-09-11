#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#define PATH_MAX_LEN 256
#define DIGEST_LEN 65
#define FIFO_REQ "/tmp/sha256_req_fifo"
#define FIFO_RESP_PREFIX "/tmp/sha256_resp_"
#define CACHE_QUERY_CMD "CACHE?"
#define MAX_CLIENTS 64
#define MAX_THREADS 8

/**
 * Structure for client -> server request
 */
typedef struct {
    pid_t pid;                        // Process ID of the client
    char filepath[PATH_MAX_LEN];      // Path to the file to hash or query
} sha256_request_t;

// Structure for server -> client response
typedef struct {
    char digest[DIGEST_LEN];          // SHA-256 digest as hex string
    int status;                       // 0=ok, 1=error, 2=cache miss, 3=cache hit
} sha256_response_t;

#endif // COMMON_H 
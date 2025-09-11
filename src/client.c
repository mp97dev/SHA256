#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "common.h"

// Utility function: create the response FIFO for the client
void make_resp_fifo(char *fifo_path, pid_t pid) {
    snprintf(fifo_path, PATH_MAX_LEN, FIFO_RESP_PREFIX "%d_fifo", pid);
    unlink(fifo_path); // Remove if already exists
    if (mkfifo(fifo_path, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo response");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <file_path>\n", argv[0]);
        printf("       %s CACHE?     (to query the cache)\n", argv[0]);
        return 1;
    }

    pid_t pid = getpid();
    char resp_fifo[PATH_MAX_LEN];
    make_resp_fifo(resp_fifo, pid);

    // Prepara la richiesta
    sha256_request_t req;
    req.pid = pid;
    strncpy(req.filepath, argv[1], PATH_MAX_LEN - 1);
    req.filepath[PATH_MAX_LEN - 1] = '\0'; // Ensure null-termination

    // Invia la richiesta sulla FIFO globale
    int fd_req = open(FIFO_REQ, O_WRONLY);
    if (fd_req == -1) {
        perror("open FIFO request");
        unlink(resp_fifo);
        return 1;
    }
    ssize_t w = write(fd_req, &req, sizeof(req));
    if (w != sizeof(req)) {
        fprintf(stderr, "[CLIENT] Error: failed to write full request to server.\n");
        close(fd_req);
        unlink(resp_fifo);
        return 1;
    }
    close(fd_req);

    // Se è una interrogazione cache, non attende risposta
    if (strcmp(argv[1], CACHE_QUERY_CMD) == 0) {
        printf("[CLIENT] Cache query request sent.\n");
        unlink(resp_fifo);
        return 0;
    }

    // Attende la risposta sulla propria FIFO
    int fd_resp = open(resp_fifo, O_RDONLY);
    if (fd_resp == -1) {
        perror("open FIFO response");
        unlink(resp_fifo);
        return 1;
    }
    sha256_response_t resp;
    ssize_t n = read(fd_resp, &resp, sizeof(resp));
    if (n == sizeof(resp)) {
        if (resp.status == 0 || resp.status == 3) {
            printf("SHA-256: %s\n", resp.digest);
            if (resp.status == 3) printf("[CLIENT] (Response from cache)\n");
        } else {
            printf("Error in digest calculation or file not found.\n");
        }
    } else {
        printf("Error receiving response from server.\n");
    }
    close(fd_resp);
    unlink(resp_fifo);
    return 0;
} 
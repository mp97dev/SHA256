
#include <openssl/sha.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>


/**
 * Computes the SHA-256 digest of a file and outputs it as a hexadecimal string.
 * 
 * @param filename The path to the file to be hashed
 * @param out_digest A buffer to store the resulting hexadecimal digest. Must be at least 65 bytes long.
 * @return 0 on success, -1 on failure.
*/
int sha256_digest_file(const char *filename, char *out_digest) {
    const size_t HASH_SIZE = 32;
    const size_t BUFFER_SIZE = 4096;
    unsigned char hash[HASH_SIZE];
    SHA256_CTX ctx = {0};

    SHA256_Init(&ctx);
    char buffer[BUFFER_SIZE];
    int file = open(filename, O_RDONLY);
    if (file == -1) {
        perror("open");
        return -1;
    }
    ssize_t bytes_read;
    
    // Read the file in blocks and update the SHA256 context
    do {
        bytes_read = read(file, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            SHA256_Update(&ctx, buffer, bytes_read);
        } else if (bytes_read < 0) {
            perror("read");
            close(file);
            return -1;
        }
    } while (bytes_read > 0);

    SHA256_Final(hash, &ctx);
    close(file);

    // Convert the binary digest to a hexadecimal string
    for (size_t i = 0; i < HASH_SIZE; i++) {
        sprintf(out_digest + (i * 2), "%02x", hash[i]);
    }
    out_digest[HASH_SIZE * 2] = '\0';

    return 0;
} 
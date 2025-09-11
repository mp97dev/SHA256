#ifndef SHA256_UTILS_H
#define SHA256_UTILS_H

// Calcola la SHA-256 di un file e la restituisce come stringa esadecimale
// Ritorna 0 se ok, -1 se errore
int sha256_digest_file(const char *filename, char *out_digest);

#endif // SHA256_UTILS_H 
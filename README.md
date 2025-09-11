# SHA256 Client-Server Service

This project implements a client-server architecture for computing SHA-256 file hashes. The server receives file requests from clients, computes the SHA-256 digest, and returns the result. It also supports caching for repeated queries.

Features:
- **Client/Server in C**: Communicate via FIFO for file hash requests and cache queries.
- **Colorful Bash Test Script**: `run_test.sh` builds the project, runs the server and client, and compares the server's hash result with standard CLI tools (`sha256sum`, `shasum`, or `openssl`).
- **Flexible Testing**: Pass any file path to `run_test.sh` to test different files.
- **Cache Query**: The client can query the server's cache for previously computed hashes.

## Quick Start

```bash
./run_test.sh [path/to/file.txt]
```
If no file is provided, the default is `test/test_files/testfile.txt`.


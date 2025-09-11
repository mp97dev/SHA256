#!/bin/bash
# Test script to compile the project, and execute a test with `test/testfile.txt`
set -e

# Source utilities (colors, logging, sha256 helpers, cleanup)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
. "$SCRIPT_DIR/script/lib.sh"

SERVER_PID=""
trap cleanup EXIT # Needed to cleanup server at the end

log_step "CMake building"
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    log_info "Creating build directory: $BUILD_DIR"
    mkdir "$BUILD_DIR"
fi
cd "$BUILD_DIR"
cmake .. 
make -j"${MAKE_JOBS:-$(nproc 2>/dev/null || echo 1)}" > /dev/null
cd ..
log_success "Build completed"

log_step "Starting server"
./build/server &
SERVER_PID=$!
log_info "Server started with PID $SERVER_PID"

sleep 1

log_step "Running client test"

# Accept file path argument, default to test/testfile.txt
TEST_FILE="${1:-test/testfile.txt}"
if [ ! -f "$TEST_FILE" ]; then
    log_error "Test file not found: $TEST_FILE"
    exit 1
fi

log_info "Submitting file to server: $TEST_FILE"
CLIENT_OUT=$(./build/client "$TEST_FILE")
printf "%s\n" "$CLIENT_OUT" | sed 's/^/    /'

# Extract digest from client output
DIGEST_CLIENT=$(printf "%s\n" "$CLIENT_OUT" | sed -n 's/^SHA-256: \([0-9a-fA-F]\{64\}\).*/\1/p' | tr 'A-F' 'a-f')
if [ -z "$DIGEST_CLIENT" ]; then
    log_error "Failed to parse digest from client output"
    exit 1
fi

# Determine CLI SHA-256 implementation
get_cli_sha256() {
    local file="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file" | awk '{print $1}'
    elif command -v openssl >/dev/null 2>&1; then
        # Output format: SHA256(filename)= <hex> OR <hex> filename with -r
        openssl dgst -sha256 -r "$file" | awk '{print $1}'
    else
        return 127
    fi
}

CLI_DIGEST_RAW=$(get_cli_sha256 "$TEST_FILE" || true)
if [ -z "$CLI_DIGEST_RAW" ]; then
    log_warn "No CLI SHA-256 tool found (sha256sum/shasum/openssl). Skipping comparison."
else
    DIGEST_CLI=$(printf "%s" "$CLI_DIGEST_RAW" | tr 'A-F' 'a-f')
    if [ "$DIGEST_CLIENT" = "$DIGEST_CLI" ]; then
        log_success "Digest matches CLI: $DIGEST_CLIENT"
    else
        log_error "Digest mismatch"
        log_info  " client: $DIGEST_CLIENT"
        log_info  "    cli: $DIGEST_CLI"
        exit 1
    fi
fi

log_info "Querying cache"
./build/client CACHE?

log_success "Test completed"

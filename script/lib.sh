#!/usr/bin/env bash
# Common Bash utilities: colors, logging, sha256 helpers, and cleanup

# Enable colors only if stdout is a terminal with color support
if [ -t 1 ] && command -v tput >/dev/null 2>&1 && [ "$(tput colors 2>/dev/null || echo 0)" -ge 8 ]; then
    BOLD="\033[1m"; DIM="\033[2m"; RESET="\033[0m"
    RED="\033[31m"; GREEN="\033[32m"; YELLOW="\033[33m"; BLUE="\033[34m"; CYAN="\033[36m"
else
    BOLD=""; DIM=""; RESET=""; RED=""; GREEN=""; YELLOW=""; BLUE=""; CYAN=""
fi

log_step()    { printf "%b[➡] %s%b\n"   "$CYAN$BOLD" "$1" "$RESET"; }
log_info()    { printf "%b\n[ℹ] %s%b\n"   "$BLUE" "$1" "$RESET"; }
log_success() { printf "%b[✔] %s%b\n"   "$GREEN" "$1" "$RESET"; }
log_warn()    { printf "%b[⚠] %s%b\n"   "$YELLOW" "$1" "$RESET"; }
log_error()   { printf "%b[✖] %s%b\n"   "$RED$BOLD" "$1" "$RESET"; }

# Compute SHA-256 with available CLI tools
get_cli_sha256() {
    local file="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file" | awk '{print $1}'
    elif command -v openssl >/dev/null 2>&1; then
        # Output format: with -r it's '<hex>  filename'
        openssl dgst -sha256 -r "$file" | awk '{print $1}'
    else
        return 127
    fi
}

# Cleanup helper: expects SERVER_PID variable in caller's scope
cleanup() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        log_info "Stopping server (PID $SERVER_PID)"
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}

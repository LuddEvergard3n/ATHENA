#!/bin/bash
# ATHENA v1.1.2 CLI — Pre-compiled binary (Linux x86_64)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="$SCRIPT_DIR/../../athena-core/data"
export ATHENA_DATA_DIR="$DATA_DIR"
cd "$SCRIPT_DIR/../../athena-core"
exec "$SCRIPT_DIR/athena-cli" "$@"

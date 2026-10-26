#!/usr/bin/env bash
# Signal-safe container execution wrapper with pre-run cleanup & automatic CID tracking.
if [ -z "$CONTAINER_ENGINE" ]; then
    if command -v podman >/dev/null 2>&1; then
        CONTAINER_ENGINE="podman"
    else
        CONTAINER_ENGINE="docker"
    fi
fi
CONTAINER_NAME="syntropicos_run_$$"
CID_FILE="$(mktemp /tmp/syn_container_XXXXXX.cid)"
rm -f "$CID_FILE"

cleanup() {
    if [ -f "$CID_FILE" ]; then
        CID="$(cat "$CID_FILE" 2>/dev/null || true)"
        if [ -n "$CID" ]; then
            "$CONTAINER_ENGINE" rm -f "$CID" >/dev/null 2>&1 || true
        fi
        rm -f "$CID_FILE" 2>/dev/null || true
    fi
    "$CONTAINER_ENGINE" rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
}

trap cleanup EXIT INT TERM HUP

# Pre-execution sweep: kill any leftover/orphaned containers from interrupted host tasks
if [[ "$CONTAINER_ENGINE" == *podman* ]]; then
    podman ps -q --filter "image=localhost/syntropicos-test" | xargs -r podman rm -f >/dev/null 2>&1 || true
fi

"$CONTAINER_ENGINE" run --init --name "$CONTAINER_NAME" --cidfile "$CID_FILE" --rm "$@"
EXIT_CODE=$?
cleanup
exit $EXIT_CODE

#!/bin/bash

get_process_tree() {
    local pid=$1
    local pids=()

    # Get all child PIDs recursively
    get_children() {
        local parent=$1
        local children=$(pgrep -P $parent 2>/dev/null)

        for child in $children; do
            pids+=($child)
            get_children $child
        done
    }

    # Start with the given PID
    pids+=($pid)
    get_children $pid

    # Get resource usage for all PIDs
    if [ ${#pids[@]} -gt 0 ]; then
        for p in "${pids[@]}"; do
            if kill -0 $p 2>/dev/null; then
                ps -o pid,ppid,pcpu,pmem,vsz,rss,comm -p $p --no-headers 2>/dev/null
            fi
        done
    fi
}

# Check if PID argument is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <pid>"
    exit 1
fi

get_process_tree $1

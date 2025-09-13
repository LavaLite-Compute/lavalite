#!/bin/bash

# Simple OpenLava rsync script
# Copies installation from master to slaves with sudo on slaves

# ---------------------- start of customization ------------------------
LSF_ENVDIR=${LSF_ENVDIR:-/opt/openlava/etc}
INSTALL_ROOT=$(dirname "$LSF_ENVDIR")

# Set the name and the group of the openlava administrator
LAVA_ADMIN="david"
LAVA_GROUP="david"
# Set the hostfile with the name of the slave (sbatchd) hosts
HOSTFILE=./slaves
# ---------------------- end of customization ------------------------

DRYRUN=""
CONFIG_ONLY=0

usage() {
    cat <<EOF
Usage: $0 [-c] [-d] [-h] -f hostfile

Options:
  -c   Copy only configuration files (etc/)
  -d   Dry run (rsync -n)
  -h   Show this help
  -f   Hostfile with list of slave hosts
EOF
    exit 0
}

while getopts "cdf:h" opt; do
    case "$opt" in
        c) CONFIG_ONLY=1 ;;
        d) DRYRUN="-n" ;;
        f) HOSTFILE="$OPTARG" ;;
        h) usage ;;
        *) usage ;;
    esac
done

if [ ! -f "$HOSTFILE" ]; then
    echo "Hostfile not found: $HOSTFILE"
    exit 1
fi

# Directories to sync
if [ "$CONFIG_ONLY" -eq 1 ]; then
    DIRS="etc"
else
    DIRS="bin etc include lib sbin share"
fi

for host in $(cat "$HOSTFILE"); do
    # Always ensure logdir exists
    ssh "$host" "sudo mkdir -p $INSTALL_ROOT/log"
    for d in $DIRS; do
       echo "==> Syncing to $host directory $d"
       rsync -av $DRYRUN --delete --chown=$LAVA_ADMIN:$LAVA_GROUP \
           --rsync-path="sudo rsync" \
           $INSTALL_ROOT/$d "$host:$INSTALL_ROOT/$d/"
    done
done

exit 0

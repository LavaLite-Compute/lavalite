#!/bin/bash

# Simple LavaLite rsync script
# Pushes installation from master to slaves with sudo on slaves

# ---------------------- start of customization ------------------------

# Lavalite environment directory (must be set)
LAVALITE_ENVDIR=${LAVALITE_ENVDIR}
if [ -z "$LAVALITE_ENVDIR" ]; then
    echo "Error: LAVALITE_ENVDIR must be set (e.g., /opt/lavalite/etc)"
    exit 1
fi

INSTALL_ROOT=$(dirname "$LAVALITE_ENVDIR")

# Set the name and group of the Lavalite administrator
# As example use lavalite ll user
LAVALITE_ADMIN="ll"

# Validate LAVALITE_ADMIN exists
if ! id "$LAVALITE_ADMIN" &>/dev/null; then
    echo "Error: user '$LAVALITE_ADMIN' does not exist on this system."
    exit 1
fi
# As example use lavalite ll group
LAVALITE_GROUP="ll"
# Validate LAVALITE_GROUP exists
if ! getent group "$LAVALITE_GROUP" &>/dev/null; then
    echo "Error: group '$LAVALITE_GROUP' does not exist on this system."
    exit 1
fi

# Set the hostfile with the name of the slave (sbatchd) hosts
HOSTFILE="$LSF_ENVDIR/slaves"
# Validate HOSTFILE exists and is readable
if [ ! -r "$HOSTFILE" ]; then
    echo "Error: hostfile '$HOSTFILE' not found or not readable."
    exit 1
fi

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

LOCALSTATEDIR="$INSTALL_ROOT/var"

for host in $(cat "$HOSTFILE"); do
    echo "==> Preparing $host"

    # Ensure remote localstatedir exists
	ssh "$host" bash <<EOF
      sudo mkdir -p "$LOCALSTATEDIR/log"
      sudo mkdir -p "$LOCALSTATEDIR/work/info"
    EOF

   if [ $? -ne 0 ]; then
      echo "Error: failed to create directories on $host"
      continue
   fi

    for d in $DIRS; do
        if [ ! -d "$INSTALL_ROOT/$d" ]; then
            echo "Warning: local directory $INSTALL_ROOT/$d does not exist, skipping"
            continue
        fi

        echo "==> Syncing $d to $host"
        rsync -av $DRYRUN --delete --chown="$LAVALITE_ADMIN:$LAVALITE_GROUP" \
            --rsync-path="sudo rsync" \
            "$INSTALL_ROOT/$d/" "$host:$INSTALL_ROOT/$d/" || {
                echo "Error: rsync to $host:$INSTALL_ROOT/$d failed"
            }
    done
done

exit 0

#!/bin/sh
#
# LavaLite post-install setup.
# Must be run as root after 'make install'.
#
# Requires LL_CONF_DIR to be set in the environment.
#

set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "post-install: must be run as root" >&2
    exit 1
fi

if [ -z "$LL_CONF_DIR" ]; then
    echo "post-install: LL_CONF_DIR is not set" >&2
    exit 1
fi

if [ ! -f "$LL_CONF_DIR/ll.conf" ]; then
    echo "post-install: $LL_CONF_DIR/ll.conf not found" >&2
    exit 1
fi

MBD_USER=$(grep '^LL_MBD_USER' "$LL_CONF_DIR/ll.conf" | cut -d= -f2)
if [ -z "$MBD_USER" ]; then
    echo "post-install: LL_MBD_USER not found in ll.conf" >&2
    exit 1
fi

if ! getent passwd "$MBD_USER" > /dev/null 2>&1; then
    echo "post-install: user '$MBD_USER' does not exist" >&2
    exit 1
fi

MBD_GID=$(getent passwd "$MBD_USER" | cut -d: -f4)
MBD_GROUP=$(getent group "$MBD_GID" | cut -d: -f1)

PREFIX=$(cd "$LL_CONF_DIR/.." && pwd)
STATE_DIR=$PREFIX/var/state
LOG_DIR=$PREFIX/var/log

echo "post-install: mbd_user=$MBD_USER group=$MBD_GROUP"
echo "post-install: prefix=$PREFIX"
echo "post-install: state=$STATE_DIR log=$LOG_DIR"

# state and log directories
mkdir -p "$STATE_DIR/mbd"
mkdir -p "$STATE_DIR/sbd"
mkdir -p "$LOG_DIR"

chown "$MBD_USER:$MBD_GROUP" "$STATE_DIR/mbd"
chmod 750 "$STATE_DIR/mbd"

chown "$MBD_USER:$MBD_GROUP" "$LOG_DIR"
chmod 755 "$LOG_DIR"

# bhist: setgid so users can read the manifest via the API only
BHIST="$PREFIX/bin/bhist"
if [ ! -f "$BHIST" ]; then
    echo "post-install: $BHIST not found" >&2
    exit 1
fi

chown "root:$MBD_GROUP" "$BHIST"
chmod 2755 "$BHIST"

# llsign: setgid so users can sign their own requests without reading
# the client key, see llsign(8)
LLSIGN="$PREFIX/libexec/llsign"
if [ ! -f "$LLSIGN" ]; then
    echo "post-install: $LLSIGN not found" >&2
    exit 1
fi

chown "root:$MBD_GROUP" "$LLSIGN"
chmod 2755 "$LLSIGN"

# keys: the same files on every host, generated once and copied, so
# not generated here. ll.auth.key on every host, ll.daemon.key only
# where mbd or sbd runs.
if [ -f "$LL_CONF_DIR/ll.auth.key" ]; then
    chown "root:$MBD_GROUP" "$LL_CONF_DIR/ll.auth.key"
    chmod 640 "$LL_CONF_DIR/ll.auth.key"
else
    echo "post-install: $LL_CONF_DIR/ll.auth.key missing" >&2
fi

if [ -f "$LL_CONF_DIR/ll.daemon.key" ]; then
    chown "$MBD_USER:$MBD_GROUP" "$LL_CONF_DIR/ll.daemon.key"
    chmod 600 "$LL_CONF_DIR/ll.daemon.key"
else
    echo "post-install: $LL_CONF_DIR/ll.daemon.key missing," \
         "needed where mbd or sbd runs" >&2
fi

# whoever is in the mbd group can read the client key and sign as
# any user: only the mbd user belongs in it
MEMBERS=$(getent group "$MBD_GROUP" | cut -d: -f4)
for m in $(echo "$MEMBERS" | tr ',' ' '); do
    if [ "$m" != "$MBD_USER" ]; then
        echo "post-install: WARNING user $m is in group $MBD_GROUP," \
             "it can sign requests as any user" >&2
    fi
done

echo "post-install: done"

#!/bin/sh
set -e

PANDOC=${PANDOC:-pandoc}
MDDIR=$(dirname "$0")/../markdown
MANDIR=$(dirname "$0")/..

die() { echo "convert.sh: $*" >&2; exit 1; }

command -v "$PANDOC" >/dev/null || die "pandoc not found"

convert() {
    src=$1
    dst=$2
    echo "  GEN  $dst"
    "$PANDOC" -s -t man "$src" -o "$dst"
}

convert "$MDDIR/bhosts.1.md"      "$MANDIR/man1/bhosts.1"
convert "$MDDIR/bjobs.1.md"       "$MANDIR/man1/bjobs.1"
convert "$MDDIR/bkill.1.md"       "$MANDIR/man1/bkill.1"
convert "$MDDIR/bmgroup.1.md"     "$MANDIR/man1/bmgroup.1"
convert "$MDDIR/bqueues.1.md"     "$MANDIR/man1/bqueues.1"
convert "$MDDIR/bsub.1.md"        "$MANDIR/man1/bsub.1"
convert "$MDDIR/bswitch.1.md"     "$MANDIR/man1/bswitch.1"
convert "$MDDIR/lshosts.1.md"     "$MANDIR/man1/lshosts.1"
convert "$MDDIR/lsid.1.md"        "$MANDIR/man1/lsid.1"
convert "$MDDIR/lsload.1.md"      "$MANDIR/man1/lsload.1"
convert "$MDDIR/lsb.acct.5.md"    "$MANDIR/man5/lsb.acct.5"
convert "$MDDIR/lsb.events.5.md"  "$MANDIR/man5/lsb.events.5"
convert "$MDDIR/lsb.hosts.5.md"   "$MANDIR/man5/lsb.hosts.5"
convert "$MDDIR/lsb.queues.5.md"  "$MANDIR/man5/lsb.queues.5"
convert "$MDDIR/lsf.cluster.5.md" "$MANDIR/man5/lsf.cluster.5"
convert "$MDDIR/lsf.conf.5.md"    "$MANDIR/man5/lsf.conf.5"
convert "$MDDIR/lim.8.md"         "$MANDIR/man8/lim.8"
convert "$MDDIR/mbd.8.md"         "$MANDIR/man8/mbd.8"
convert "$MDDIR/sbd.8.md"         "$MANDIR/man8/sbd.8"

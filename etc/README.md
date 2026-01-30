
## Configuration

This directory contains example configuration files from a working LavaCore installation.
Customize them to suit your cluster layout, resource policies, and job scheduling needs.

---

## lava-rsync.sh

LavaCore includes a lightweight deployment script, `lava-rsync.sh`, for pushing
binaries and configuration files to remote slave hosts.

This script avoids the need for NFS, Ansible, or other heavyweight orchestration tools.
It uses `rsync` over SSH to synchronize:

- Scheduler binaries
- Configuration files
- Optional service files

### Usage

```bash
./tools/lava-rsync.sh -h
Usage: ./lava-rsync.sh [-c] [-d] [-h] -f hostfile

Options:
  -c   Copy only configuration files (etc/)
  -d   Dry run (rsync -n)
  -h   Show this help
  -f   Hostfile with list of slave hosts


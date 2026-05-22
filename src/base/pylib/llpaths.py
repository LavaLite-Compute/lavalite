#
# LavaLite common path resolver
#
# Deterministic rules:
#   - $LSF_ENVDIR must exist
#   - lsf.conf = $LSF_ENVDIR/lsf.conf
#   - lsf.conf must define LSB_SHAREDIR
#

import os
import pathlib


class LLConfigError(RuntimeError):
    pass


def _parse_conf(path):
    conf = {}

    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.strip()

            if not line or line.startswith("#"):
                continue

            if "#" in line:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue

            if "=" not in line:
                continue

            k, v = line.split("=", 1)
            conf[k.strip()] = v.strip().strip('"').strip("'")

    return conf


# ------------------------------------------------------------

def envdir():
    v = os.environ.get("LSF_ENVDIR", "").strip()
    if not v:
        raise LLConfigError("$LSF_ENVDIR is not set")
    return v


def conf_path():
    p = os.path.join(envdir(), "lsf.conf")
    if not pathlib.Path(p).is_file():
        raise LLConfigError(f"missing lsf.conf: {p}")
    return p


def conf():
    return _parse_conf(conf_path())


def require(c, key):
    v = c.get(key, "").strip()
    if not v:
        raise LLConfigError(f"{key} missing/empty in lsf.conf")
    return v


# ------------------------------------------------------------
# shared runtime paths
# ------------------------------------------------------------

def lsb_sharedir():
    c = conf()
    return require(c, "LSB_SHAREDIR")


def mbd_dir():
    return os.path.join(lsb_sharedir(), "mbd")


def lsb_events():
    p = os.path.join(mbd_dir(), "lsb.events")
    if not pathlib.Path(p).is_file():
        raise LLConfigError(f"cannot open {p}")
    return p


def lsb_acct():
    p = os.path.join(mbd_dir(), "lsb.acct")
    if not pathlib.Path(p).is_file():
        raise LLConfigError(f"cannot open {p}")
    return p

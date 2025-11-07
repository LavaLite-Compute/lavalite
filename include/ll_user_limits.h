#pragma once

// ===== User-facing caps (ABI + on-wire), fixed values =====

// Logical line length used in user-visible data structures and text I/O
#define LL_LINE_MAX 512

// Maximum string size encoded over XDR (don’t change without protocol bump)
#define LL_WIRE_STR_MAX 512

// Short user-visible name (host types, models, queues, etc.)
#define LL_NAME_MAX 64      // legacy MAXLSFNAMELEN

// Maximum number of simple resources referenced at once
#define LL_SRES_MAX 32      // legacy MAXSRES

// Maximum length of a resource description
#define LL_RES_DESC_MAX 256     // legacy MAXRESDESLEN

// Number of built-in load indices
#define LL_BUILTIN_INDEX_MAX 11      // legacy NBUILTINDEX

// Host type/model catalogue sizes
#define LL_HOSTTYPE_MAX 128     // legacy MAXTYPES
#define LL_HOSTMODEL_MAX 128     // legacy MAXMODELS

// Hostname string bound — fixed, not libc-dependent
#define LL_HOSTNAME_MAX 255

// File or path name bound — fixed, not tied to PATH_MAX
#define LL_PATH_MAX PATH_MAX     // legacy MAXFILENAMELEN

// Semantic widths for specific public struct fields
#define LL_JOBNAME_LEN LL_LINE_MAX
#define LL_CMDLINE_LEN LL_LINE_MAX

// ===== Legacy aliases kept ON (safe for migration) =====
#ifndef MAXLINELEN
#  define MAXLINELEN LL_LINE_MAX
#endif
#ifndef MAXLSFNAMELEN
#  define MAXLSFNAMELEN LL_NAME_MAX
#endif
#ifndef MAXSRES
#  define MAXSRES LL_SRES_MAX
#endif
#ifndef MAXRESDESLEN
#  define MAXRESDESLEN LL_RES_DESC_MAX
#endif
#ifndef NBUILTINDEX
#  define NBUILTINDEX LL_BUILTIN_INDEX_MAX
#endif
#ifndef MAXFILENAMELEN
#  define MAXFILENAMELEN LL_PATH_MAX
#endif

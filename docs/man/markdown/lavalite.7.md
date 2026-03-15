# NAME

lavalite - lightweight HPC scheduler rooted in Platform Lava 1.0

# DESCRIPTION

LavaLite is a modern batch scheduler for high-performance computing
environments. It is a clean rewrite aligned with the architectural
principles of Platform Lava 1.0 (2007), designed for fast, intuitive
workflows without reliance on container orchestration or heavyweight HPC
frameworks.

LavaLite provides:

· 2 Simple job submission and control via CLI tools

· 2 Minimal runtime dependencies

· 2 Audit-friendly codebase with no legacy entanglements

# LINEAGE

LavaLite descends from Platform Lava 1.0, originally released in 2007.
No code from OpenLava, Volclava, or other forks has been retained.

# FILES

*/etc/lavalite/*

:   System-wide configuration files

*\$HOME/.lavalite/*

:   User-specific job and session data

# SEE ALSO

bsub(1), bkill(1), laload(1), lshosts(1), bhosts(1), bqueues(1), lim(8),
sbatchd(8), mbatchd(8)

# AUTHOR

LavaLite Contributors Maintained by Lu

# COPYRIGHT

GPLv2. See LICENSE and NOTICE for details.

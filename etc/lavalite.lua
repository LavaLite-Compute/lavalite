help([[LavaLite]])
whatis("Name: LavaLite")
whatis("Category: HPC/HTC Workload Manager")

family("lavalite")
local root = "/opt/lavalite-1.0.0"

prepend_path("PATH",            pathJoin(root, "bin"))
prepend_path("PATH",            pathJoin(root, "sbin"))
prepend_path("LD_LIBRARY_PATH", pathJoin(root, "lib"))
prepend_path("MANPATH",         pathJoin(root, "share", "man"))
setenv("LL_CONF_DIR", pathJoin(root, "etc"))

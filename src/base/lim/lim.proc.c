// Copyright (C) LavaLite Contributors
// GPL v2
//
// Exposes 11 load indexes used by the scheduler API:
//   r15s r1m r15m ut pg io ls it tmp swp mem
//
// Note: index names are legacy; values are Linux/top-like.

/*
 * LavaLite load indexes on Linux:
 *   r15s  - runnable tasks (from /proc/loadavg, "running")
 *   r1m   - 1 minute loadavg   (from /proc/loadavg)
 *   r15m  - 15 minute loadavg  (from /proc/loadavg)
 *   ut    - CPU busy % over last interval (from /proc/stat)
 *   pg    - swap pages per second (pswpin+pswpout delta)
 *   io    - page IO per second  (pgpgin+pgpgout delta)
 *   ls    - login sessions (unused on Linux, currently 0)
 *   it    - interactive idle (unused on Linux, currently 0)
 *   tmp   - free /tmp in MB
 *   swp   - free swap in MB
 *   mem   - "MemAvailable" in MB
 *
 * The goal is: "top - summary, but cluster-wide".
 * These are raw Linux numbers.
 */

#include "base/lim/lim.h"

static char proc_buf[LL_BUFSIZ_4K];

struct proc_state {
    double prev_ts;
    double prev_pgpg_inout;   // pgpgin + pgpgout
    double prev_pswp_inout;   // pswpin + pswpout
    int initialized;
};

static struct proc_state proc_state;

static double now_monotonic_sec(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return 0.0;

    return (double)ts.tv_sec;
}


static int read_file_once(const char *path, char *out, size_t outsz)
{
    if (outsz == 0) {
        return -1;
    }

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    ssize_t n = read(fd, out, outsz - 1);
    close(fd);

    if (n <= 0) {
        return -1;
    }

    out[n] = 0;
    return 0;
}

// Load average and run queue (r15s, r1m, r15m)
// like uptime
static int parse_loadavg(float *r15s, float *r1m, float *r15m)
{
    // /proc/loadavg: "1m 5m 15m running/total lastpid"
    double l1 = 0.0;
    double l5 = 0.0;
    double l15 = 0.0;
    int running = 0;
    int total = 0;

    if (read_file_once("/proc/loadavg", proc_buf, sizeof(proc_buf)) < 0) {
        return -1;
    }

    int n = sscanf(proc_buf, "%lf %lf %lf %d/%d",
                   &l1, &l5, &l15, &running, &total);
    if (n < 5) {
        return -1;
    }

    if (running < 0) {
        running = 0;
    }

    *r15s = (float)running;   // runnable tasks as reported by kernel
    *r1m = (float)l1;         // 1-minute load average
    *r15m = (float)l15;       // 15-minute load average

    return 0;
}

// CPU busy% (ut) from /proc/stat

struct cpu_stat {
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
};

static int parse_cpu_busy(double *busy_pct)
{
    static struct cpu_stat prev;
    static int prev_valid = 0;
    struct cpu_stat cur;

    if (read_file_once("/proc/stat", proc_buf, sizeof(proc_buf)) < 0)
        return -1;

   int n = sscanf(proc_buf,
               "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
                  &cur.user, &cur.nice, &cur.system, &cur.idle,
                  &cur.iowait, &cur.irq, &cur.softirq, &cur.steal);
   if (n < 8)
       return -1;

   if (!prev_valid) {
       prev = cur;
       prev_valid = 1;
       *busy_pct = 0.0;
       return 0;
   }

   uint64_t idle     = cur.idle + cur.iowait;
   uint64_t total    = idle + cur.user + cur.nice + cur.system +
       cur.irq + cur.softirq + cur.steal;
    uint64_t p_idle   = prev.idle + prev.iowait;
    uint64_t p_total  = p_idle + prev.user + prev.nice + prev.system +
        prev.irq + prev.softirq + prev.steal;

    uint64_t dt = total - p_total;
    uint64_t di = idle  - p_idle;

    if (dt == 0) {
        *busy_pct = 0.0;
    } else {
        *busy_pct = (double)(dt - di) * 100.0 / (double)dt;
    }

    if (*busy_pct < 0.0)
        *busy_pct = 0.0;
    if (*busy_pct > 100.0)
        *busy_pct = 100.0;

    prev = cur;
    return 0;
}

// VM counters (pg, swap activity) from /proc/vmstat

static int read_vmstat_counters(double *pgpg_inout, double *pswp_inout)
{
    FILE *f = fopen("/proc/vmstat", "r");
    if (f == NULL) {
        return -1;
    }

    char line[LL_BUFSIZ_256];
    char tag[LL_BUFSIZ_64];

    *pgpg_inout = 0.0;
    *pswp_inout = 0.0;

    while (fgets(line, sizeof(line), f) != NULL) {
        double val = 0.0;
        tag[0] = '\0';

        int n = sscanf(line, "%63s %lf", tag, &val);
        if (n != 2) {
            continue;
        }

        if (strcmp(tag, "pgpgin") == 0) {
            *pgpg_inout += val;
        } else if (strcmp(tag, "pgpgout") == 0) {
            *pgpg_inout += val;
        } else if (strcmp(tag, "pswpin") == 0) {
            *pswp_inout += val;
        } else if (strcmp(tag, "pswpout") == 0) {
            *pswp_inout += val;
        }
    }

    fclose(f);
    return 0;
}

// /proc/meminfo helpers (mem, swp) and /tmp (tmp)

static int read_meminfo_kb(uint64_t *mem_total_kb,
                           uint64_t *mem_free_kb,
                           uint64_t *mem_avail_kb,
                           uint64_t *buffers_kb,
                           uint64_t *cached_kb,
                           uint64_t *swap_total_kb,
                           uint64_t *swap_free_kb)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (f == NULL) {
        return -1;
    }

    char line[LL_BUFSIZ_256];
    char tag[LL_BUFSIZ_64];

    *mem_total_kb = 0;
    *mem_free_kb = 0;
    *mem_avail_kb = 0;
    *buffers_kb = 0;
    *cached_kb = 0;
    *swap_total_kb = 0;
    *swap_free_kb = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        uint64_t val = 0;
        tag[0] = 0;

        int n = sscanf(line, "%63s %lu", tag, &val);
        if (n != 2) {
            continue;
        }

        if (strcmp(tag, "MemTotal:") == 0) {
            *mem_total_kb = val;
        } else if (strcmp(tag, "MemFree:") == 0) {
            *mem_free_kb = val;
        } else if (strcmp(tag, "MemAvailable:") == 0) {
            *mem_avail_kb = val;
        } else if (strcmp(tag, "Buffers:") == 0) {
            *buffers_kb = val;
        } else if (strcmp(tag, "Cached:") == 0) {
            *cached_kb = val;
        } else if (strcmp(tag, "SwapTotal:") == 0) {
            *swap_total_kb = val;
        } else if (strcmp(tag, "SwapFree:") == 0) {
            *swap_free_kb = val;
        }
    }

    fclose(f);
    return 0;
}

static float mem_freeish_mb(void)
{
    uint64_t mem_total_kb = 0;
    uint64_t mem_free_kb = 0;
    uint64_t mem_avail_kb = 0;
    uint64_t buffers_kb = 0;
    uint64_t cached_kb = 0;
    uint64_t swap_total_kb = 0;
    uint64_t swap_free_kb = 0;

    if (read_meminfo_kb(&mem_total_kb, &mem_free_kb, &mem_avail_kb,
                        &buffers_kb, &cached_kb,
                        &swap_total_kb, &swap_free_kb) < 0) {
        return 0.0f;
    }

    uint64_t freeish_kb =
        (mem_avail_kb > 0)
            ? mem_avail_kb
            : (mem_free_kb + buffers_kb + cached_kb);

    // convert to MB
    return (float)(freeish_kb / 1024.0);
}


static float swap_free_mb(void)
{
    uint64_t mem_total_kb = 0;
    uint64_t mem_free_kb = 0;
    uint64_t mem_avail_kb = 0;
    uint64_t buffers_kb = 0;
    uint64_t cached_kb = 0;
    uint64_t swap_total_kb = 0;
    uint64_t swap_free_kb = 0;

    if (read_meminfo_kb(&mem_total_kb, &mem_free_kb, &mem_avail_kb,
                        &buffers_kb, &cached_kb,
                        &swap_total_kb, &swap_free_kb) < 0) {
        return 0.0f;
    }

    double mb = (double)swap_free_kb / 1024.0;
    if (mb < 0.0) {
        mb = 0.0;
    }

    return (float)mb;
}

static float tmp_avail_mb(void)
{
    struct statfs fs;

    if (statfs("/tmp", &fs) < 0) {
        return 0.0f;
    }

    if (fs.f_bavail <= 0) {
        return 0.0f;
    }

    double mb = (double)fs.f_bavail * (double)fs.f_bsize /
                (1024.0 * 1024.0);
    if (mb < 0.0) {
        mb = 0.0;
    }

    return (float)mb;
}

static uint64_t read_num_cpus(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n <= 0)
        return 1;

    return (uint64_t)n;
}

void init_read_proc(void)
{
    memset(&proc_state, 0, sizeof(proc_state));

    me->load_index[R15S] = 0.0;
    me->load_index[R1M]  = 0.0;
    me->load_index[R15M] = 0.0;

    // Max tmp space
    struct statfs fs;
    if (statfs("/tmp", &fs) == 0) {
        uint64_t bytes = (uint64_t)fs.f_blocks * fs.f_bsize;
        me->max_tmp = (float)(bytes / (1024.0 * 1024.0));
    } else {
        me->max_tmp = 0.0;
    }

    me->num_cpus = read_num_cpus();

    // Max mem/swap
    uint64_t mem_total_kb = 0;
    uint64_t mem_free_kb  = 0;
    uint64_t mem_avail_kb = 0;
    uint64_t buffers_kb   = 0;
    uint64_t cached_kb    = 0;
    uint64_t swap_total_kb = 0;
    uint64_t swap_free_kb  = 0;

    if (read_meminfo_kb(&mem_total_kb, &mem_free_kb, &mem_avail_kb,
                        &buffers_kb, &cached_kb,
                        &swap_total_kb, &swap_free_kb) == 0) {

        me->max_mem  = mem_total_kb  / 1024;   // MB
        me->max_swap = swap_total_kb / 1024;   // MB
    }

    // Prime vmstat counters
    double pgpg = 0.0;
    double pswp = 0.0;

    if (read_vmstat_counters(&pgpg, &pswp) == 0) {
        proc_state.prev_pgpg_inout = pgpg;
        proc_state.prev_pswp_inout = pswp;
    }

    proc_state.prev_ts = now_monotonic_sec();
    proc_state.initialized = 1;
}


void read_proc(void)
{
    float r15s = 0.0f;
    float r1m = 0.0f;
    float r15m = 0.0f;

    // Load averages + runnable tasks
    if (parse_loadavg(&r15s, &r1m, &r15m) == 0) {
        me->load_index[R15S] = r15s;
        me->load_index[R1M] = r1m;
        me->load_index[R15M] = r15m;
    }

    // CPU busy%
    double busy = 0.0;
    if (parse_cpu_busy(&busy) == 0) {
        me->load_index[UT] = (float)busy;
    }

    // Wall-clock delta
    double ts = now_monotonic_sec();
    if (!proc_state.initialized) {
        proc_state.prev_ts = ts;
        proc_state.initialized = 1;
    }

    double dt = ts - proc_state.prev_ts;
    if (dt <= 0.0) {
        dt = 1.0;
    }
    proc_state.prev_ts = ts;

    // VM rates: PG = swap activity, IO = page activity
    double pgpg = 0.0;
    double pswp = 0.0;

    if (read_vmstat_counters(&pgpg, &pswp) == 0) {
        double d_pgpg = pgpg - proc_state.prev_pgpg_inout;
        double d_pswp = pswp - proc_state.prev_pswp_inout;

        if (d_pgpg < 0.0) {
            d_pgpg = 0.0;
        }
        if (d_pswp < 0.0) {
            d_pswp = 0.0;
        }

        double rate_pg = d_pgpg / dt;     // page IO per second
        double rate_swp = d_pswp / dt;    // swap pages per second

        if (rate_pg < 0.0) {
            rate_pg = 0.0;
        }
        if (rate_swp < 0.0) {
            rate_swp = 0.0;
        }

        me->load_index[PG] = (float)rate_swp;
        me->load_index[IO] = (float)rate_pg;

        proc_state.prev_pgpg_inout = pgpg;
        proc_state.prev_pswp_inout = pswp;
    }

    // Linux: ls/it not meaningful here (for now)
    me->load_index[LS] = 0.0f;
    me->load_index[IT] = 0.0f;

    // tmp / swp / mem (MB of free-ish space)
    me->load_index[TMP] = tmp_avail_mb();
    me->load_index[SWP] = swap_free_mb();
    me->load_index[MEM] = mem_freeish_mb();
}

/* $Id: lim.linux.h,v 1.6 2007/08/15 22:18:54 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "lsf/lim/lim.h"

#define UNIX_KERNEL_EXE_FILE  "/vmlinux"

#define  CPUSTATES 4
#define ut_name   ut_user

static char buffer[MSGSIZE];
static long long int main_mem, free_mem, shared_mem, buf_mem, cashed_mem;
static long long int swap_mem, free_swap;

#define nonuser(ut) ((ut).ut_type != USER_PROCESS)

static double prev_time = 0, prev_idle = 0;
static double prev_cpu_user, prev_cpu_nice, prev_cpu_sys, prev_cpu_idle;
static unsigned long    prevRQ;
static int getPage(double *page_in, double *page_out, bool_t isPaging);
static int readMeminfo(void);

#ifdef __i386__
#include <sys/sysinfo.h>
#define HT_BIT          0x10000000
#define FAMILY_ID    0x0f00
#define PENTIUM4_ID  0x0f00
#define NUM_LOG_BITS  0x00FF0000

int
CPUID_is_supported()
{
        unsigned int f1, f2;
        int flag = 0x200000;
        __asm__ volatile("pushfl\n\t"
                        "pushfl\n\t"
                        "popl %0\n\t"
                        "movl %0,%1\n\t"
                        "xorl %2,%0\n\t"
                        "pushl %0\n\t"
                        "popfl\n\t"
                        "pushfl\n\t"
                        "popl %0\n\t"
                        "popfl\n\t"
                        : "=&r" (f1), "=&r" (f2)
                        : "ir" (flag));
        return ((f1^f2) & flag) != 0;
}

unsigned int HTSupported(void)
{
    unsigned char log_cpu_cnt=0;
    unsigned int reg_eax = 0;
    unsigned int reg_ebx = 0;
    unsigned int reg_edx = 0;
    unsigned int junk, junk1, junk2;
    unsigned int vendor_id[3] = {0, 0, 0};

   if ( !CPUID_is_supported() )
   {

       ls_syslog(LOG_DEBUG, "CPUID is not supported");
       return 0;
   }
    __asm__("cpuid" : "=a" (reg_eax), "=b" (*vendor_id),
                         "=c" (vendor_id[2]), "=d" (vendor_id[1]) : "a" (0));

    if ( strncmp( (char *) vendor_id, "GenuineIntel", 12) != 0 ) {
        ls_syslog(LOG_DEBUG,"Not an Intel chip");
        return 0;
    }
    __asm__("cpuid" : "=a" (reg_eax), "=b"(junk), "=c"(junk1),
                                                   "=d" (reg_edx) : "a" (1));

   if ((reg_eax & FAMILY_ID) !=  PENTIUM4_ID) {
       ls_syslog(LOG_DEBUG,"Not a P4 processor");
       return 0;
    }

    if ( (reg_edx & HT_BIT) == 0 ) {
        ls_syslog(LOG_DEBUG,"No hyper-threading");
        return 0;
    }

    __asm__("cpuid" : "=a"(junk) , "=b"(reg_ebx), "=c"(junk1),
                                                    "=d"(junk2)  : "a" (1));

    log_cpu_cnt =  (unsigned char) ((reg_ebx & NUM_LOG_BITS) >> 16);
    ls_syslog(LOG_DEBUG,"Log_cpu_cnt = %d", log_cpu_cnt);

    return ( log_cpu_cnt == 1) ? 0 :  log_cpu_cnt;
}

#include <sys/klog.h>
#define BUF_SIZE 16392
#define HT_TEXT "cpu_sibling_map["

int ht_os_cpus()
{

    static char fname[] ="ht_os_cpus()";
    char buf[BUF_SIZE];
    int n, i ;
    int len = strlen(HT_TEXT);
    int cpu_cnt=0;

    n = klogctl( 3, buf, BUF_SIZE );

    if (n < 0)
    {
       ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "klogctl", "klogctl");
       return -1;
    }

    for (i = 0; i < n; i++)
    {
        if ((i == 0 || buf[i - 1] == '\n') && buf[i] == '<')
        {
            i++;
            while (buf[i] >= '0' && buf[i] <= '9')
                i++;
            if (buf[i] == '>')
                i++;
        }
        if ( strncmp( &buf[i], HT_TEXT, len) == 0)
        {
            ls_syslog(LOG_DEBUG,"Found string<%d>\n", cpu_cnt);
            cpu_cnt++;
        }
    }
    return cpu_cnt;
}

#define HT_FLAG_CHECKED 0x01
#define SIBLINGS_CHECKED 0x02

int issiblings(FILE *fp)
{
    char * position = NULL;
    int ht = 0;
    int siblings = 0;
    int checkedItems = 0;

    fseek(fp,0,SEEK_SET);
    while(fgets(buffer, sizeof(buffer),fp) != NULL){
       if (strncmp (buffer, "flags",sizeof("flags")-1) == 0){
           checkedItems |= HT_FLAG_CHECKED;
           if (strstr(buffer,"ht") != 0){
               ht = 1;
           }
       }
       if ((strncmp (buffer, "siblings",sizeof("siblings")-1) == 0)
           || (strncmp (buffer, "Number of siblings",sizeof("Number of siblings")-1) == 0)) {
           checkedItems |= SIBLINGS_CHECKED;
           position = strchr(buffer, (int)':');
           if (position != NULL) {
               siblings = atoi(position+2);
           }
       }
       if ((checkedItems & HT_FLAG_CHECKED) && (checkedItems & SIBLINGS_CHECKED)) {
           break;
       }
    }

    if ( ht == 1 && siblings > 0) {
        return siblings;
    } else if ( ht == 1) {
        return 0;
    } else {
       return -1;
    }
}
#endif

int
numCpus(void)
{
    static char fname[] = "numCpus";
    int cpu_number = 0;
    FILE *fp;
    int ret;

    if((fp=fopen("/proc/cpuinfo","r"))==NULL) {
    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "fopen",
        "/proc/cpuinfo");
    ls_syslog(LOG_ERR, "%s: assuming one CPU only", fname);
    return 1;
    }

 #if defined(__alpha)
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    if (strncmp (buffer, "cpus detected", sizeof("cpus detected") - 1) == 0) {
        if ((position = strchr(buffer, (int)':')) == NULL){
            break;
        }
        else{
            cpu_number = atoi(position+2);
            break;
        }
    }
    }

 #endif

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
       if (strncmp (buffer, "processor", sizeof("processor") - 1) == 0) {
          cpu_number++;
       }
    }
#ifdef __i386__

    ret = issiblings(fp);
    if ( ret == 0) {
       int log_cpus_per_phys;
       int klog_cpus=0;
       int cpus_of_os =0;

       cpus_of_os = get_nprocs_conf();
       log_cpus_per_phys  = HTSupported();

       if ( log_cpus_per_phys != 0 ) {
          klog_cpus = ht_os_cpus();
          if (klog_cpus != -1 ) {

             if (klog_cpus == 0 ) {
                 ls_syslog(LOG_DEBUG," Hyper-Threading available in hardware but not supported by OS or BIOS");
             } else if ( klog_cpus == 1 || klog_cpus != cpus_of_os ) {
                 ls_syslog(LOG_DEBUG, "Can not determine hyper-threading support for sure ");
             } else {
               ls_syslog(LOG_DEBUG, "Hyper-threading supported by CPU, BIOS and OS ");
               cpu_number = klog_cpus/log_cpus_per_phys;
             }
         }
       }
    } else if (ret > 0) {
        ls_syslog(LOG_DEBUG, "This Linux box has siblings parameter in /proc/cpuinfo, siblings number is %d", ret);
        cpu_number = cpu_number/ret;
    }

#endif

    fclose(fp);
    return cpu_number;
}

int
queueLengthEx(float *r15s, float *r1m, float *r15m)
{
  static char fname[]="queueLengthEx";
#define LINUX_LDAV_FILE "/proc/loadavg"
  char ldavgbuf[40];
  double loadave[3];
  int    fd, count;

  fd = open(LINUX_LDAV_FILE, O_RDONLY);
  if ( fd < 0) {
    ls_syslog(LOG_ERR,"%s: %m", fname);
    return -1;
  }

  count = read(fd, ldavgbuf, sizeof(ldavgbuf));
  if ( count < 0) {
    ls_syslog(LOG_ERR,"%s:%m", fname);
    close(fd);
    return -1;
  }

  close(fd);
  count = sscanf(ldavgbuf, "%lf %lf %lf", &loadave[0],
                 &loadave[1], &loadave[2]);
  if (count != 3) {
    ls_syslog(LOG_ERR,"%s: %m", fname);
    return -1;
  }

  *r15s = (float)queueLength();
  *r1m  = (float)loadave[0];
  *r15m = (float)loadave[2];

  return 0;
}

float
queueLength(void)
{
    static char fname[] = "queueLength";
    float ql;
    struct dirent *process;
    int fd;
    unsigned long size;
    char status;
    DIR *dir_proc_fd;
    char filename[120];
    unsigned int running = 0;

    dir_proc_fd = opendir("/proc");
    if (dir_proc_fd == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "opendir", "/proc");
        return 0.0;
    }

    while (( process = readdir( dir_proc_fd ))) {
    if (isdigit( process->d_name[0] ) ) {
        sprintf( filename, "/proc/%s/stat", process->d_name );
        fd = open( filename, O_RDONLY, 0);
        if ( fd == -1 ) {
        ls_syslog(LOG_DEBUG, "%s: cannot open [%s], %m", fname, filename);
        continue;
        }
        if ( read(fd, buffer, sizeof( buffer) - 1 ) <= 0 ) {
        ls_syslog(LOG_DEBUG, "%s: cannot read [%s], %m", fname, filename);
        close( fd );
        continue;
        }
        close( fd );
        sscanf(buffer, "%*d %*s %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %*u %*d %*u %lu %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u\n",
        &status, &size );
        if ( status == 'R' && size > 0 )
        running++;
    }
    }
    closedir( dir_proc_fd );

    if ( running > 0 ) {
        ql = running - 1;
        if ( ql < 0 )
        ql = 0;
    }
    else {
    ql = 0;
    }

    prevRQ = ql;

    return ql;
}

void
cpuTime (double *itime, double *etime)
{
    static char fname[] = "cpuTime";
    double ttime;
    int stat_fd;
    double cpu_user, cpu_nice, cpu_sys, cpu_idle;

    stat_fd = open("/proc/stat", O_RDONLY, 0);
    if ( stat_fd == -1 ) {
    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "open", "/proc/stat");
    return;
    }

    if ( read( stat_fd, buffer, sizeof( buffer ) - 1 ) <= 0 ) {
    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "read", "/proc/stat");
        close( stat_fd );
    return;
    }
    close( stat_fd );

    sscanf( buffer, "cpu  %lf %lf %lf %lf",
    &cpu_user, &cpu_nice, &cpu_sys, &cpu_idle );

    *itime = (cpu_idle - prev_idle);
    prev_idle = cpu_idle;

    ttime = cpu_user + cpu_nice + cpu_sys + cpu_idle;
    *etime = ttime - prev_time;

    prev_time = ttime;

    if (*etime == 0 )
        *etime = 1;

    return;
}

int
realMem (float extrafactor)
{
    int realmem;

    if (readMeminfo() == -1)
    return 0;
    realmem = ( free_mem + buf_mem + cashed_mem) / 1024;

    realmem -= 2;
    realmem +=  extraload[MEM] * extrafactor;
    if (realmem < 0)
    realmem = 0;

    return realmem;
}

float
tmpspace(void)
{
    static char fname[] = "tmpspace()";
    static float tmps = 0.0;
    static int tmpcnt;
    struct statfs fs;

    if ( tmpcnt >= TMP_INTVL_CNT )
    tmpcnt = 0;

    tmpcnt++;
    if (tmpcnt != 1)
    return tmps;

    if (statfs("/tmp", &fs) < 0) {
    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "statfs", "/tmp");
        return tmps;
    }

    if (fs.f_bavail > 0)
    tmps = (float) fs.f_bavail / ((float) (1024 *1024)/fs.f_bsize);
    else
    tmps = 0.0;

    return tmps;

}

float
getswap(void)
{
    static short tmpcnt;
    static float swap;

    if ( tmpcnt >= SWP_INTVL_CNT )
    tmpcnt = 0;

    tmpcnt++;
    if (tmpcnt != 1)
    return swap;

    if (readMeminfo() == -1)
    return 0;
    swap = free_swap / 1024.0;
    return swap;
}

float
getpaging(float etime)
{
    static float smoothpg = 0.0;
    static char first = TRUE;
    static double prev_pages;
    double page, page_in, page_out;

    if ( getPage(&page_in, &page_out, TRUE) == -1 ) {
        return 0.0;
    }

    page = page_in + page_out;
    if (first) {
    first = FALSE;
    }
    else {
    if (page < prev_pages)
        smooth(&smoothpg, (prev_pages - page) / etime, EXP4);
    else
        smooth(&smoothpg, (page - prev_pages) / etime, EXP4);
    }

    prev_pages = page;

    return smoothpg;
}

float
getIoRate(float etime)
{
    float kbps;
    static char first = TRUE;
    static double prev_blocks = 0;
    static float smoothio = 0;
    double page_in, page_out;

    if ( getPage(&page_in, &page_out, FALSE) == -1 ) {
        return 0.0;
    }

    if (first) {
    kbps = 0;
    first = FALSE;

    if (myHostPtr->statInfo.nDisks == 0)
        myHostPtr->statInfo.nDisks = 1;
    } else
    kbps = page_in + page_out - prev_blocks;

    if (kbps > 100000.0) {
        ls_syslog(LOG_DEBUG,"readLoad: IO rate=%f bread=%d bwrite=%d", kbps, page_in, page_out);
    }

    prev_blocks = page_in + page_out;
    smooth(&smoothio, kbps, EXP4);

    return smoothio;
}

static int
readMeminfo(void)
{
    static char fname[] = "readMeminfo";
    FILE *f;
    char lineBuffer[80];
    long long int value;
    char tag[80];

    if ((f = fopen("/proc/meminfo", "r")) == NULL) {
    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "open",
        "/proc/meminfo");
        return -1;
    }

    while (fgets(lineBuffer, sizeof(lineBuffer), f)) {

        if (sscanf(lineBuffer, "%s %lld kB", tag, &value) != 2)
            continue;

         if (strcmp(tag, "MemTotal:") == 0)
         main_mem = value;
     if (strcmp(tag, "MemFree:") == 0)
         free_mem = value;
         if (strcmp(tag, "MemShared:") == 0)
             shared_mem = value;
         if (strcmp(tag, "Buffers:") == 0)
         buf_mem = value;
         if (strcmp(tag, "Cached:") == 0)
         cashed_mem = value;
         if (strcmp(tag, "SwapTotal:") == 0)
         swap_mem = value;
         if (strcmp(tag, "SwapFree:") == 0)
         free_swap = value;
        }
        fclose(f);
        return 0;
}

void
initReadLoad(int checkMode, int *kernelPerm)
{
    static char fname[] = "initReadLoad";
    float  maxmem;
    unsigned long maxSwap;
    struct statfs fs;
    int stat_fd;
    float k_hz;

    k_hz = (float) sysconf(_SC_CLK_TCK);

    myHostPtr->loadIndex[R15S] =  0.0;
    myHostPtr->loadIndex[R1M]  =  0.0;
    myHostPtr->loadIndex[R15M] =  0.0;

    if (checkMode)
    return;

    if (statfs( "/tmp", &fs ) < 0) {
    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "statfs",
        "/tmp");
        myHostPtr->statInfo.maxTmp = 0;
    } else
        myHostPtr->statInfo.maxTmp =
        (float)fs.f_blocks/((float)(1024 * 1024)/fs.f_bsize);

    stat_fd = open("/proc/stat", O_RDONLY, 0);
    if ( stat_fd == -1 ) {
    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "open",
        "/proc/stat");

         *kernelPerm = -1;
    return;
    }

    if ( read( stat_fd, buffer, sizeof( buffer ) - 1 ) <= 0 ) {
    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "read",
        "/proc/stat");
    close( stat_fd );

         *kernelPerm = -1;
    return;
    }
    close( stat_fd );
    sscanf( buffer, "cpu  %lf %lf %lf %lf",
    &prev_cpu_user, &prev_cpu_nice, &prev_cpu_sys, &prev_cpu_idle );

    prev_idle = prev_cpu_idle;
    prev_time = prev_cpu_user + prev_cpu_nice + prev_cpu_sys + prev_cpu_idle;

    if (readMeminfo() == -1)
    return;
    maxmem = main_mem / 1024;
    maxSwap = swap_mem / 1024;

    if (maxmem < 0.0)
        maxmem = 0.0;
    myHostPtr->statInfo.maxMem = maxmem;

    myHostPtr->statInfo.maxSwap = maxSwap;

}

const char*
getHostModel(void)
{
    static char model[MAXLSFNAMELEN];
    char buf[128], b1[128], b2[128];
    int pos = 0;
    int bmips = 0;
    FILE* fp;

    model[pos] = '\0';
    b1[0] = '\0';
    b2[0] = '\0';

    if ((fp = fopen("/proc/cpuinfo", "r")) == NULL)
        return model;

    while (fgets(buf, sizeof(buf) - 1, fp)) {
        if (strncasecmp(buf, "cpu\t", 4) == 0
        || strncasecmp(buf, "cpu family", 10) == 0) {
            char *p = strchr(buf, ':');
            if (p)
        strcpy(b1, stripIllegalChars(p + 2));
        }
        if (strstr(buf, "model") != 0) {
            char *p = strchr(buf, ':');
            if (p)
                strcpy(b2, stripIllegalChars(p + 2));
        }
        if (strncasecmp(buf, "bogomips", 8) == 0) {
            char *p = strchr(buf, ':');
            if (p)
                bmips = atoi(p + 2);
        }
    }

    fclose(fp);

    if (!b1[0])
        return model;

    if (isdigit(b1[0]))
        model[pos++] = 'x';

    strncpy(&model[pos], b1, MAXLSFNAMELEN - 15);
    model[MAXLSFNAMELEN - 15] = '\0';
    pos = strlen(model);
    if (bmips) {
        pos += sprintf(&model[pos], "_%d", bmips);
        if (b2[0]) {
            model[pos++] = '_';
            strncpy(&model[pos], b2, MAXLSFNAMELEN - pos - 1);
        }
        model[MAXLSFNAMELEN - 1] = '\0';
    }

    return model;
}

static int getPage(double *page_in, double *page_out,bool_t isPaging)
{
    static char fname[] = "getPage";
    FILE *f;
    char lineBuffer[80];
    double value;
    char tag[80];

    if ((f = fopen("/proc/vmstat", "r")) == NULL) {
       ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "fopen",
           "/proc/vmstat");
        return -1;
    }

    while (fgets(lineBuffer, sizeof(lineBuffer), f)) {

        if (sscanf(lineBuffer, "%s %lf", tag, &value) != 2)
            continue;

         if(isPaging){
            if (strcmp(tag, "pswpin") == 0)
               *page_in = value;
            if (strcmp(tag, "pswpout") == 0)
               *page_out = value;
         } else {
            if (strcmp(tag, "pgpgin") == 0)
               *page_in = value;
            if (strcmp(tag, "pgpgout") == 0)
               *page_out = value;
         }
     }
     fclose(f);
    return 0;
}

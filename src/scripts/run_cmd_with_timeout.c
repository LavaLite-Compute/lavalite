// run_cmd_with_timeout.c (snippet)
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

static int run_cmd_with_timeout(char *const argv[], int timeout_sec) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        setpgid(0,0);               // new process group
        execvp(argv[0], argv);
        _exit(127);
    }
    setpgid(pid, pid);
    int status = 0;
    time_t deadline = time(NULL) + timeout_sec;
    for (;;) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (r < 0 && errno != EINTR) break;
        if (time(NULL) >= deadline) {
            kill(-pid, SIGTERM);
            sleep(1);
            kill(-pid, SIGKILL);
            waitpid(pid, &status, 0);
            return 124;             // timeout
        }
        usleep(100 * 1000);
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

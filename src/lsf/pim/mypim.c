#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LINE_LENGTH 1024
#define MAX_COMMAND_LENGTH 256
#define DEFAULT_TIMEOUT_SECONDS 10

// Structure to hold process information
typedef struct {
    int pid;
    int ppid;
    float cpu_percent;
    float mem_percent;
    long vsz;
    long rss;
    char command[64];
} ProcessInfo;

// Function to set file descriptor to non-blocking mode
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Function to read from file descriptor with timeout
int read_with_timeout(int fd, char* buffer, size_t size, int timeout_seconds) {
    fd_set read_fds;
    struct timeval timeout;
    int result;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    result = select(fd + 1, &read_fds, NULL, NULL, &timeout);

    if (result < 0) {
        return -1; // Error
    } else if (result == 0) {
        return 0;  // Timeout
    } else {
        // Data available, read it
        return read(fd, buffer, size - 1);
    }
}

// Enhanced get_process_tree with timeout protection
int get_process_tree_with_timeout(int target_pid, ProcessInfo** processes, int timeout_seconds) {
    int pipefd[2];
    pid_t child_pid;
    char command[MAX_COMMAND_LENGTH];
    char line[MAX_LINE_LENGTH];
    char read_buffer[MAX_LINE_LENGTH];
    ProcessInfo* proc_list = NULL;
    int proc_count = 0;
    int capacity = 10;
    int status;
    int bytes_read;
    int line_pos = 0;
    int total_time = 0;

    // Allocate initial memory
    proc_list = malloc(capacity * sizeof(ProcessInfo));
    if (!proc_list) {
        return -1;
    }

    // Create pipe
    if (pipe(pipefd) == -1) {
        free(proc_list);
        return -1;
    }

    // Create the embedded script command
    snprintf(command, sizeof(command),
        "bash -c '"
        "get_process_tree() {"
        "    local pid=$1; local pids=(); "
        "    get_children() {"
        "        local parent=$1; "
        "        local children=$(pgrep -P $parent 2>/dev/null); "
        "        for child in $children; do "
        "            pids+=($child); "
        "            get_children $child; "
        "        done; "
        "    }; "
        "    pids+=($pid); "
        "    get_children $pid; "
        "    if [ ${#pids[@]} -gt 0 ]; then "
        "        for p in \"${pids[@]}\"; do "
        "            if kill -0 $p 2>/dev/null; then "
        "                ps -o pid,ppid,pcpu,pmem,vsz,rss,comm -p $p --no-headers 2>/dev/null; "
        "            fi; "
        "        done; "
        "    fi; "
        "}; "
        "get_process_tree %d'", target_pid);

    // Fork and exec
    child_pid = fork();
    if (child_pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        free(proc_list);
        return -1;
    }

    if (child_pid == 0) {
        // Child process
        close(pipefd[0]); // Close read end
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
        dup2(pipefd[1], STDERR_FILENO); // Redirect stderr to pipe
        close(pipefd[1]);

        // Execute the command
        execl("/bin/sh", "sh", "-c", command, NULL);
        _exit(1); // Should not reach here
    }

    // Parent process
    close(pipefd[1]); // Close write end

    // Set read end to non-blocking
    if (set_nonblocking(pipefd[0]) == -1) {
        close(pipefd[0]);
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        free(proc_list);
        return -1;
    }

    // Read data with timeout
    line[0] = '\0';
    line_pos = 0;

    while (total_time < timeout_seconds) {
        bytes_read = read_with_timeout(pipefd[0], read_buffer, sizeof(read_buffer), 1);

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                total_time++;
                continue;
            } else {
                // Real error
                break;
            }
        } else if (bytes_read == 0) {
            // Timeout or EOF
            // Check if child is still running
            int child_status;
            pid_t result = waitpid(child_pid, &child_status, WNOHANG);
            if (result == child_pid) {
                // Child has terminated
                break;
            } else if (result == 0) {
                // Child still running, continue waiting
                total_time++;
                continue;
            } else {
                // Error in waitpid
                break;
            }
        } else {
            // Data received
            read_buffer[bytes_read] = '\0';

            // Process the data character by character to handle partial lines
            for (int i = 0; i < bytes_read; i++) {
                if (read_buffer[i] == '\n') {
                    // End of line, process it
                    line[line_pos] = '\0';

                    if (strlen(line) > 1) {
                        // Reallocate if needed
                        if (proc_count >= capacity) {
                            capacity *= 2;
                            ProcessInfo* temp = realloc(proc_list, capacity * sizeof(ProcessInfo));
                            if (!temp) {
                                close(pipefd[0]);
                                kill(child_pid, SIGTERM);
                                waitpid(child_pid, NULL, 0);
                                free(proc_list);
                                return -1;
                            }
                            proc_list = temp;
                        }

                        // Parse the line
                        if (sscanf(line, "%d %d %f %f %ld %ld %63s",
                                  &proc_list[proc_count].pid, &proc_list[proc_count].ppid,
                                  &proc_list[proc_count].cpu_percent, &proc_list[proc_count].mem_percent,
                                  &proc_list[proc_count].vsz, &proc_list[proc_count].rss,
                                  proc_list[proc_count].command) >= 6) {
                            proc_count++;
                        }
                    }

                    line_pos = 0;
                } else if (line_pos < MAX_LINE_LENGTH - 1) {
                    // Add character to current line
                    line[line_pos++] = read_buffer[i];
                }
            }
        }
    }

    close(pipefd[0]);

    // Check if we timed out
    if (total_time >= timeout_seconds) {
        // Kill the child process
        kill(child_pid, SIGTERM);
        usleep(100000); // Wait 100ms

        // If still alive, use SIGKILL
        if (waitpid(child_pid, &status, WNOHANG) == 0) {
            kill(child_pid, SIGKILL);
            waitpid(child_pid, &status, 0);
        }

        free(proc_list);
        return -2; // Timeout error
    } else {
        // Wait for child to finish normally
        waitpid(child_pid, &status, 0);
    }

    *processes = proc_list;
    return proc_count;
}

// Wrapper function with default timeout
int
get_process_tree(int target_pid, ProcessInfo** processes) {
    return get_process_tree_with_timeout(target_pid, processes,
                                         DEFAULT_TIMEOUT_SECONDS);
}

// Alternative implementation using popen with timeout (simpler but less robust)
int get_process_tree_popen_timeout(int target_pid, ProcessInfo** processes, int timeout_seconds) {
    FILE* fp;
    int fd;
    char command[MAX_COMMAND_LENGTH];
    char line[MAX_LINE_LENGTH];
    ProcessInfo* proc_list = NULL;
    int proc_count = 0;
    int capacity = 10;
    fd_set read_fds;
    struct timeval timeout;
    int result;

    // Allocate initial memory
    proc_list = malloc(capacity * sizeof(ProcessInfo));
    if (!proc_list) {
        return -1;
    }

    // Create the command
    snprintf(command, sizeof(command),
        "timeout %d bash -c '"
        "get_process_tree() {"
        "    local pid=$1; local pids=(); "
        "    get_children() {"
        "        local parent=$1; "
        "        local children=$(pgrep -P $parent 2>/dev/null); "
        "        for child in $children; do "
        "            pids+=($child); "
        "            get_children $child; "
        "        done; "
        "    }; "
        "    pids+=($pid); "
        "    get_children $pid; "
        "    if [ ${#pids[@]} -gt 0 ]; then "
        "        for p in \"${pids[@]}\"; do "
        "            if kill -0 $p 2>/dev/null; then "
        "                ps -o pid,ppid,pcpu,pmem,vsz,rss,comm -p $p --no-headers 2>/dev/null; "
        "            fi; "
        "        done; "
        "    fi; "
        "}; "
        "get_process_tree %d'", timeout_seconds, target_pid);

    // Execute with popen
    fp = popen(command, "r");
    if (!fp) {
        free(proc_list);
        return -1;
    }

    // Get file descriptor for select()
    fd = fileno(fp);
    if (fd == -1) {
        pclose(fp);
        free(proc_list);
        return -1;
    }

    // Read with timeout using select
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        timeout.tv_sec = timeout_seconds;
        timeout.tv_usec = 0;

        result = select(fd + 1, &read_fds, NULL, NULL, &timeout);

        if (result < 0) {
            // Error
            break;
        } else if (result == 0) {
            // Timeout
            pclose(fp);
            free(proc_list);
            return -2; // Timeout error
        }

        // Data available, read it
        if (fgets(line, sizeof(line), fp) == NULL) {
            break; // EOF or error
        }

        if (strlen(line) <= 1) continue;

        // Reallocate if needed
        if (proc_count >= capacity) {
            capacity *= 2;
            ProcessInfo* temp = realloc(proc_list, capacity * sizeof(ProcessInfo));
            if (!temp) {
                pclose(fp);
                free(proc_list);
                return -1;
            }
            proc_list = temp;
        }

        // Parse the line
        if (sscanf(line, "%d %d %f %f %ld %ld %63s",
                  &proc_list[proc_count].pid, &proc_list[proc_count].ppid,
                  &proc_list[proc_count].cpu_percent, &proc_list[proc_count].mem_percent,
                  &proc_list[proc_count].vsz, &proc_list[proc_count].rss,
                  proc_list[proc_count].command) >= 6) {
            proc_count++;
        }
    }

    pclose(fp);
    *processes = proc_list;
    return proc_count;
}

// Test function
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    int target_pid = atoi(argv[1]);
    ProcessInfo* processes = NULL;

    printf("Getting process tree for PID %d with timeout protection...\n", target_pid);

    int count = get_process_tree_with_timeout(target_pid, &processes, 5); // 5 second timeout

    if (count == -2) {
        printf("Operation timed out!\n");
        return 1;
    } else if (count < 0) {
        printf("Error getting process information\n");
        return 1;
    } else if (count == 0) {
        printf("No processes found\n");
        return 0;
    }

    printf("Found %d processes:\n", count);
    printf("%-8s %-8s %-8s %-8s %-10s %-10s %-20s\n",
           "PID", "PPID", "%CPU", "%MEM", "VSZ(KB)", "RSS(KB)", "COMMAND");

    for (int i = 0; i < count; i++) {
        printf("%-8d %-8d %-8.1f %-8.1f %-10ld %-10ld %-20s\n",
               processes[i].pid, processes[i].ppid,
               processes[i].cpu_percent, processes[i].mem_percent,
               processes[i].vsz, processes[i].rss, processes[i].command);
    }

    free(processes);
    return 0;
}

/* Minimal ptrace syscall tracer (Linux x86_64 only): logs requested sizes
 * of write() syscalls targeting *gshim_log.csv, proving implicit CRT
 * flush behaviour at syscall level (B13). LD_PRELOAD cannot see stdio's
 * intra-libc writes; ptrace sees everything. Single-threaded child.
 * Usage: syscall_tracer <tracefile> <prog> [args...]
 * Trace lines: "csv_write <bytes>\n". Exit code = child's (128+sig).
 */
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

#define SYS_WRITE_NO 1 /* x86_64 */
#define CSV_MARK "gshim_log.csv"

int main(int argc, char **argv)
{
    FILE *out;
    pid_t pid;
    int status, in_sys = 0, code = 0;
    if (argc < 4) {
        fprintf(stderr, "usage: %s tracefile prog [args...]\n", argv[0]);
        return 2;
    }
    out = fopen(argv[1], "w");
    if (!out) {
        perror("tracefile");
        return 2;
    }
    pid = fork();
    if (pid < 0) {
        perror("fork");
        fclose(out);
        return 2;
    }
    if (pid == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execvp(argv[2], argv + 2);
        _exit(127);
    }
    waitpid(pid, &status, 0); /* exec stop */
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        fclose(out);
        return 126;
    }
    ptrace(PTRACE_SETOPTIONS, pid, NULL,
           (void *)(long)PTRACE_O_TRACESYSGOOD);
    ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
    for (;;) {
        if (waitpid(pid, &status, 0) < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (WIFEXITED(status)) {
            code = WEXITSTATUS(status);
            break;
        }
        if (WIFSIGNALED(status)) {
            code = 128 + WTERMSIG(status);
            break;
        }
        if (!(WIFSTOPPED(status) && (WSTOPSIG(status) & 0x80))) {
            ptrace(PTRACE_SYSCALL, pid, NULL,
                   (void *)(long)(WIFSTOPPED(status) ?
                                  WSTOPSIG(status) : 0));
            continue;
        }
        if (!in_sys) {
            struct user_regs_struct r;
            if (ptrace(PTRACE_GETREGS, pid, NULL, &r) == 0 &&
                r.orig_rax == SYS_WRITE_NO) {
                char link[64], target[512];
                ssize_t n;
                snprintf(link, sizeof(link), "/proc/%d/fd/%d",
                         (int)pid, (int)r.rdi);
                n = readlink(link, target, sizeof(target) - 1);
                if (n > 0) {
                    target[n] = '\0';
                    if (strstr(target, CSV_MARK) != NULL)
                        fprintf(out, "csv_write %llu\n",
                                (unsigned long long)r.rdx);
                }
            }
        }
        in_sys = !in_sys;
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
    }
    fclose(out);
    return code;
}

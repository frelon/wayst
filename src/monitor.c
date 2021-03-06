#define _GNU_SOURCE

#include "monitor.h"

#include <signal.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <utmp.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "vector.h"

typedef struct
{
    pid_t    child_pid;
    Monitor* instance;
} MonitorInfo;

DEF_VECTOR(MonitorInfo, NULL)

static Vector_MonitorInfo instances;

void sighandler(int sig)
{
    pid_t p;
    int   status;
    while ((p = waitpid(-1, &status, WNOHANG)) != -1) {
        if (status) {
            WRN("Child process %d exited with status %d\n", p, status);
        }
        for (size_t i = 0; i < instances.size; ++i) {
            MonitorInfo* info = Vector_at_MonitorInfo(&instances, i);
            if (info->child_pid == p) {
                info->instance->callbacks.on_exit(info->instance->callbacks.user_data);
                Vector_remove_at_MonitorInfo(&instances, i, 1);
                break;
            }
        }
    }
}

Monitor Monitor_new()
{
    static bool instances_initialized = false;
    if (!instances_initialized) {
        instances_initialized = true;
        instances             = Vector_new_MonitorInfo();
    }
    Monitor self;
    self.extra_fd      = 0;
    self.child_is_dead = true;

    return self;
}

void Monitor_fork_new_pty(Monitor* self, uint32_t cols, uint32_t rows)
{
    struct sigaction sigact;
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = sighandler;
    sigaction(SIGCHLD, &sigact, NULL);

    struct winsize ws = { .ws_col = cols, .ws_row = rows };
    openpty(&self->child_fd, &self->parent_fd, NULL, NULL, &ws);
    self->child_pid = fork();
    if (self->child_pid == 0) {
        close(self->child_fd);
        login_tty(self->parent_fd);
        unsetenv("COLUMNS");
        unsetenv("LINES");
        unsetenv("TERMCAP");
        setenv("COLORTERM", "truecolor", 1);
        setenv("VTE_VERSION", "5602", 1);
        setenv("TERM", settings.term.str, 1);

        if (execvp(settings.shell.str, (char**)settings.shell_argv)) {
            printf(TERMCOLOR_RED "Failed to execute command: \'%s\'.\n%s\n\narguments: ",
                   settings.shell.str,
                   strerror(errno));
            for (int i = 0; i < settings.shell_argc; ++i) {
                printf("\'%s\'%s",
                       settings.shell_argv[i],
                       i == settings.shell_argc - 1 ? "" : ", ");
            }
            puts("\nPress Ctrl-c to exit");

            for (;;) {
                pause();
            }
        }
    } else if (self->child_pid < 0) {
        ERR("Failed to fork process %s", strerror(errno));
    }
    close(self->parent_fd);
    Vector_push_MonitorInfo(&instances,
                            (MonitorInfo){ .child_pid = self->child_pid, .instance = self });
    self->child_is_dead = false;
}

bool Monitor_wait(Monitor* self, int timeout)
{
    memset(self->pollfds, 0, sizeof(self->pollfds));
    self->pollfds[CHILD_FD_IDX].fd     = self->child_fd;
    self->pollfds[CHILD_FD_IDX].events = POLLIN;
    self->pollfds[EXTRA_FD_IDX].fd     = self->extra_fd;
    self->pollfds[EXTRA_FD_IDX].events = POLLIN;

    if (poll(self->pollfds, 2, timeout) < 0) {
        ERR("poll failed %s", strerror(errno));
    }

    self->read_info_up_to_date = true;
    return false;
}

ssize_t Monitor_read(Monitor* self)
{
    if (unlikely(self->child_is_dead)) {
        return -1;
    }

    if (!self->read_info_up_to_date) {
        memset(self->pollfds, 0, sizeof(self->pollfds[0]));
        self->pollfds[CHILD_FD_IDX].fd     = self->child_fd;
        self->pollfds[CHILD_FD_IDX].events = POLLIN;
        if (poll(self->pollfds, 1, 0) < 0) {
            ERR("poll failed %s", strerror(errno));
        }
        self->read_info_up_to_date = true;
    }

    if (self->pollfds[CHILD_FD_IDX].revents & POLLIN) {
        ssize_t rd = read(self->child_fd, self->input_buffer, sizeof(self->input_buffer));
        if (rd < 0) {
            return -1;
        }
        self->read_info_up_to_date = false;
        return rd;
    } else {
        self->read_info_up_to_date = false;
        return -1;
    }
}

ssize_t Monitor_write(Monitor* self, char* buffer, size_t bytes)
{
    return write(self->child_fd, buffer, bytes);
}

void Monitor_kill(Monitor* self)
{
    if (self->child_pid > 1) {
        kill(self->child_pid, SIGKILL);
    }
    self->child_pid = 0;
}

void Monitor_watch_window_system_fd(Monitor* self, int fd)
{
    self->extra_fd = fd;
}

/**
 * Ensure the child proceses are killed even if we segfault */
__attribute__((destructor)) void destructor()
{
    for (size_t i = 0; i < instances.size; ++i) {
        Monitor_kill(instances.buf[i].instance);
    }
}

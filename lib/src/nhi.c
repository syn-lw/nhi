#include "sqlite.h"

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <semaphore.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

sqlite3 *db;
char table_name[11];

bool is_bash, is_terminal_setup;

int bash_history_fd;

bool completion, long_completion, after_question;

bool (*is_fd_tty)[1024];

char tty_name[15];

/*
 * set_is_bash checks if current process is bash and
 * if so sets is_bash global variable to true
 */
void set_is_bash()
{
  pid_t pid = getpid();
  char path[18];
  sprintf(path, "%s%d%s", "/proc/", pid, "/comm");
  FILE *stream = fopen(path, "r");

  char context[5];
  fgets(context, 5, stream);
  if (strcmp(context, "bash") == 0) {
    is_bash = true;
  }
  fclose(stream);
}

/*
 * init runs when shared library is loaded
 */
__attribute__((constructor)) void init()
{
  db = open_db();

  set_is_bash();

  /*
   * if process is bash set new table_name based on current time
   */
  if (is_bash) {
    sprintf(table_name, "%ld", time(NULL));
    setup_queries(table_name);
    create_table(db, table_name);
    create_row(db, table_name);

    setenv("NHI_LATEST_TABLE", table_name, 1);
  } else {
    sprintf(table_name, "%s", getenv("NHI_LATEST_TABLE"));
    setup_queries(table_name);
  }

  if (ttyname(STDIN_FILENO)) {
    sprintf(tty_name, "%s", ttyname(STDIN_FILENO));
  }
}

/*
 * destroy runs when shared library is unloaded
 */
__attribute__((destructor)) void destroy()
{
  sqlite3_close(db);
}

#include "bash/version.c"

/*
 * fetch_string fetches and returns string from given tracee address
 */
char *fetch_string(pid_t pid, size_t local_iov_len, void *remote_iov_base)
{
  struct iovec local[1];
  local[0].iov_base = calloc(local_iov_len, sizeof(char));
  local[0].iov_len = local_iov_len;

  struct iovec remote[1];
  remote[0].iov_base = remote_iov_base;
  remote[0].iov_len = local_iov_len;

  process_vm_readv(pid, local, 1, remote, 1, 0);
  return local[0].iov_base;
}

/*
 * fork creates new process and creates and attaches tracer to newly created process
 */
pid_t fork(void)
{
  pid_t (*original_fork)() = (pid_t (*)())dlsym(RTLD_NEXT, "fork");

  if (!is_terminal_setup && is_bash) {
    pid_t result = original_fork();
    return result;
  }

  pid_t tracee_pid = original_fork();

  if (!tracee_pid) {
    sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    sem_init(sem, 1, 0);

    is_fd_tty = mmap(NULL, sizeof(bool)*1024, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    pid_t tracer_pid = original_fork();
    if (!tracer_pid) {
      /*
       * Close all inherited file descriptors (except 0, 1, 2).
       * 1024 is default maximum number of fds that can be opened by process in linux.
       */
      for (int i = 3; i < 1024; i++) {
        close(i);
      }

      /*
       * Open sqlite db again, but it doesn't really matter since I plan to move to mongodb anyway.
       */
      db = open_db();

      add_start_time(db, table_name);

      pid_t tracee_pid = getppid();

      int wstatus;

      bool second_time = false;

      ptrace(PTRACE_ATTACH, tracee_pid, NULL, NULL);

      sem_post(sem);

      while(1) {
        waitpid(tracee_pid, &wstatus, 0);

        if (errno == ECHILD) {
          munmap(is_fd_tty, sizeof(bool)*1024);
          break;
        }

        int signal;
        if (WIFSTOPPED(wstatus)) {
          signal = WSTOPSIG(wstatus);
          if (signal == SIGTRAP || signal == SIGSTOP) {
            signal = 0;
          }
        }

        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, tracee_pid, NULL, &regs);
        if (second_time) {
          if (regs.rax != -1) {
            switch (regs.orig_rax) {
            case SYS_write:
              if ((*is_fd_tty)[regs.rdi]) {
                char *data = fetch_string(tracee_pid, regs.rdx, (void *)regs.rsi);
                add_output(db, table_name, data);
                free(data);
              }
              break;
            case SYS_dup2:
            case SYS_dup3:
              if ((*is_fd_tty)[regs.rdi]) {
                (*is_fd_tty)[regs.rsi] = true;
              }
              break;
            case SYS_dup:
              if ((*is_fd_tty)[regs.rdi]) {
                (*is_fd_tty)[regs.rax] = true;
              }
              break;
            case SYS_fcntl:
              if (!regs.rsi && (*is_fd_tty)[regs.rdi]) {
                (*is_fd_tty)[regs.rax] = true;
              }
              break;
            case SYS_open: {
              char *data = fetch_string(tracee_pid, 1024, (void *)regs.rdi);
              if (!strcmp(data, tty_name)) {
                (*is_fd_tty)[regs.rax] = true;
              }
              free(data);
              break;
            }
            case SYS_openat: {
              char *data = fetch_string(tracee_pid, 1024, (void *)regs.rsi);
              if (!strcmp(data, tty_name)) {
                (*is_fd_tty)[regs.rax] = true;
              }
              free(data);
              break;
            }
            case SYS_close:
              (*is_fd_tty)[regs.rdi] = false;
              break;
            }
          }

          second_time = false;
        } else {
          second_time = true;
        }

        ptrace(PTRACE_SYSCALL, tracee_pid, NULL, signal);
      }

      exit(EXIT_SUCCESS);
    } else {
      sem_wait(sem);
      sem_destroy(sem);
      munmap(sem, sizeof(sem_t));
    }
  }
  return tracee_pid;
}

/*
 * execve sets "inter-process" variables indicating if fd refers to tty and executes original shared library call.
 * execve is used by bash to execute program(s) defined in command.
 */
int execve(const char *pathname, char *const argv[], char *const envp[])
{
  if (isatty(STDOUT_FILENO)) {
    (*is_fd_tty)[STDOUT_FILENO] = true;
  }
  if (isatty(STDERR_FILENO)) {
    (*is_fd_tty)[STDERR_FILENO] = true;
  }

  int (*original_execve)() = (int (*)())dlsym(RTLD_NEXT, "execve");
  int status = original_execve(pathname, argv, envp);
  return status;
}
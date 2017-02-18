/*;
 * Author: Axel Viala
 * In 3I010 Systeme course at Université Pierre et Marie Curie
 * Year 2016 - 2017
 *
 * Mini-shell
 *
 * A minishell who can launch a command in background or wait for it.
 *
 * Possibles improvements:
 *   - relative path
 *   - Managing Pipes
 *   - bg/fg
 *   - parsing commandline
 *   - scripts
 */

// For strdup
#define _POSIX_C_SOURCE 200809L

// environ declared in unistd
#include <unistd.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <assert.h>

#include <wait.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>

#include <errno.h>

/*
 * Little slice struct for avoiding copy and
 * getting cool with string usage! all inline.
 */
typedef struct _str_slice {
  const char* data;
  const uint32_t len; // end - data len of slice.
} str_slice;

str_slice slice_at(const char* data, const char c);
str_slice slice_next(const str_slice slice, const char c);
bool slice_empty(const str_slice s);

inline
str_slice slice_at(const char* data, const char c) {
  const char* end = strchr(data, c);
  return (const str_slice) { data, end - data };
}

inline
str_slice slice_next(const str_slice slice, const char c) {
  const char* data = slice.data + slice.len + 1; // skip c
  const char* end = strchr(data, c);
  if (end != NULL) {
    return (str_slice) { data, end - data };
  } else {
    return (str_slice) {0, 0};
  }
}

inline
bool slice_empty(const str_slice s) {
  return s.len == 0;
}

const char* build_full_path(const char* progname, const str_slice slice) {
  const size_t len_progname = strlen(progname);
  // null + / so + 2.
  const size_t full_path_size = len_progname + slice.len + 1 + 1;
  const size_t malloc_size = sizeof(char) * full_path_size;
  char *full_path = malloc(malloc_size);

  full_path[full_path_size - 1] = '\0';
  strncpy(full_path, slice.data, slice.len);
  full_path[slice.len] = '/';
  strncpy(full_path + slice.len + 1, progname, len_progname);

  return (const char *) full_path;
}

/*
 * A way to define a process maybe considering adding stuff about pipes etc.
 * All fields should be considered private and managed by the caller of process_new!
 *
 * Stat information comes from stat command keep for avoiding toctou racecondition problem.
 */
typedef struct _process {
  pid_t pid;          // Initialized when launched by parent.
  const dev_t st_dev; // from stat(full_path, ...)
  const ino_t st_ino; // from stat(full_path, ...)
  const char* full_path;
  const char** args;
  char** env;
} Process;

Process process_new(const char* args[], const char* paths, char** env);
void process_drop(Process* p);
const char* process_name(const Process* p);
const char** process_args(const Process* p);
const char* process_full_path(const Process* p);
char** process_env(const Process* p);
pid_t process_pid(const Process* p);
bool process_is_valid(const Process* p);
bool process_launch(const Process* p);
bool process_launch_wait(const Process* p);

static
inline
Process _process_new(const dev_t device, const ino_t inode,
                     const char* full_path, const char* args[],
                     char* env[]) {
  return (Process) { 0, device, inode, full_path, args, env };
}

/*
 * No malloc it's just a bunch of pointers...
 * - First it try to find the full path to the program in paths variable. or locally.
 * - Next it collect perms, Inode, device info with stat.
 * Should be tested against process_valid
 *
 * Exemple:
 * ```c
 * const char* args[] = { "ls", "-a", NULL };
 * const char* env[] = { "PATH=/usr/bin:/bin", NULL };
 * const char* paths = "/usr/bin:/bin";
 * Process p = process_new(args, paths, env);
 * if (process_is_valid(&p)) {
 *   if (!process_launch(&p)) {
 *     fprintf(stderr, "Something went wrong!");
 *   }
 * }
 * ```
 * Note: Something strange with passing paths and env...
 *       maybe passing only a const copy of env.
 */
inline
Process process_new(const char* args[], const char* paths, char** env) {
  const char* progname = args[0];
  // TODO: Test if progname contain / if so must be absolute.
  // Search for a valid path.
  for (str_slice slice = slice_at(paths, ':');
       !slice_empty(slice);
       slice = slice_next(slice, ':')) {
    const char *full_path = build_full_path(progname, slice);
    if (access(full_path, X_OK) == 0) {
      struct stat prog_stat = {};
      if (stat(full_path, &prog_stat) == -1) {
        // if stat failed for other raisons.
        if (errno != EACCES) { perror("stat"); }
      } else {
        if (S_ISREG(prog_stat.st_mode)) {
          return _process_new(prog_stat.st_dev, prog_stat.st_ino, full_path, args, env);
        }
        // error = EACCES;
      }
    } else {
      // TODO: Manage other errors...
      switch (errno) {
      case ENOENT:
      case ENAMETOOLONG:
      case EACCES:
      case ENOTDIR: { break; }
      default: { perror("acess"); break; }
      }
    }
    free((void *)full_path);
  }
  // Unable to find a proper program associated with theses progname/paths
  return _process_new(0, 0, NULL, NULL, NULL);
}

/*
 * Free ressources allocated within process_new.
 */
void process_drop(Process* p) {
  free((char*)process_full_path(p));
}

/*
 * Don't mess up with the pointer!
 */
inline
const char* process_name(const Process* p) {
  return p->args[0];
}

inline
const char** process_args(const Process* p) {
  return p->args;
}

inline
const char* process_full_path(const Process* p) {
  return p->full_path;
}

inline
char** process_env(const Process* p) {
  return p->env;
}

inline
pid_t process_pid(const Process* p) {
  return p->pid;
}

extern
inline
bool process_is_valid(const Process* p) {
  return p->full_path != NULL;
}

/*
 * See: https://www.securecoding.cert.org/confluence/display/c/FIO45-C.+Avoid+TOCTOU+race+conditions+while+accessing+files
 * Maybe we should test acess and related stuff?
 */
static
inline
bool _check_toctou(const Process* p, const struct stat *c) {
  return (p->st_dev == c->st_dev
          && p->st_ino == c->st_ino);
}

/*
 * Helper considered private.
 */
bool _process_launch(const Process* p) {
  pid_t pid = fork();
  if (pid == 0) {
    // Won't return so it's OK to cast here.
    char* const* args = ((char* const*) process_args(p));
    char* const* envp = ((char* const*) process_env(p));

    struct stat child_stat = {};
    stat(process_full_path(p), &child_stat);
    if (_check_toctou(p, &child_stat)) {
      execve(process_full_path(p), args, envp);
      perror("execve"); // SHOULD not return.
      return false;
    } else {
      fprintf(stderr, "%s is not the file expected suspect a time of use, time of check race condition(TOCTOU)!\n", process_name(p));
      return false;
    }
  } else if (pid > 0) {
    // It's Ok to relax constness here. pid should not be checked before launching the process.
    ((Process *)p)->pid = pid;
    return true;
  } else {
    perror("fork");
    return false;
  }
}

/*
 * Launch and set pid of the child process.
 * return false if failed.
 *
 * TODO: Improve return value...
 */
inline
bool process_launch(const Process* p) {
  return _process_launch(p);
}

/*
 * Block process/thread until command succeed.
 */
inline
bool process_launch_wait(const Process* p) {
  bool process_status = _process_launch(p);
  if (process_status) {
    int status = 0;
    waitpid(process_pid(p), &status, 0);
  }
  return process_status;
}

extern char **environ;
int main(int argc, const char* argv[]) {
  if (argc > 1) {
    // TODO: Checking if argv[1] = ./process_name.
    const char* paths = strdup(getenv("PATH"));
    const char** process_args = &argv[1];

    Process p = process_new(process_args, paths, environ);
    if (process_is_valid(&p)) {
      if (!process_launch_wait(&p)) {
        fprintf(stderr, "Something went wrong.\n");
      }
    }
    process_drop(&p);
    free((void *)paths); // lifetime dans ce block.
  }
  return EXIT_SUCCESS;
}

/*
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
 * Little slice struct for char* null terminated.
 */
typedef struct _str_slice {
  const char* data;
  uint32_t len; // end - data len of slice.
} str_slice;

const char* slice_data(const str_slice s);
uint32_t slice_len(const str_slice slice);
str_slice slice_new(const char* data, const uint32_t len);
str_slice slice_at(const char* data, const char c);
str_slice slice_next(const str_slice slice, const char c);
bool slice_empty(const str_slice s);

inline
uint32_t slice_len(const str_slice slice) {
  return slice.len;
}

inline
const char* slice_data(const str_slice s) {
  return s.data;
}

/*
 * Caller keeps responsiblity for lifetime of data.
 */
inline
str_slice slice_new(const char* data, uint32_t len) {
  return (str_slice) { data, len };
}

inline
str_slice slice_at(const char* data, const char c) {
  const char* end = strchr(data, c);
  uint32_t len = end - data;
  return slice_new(data, len);
}

inline
str_slice slice_next(const str_slice s, const char c) {
  const char* data = slice_data(s) + slice_len(s) + 1; // skip c
  const char* end = strchr(data, c);
  if (end != NULL) {
    return slice_new(data, end - data);
  } else {
    return slice_new(NULL, 0);
  }
}

inline
bool slice_empty(const str_slice s) {
  return s.len == 0;
}

const char* build_full_path(const str_slice prog, const str_slice slice) {
  size_t full_path_size = slice_len(prog) + slice_len(slice) + 2; // save place for '/' and '\0'.
  size_t malloc_size = sizeof(char) * full_path_size;
  char *full_path = malloc(malloc_size);

  full_path[full_path_size - 1] = '\0';
  memcpy(full_path, slice_data(slice), slice_len(slice));
  full_path[slice_len(slice)] = '/';
  memcpy(full_path + slice_len(slice) + 1, slice_data(prog), slice_len(prog));

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
  dev_t st_dev; // from stat(full_path, ...)
  ino_t st_ino; // from stat(full_path, ...)
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

// TODO: Manage other errors...
static
inline
void _process_new_errors(int err) {
  // TODO: Manage other errors...
  switch (err) {
    case ENOENT:
    case ENAMETOOLONG:
    case EACCES:
    case ENOTDIR: { break; }
    default: { perror("acess"); break; }
  }
}

/*
 * - First it try to find the full path to the program in paths variable. or locally.
 * - Next it collect perms, Inode, device info with stat.
 * Should be tested against process_valid
 *
 * Exemple:
 * ```c
 * char* args[] = { "ls", "-a", NULL };
 * char* env[] = { "PATH=/usr/bin:/bin", NULL };
 * char* paths = "/usr/bin:/bin";
 * Process p = process_new(args, paths, env);
 * if (process_is_valid(&p)) {
 *   if (!process_launch(&p)) {
 *     fprintf(stderr, "Something went wrong!");
 *   }
 * }
s * ```
 * Note: Something strange with passing paths and env...
 *       maybe passing only a const copy of env.
 */
inline
Process process_new(const char* args[], const char* paths, char** env) {
  // TODO: Test if progname contain / if so must be absolute.
  // Search for a valid path.
  const str_slice progname = slice_new(args[0], strlen(args[0]));
  for (str_slice s = slice_at(paths, ':');
       !slice_empty(s);
       s = slice_next(s, ':')) {
    const char *full_path = build_full_path(progname, s);
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
      _process_new_errors(errno);
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
  if (argc <= 1) {
    // TODO: Checking if argv[1] = ./process_name.
    size_t len = 0;
    ssize_t read = 0;
    char *line = NULL;
    bool should_wait = true;

    while (true) {
      read = getdelim(&line, &len, '\n', stdin);
      if (read != -1) {
        size_t count = 0;
        // count spaces for aproximation of args size.
        for (size_t i = 0; i < read; i += 1) {
          if (line[i] == ' ') { count += 1; }
        }

        // Alloc apromatively readed chars / (space count)... think about free this after.
        char** args = malloc(sizeof(char *) * (read / (count + 1)));

        // REALLY simple parsing of command line.
        char* buffer_strtok = NULL;
        // buff is null on subsequent call of strtok_r see man strtok_r.
        char *buff = line;
        size_t i = 0;
        for (; true ; i += 1, buff = NULL) {
          char* token = strtok_r(buff, " \n", &buffer_strtok);
          if (token != NULL) {
            args[i] = token;
            // REALLY naive solution...
            // Test against builtins...
            // Or use flex/bison seriously...
            if (strchr(token, '&')) {
              should_wait = false;
            }
          } else {
            break;
          }
        }
        if (i == 0) { continue; }

        fputc('\n', stdout);
        const char* paths = strdup(getenv("PATH"));
        Process p = process_new((const char**)args, paths, environ);
        if (process_is_valid(&p)) {
          bool launch_status = true;
          if (should_wait) {
            launch_status = process_launch_wait(&p);
          } else {
            launch_status = process_launch(&p);
          }
          if (!launch_status) {
            fprintf(stderr, "Something went wrong.\n");
          }
        }
        process_drop(&p);
        free((void *)paths); // lifetime dans ce block.
      } else {
        perror("getline");
      }
    }
  }
  return EXIT_SUCCESS;
}

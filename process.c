#define _POSIX_C_SOURCE 200809L
/*
 * Author: Axel Viala
 * In 3I010 Systeme course at Université Pierre et Marie Curie
 * Year 2016 - 2017
 *
 * Part of Mini-shell
 *
 * process.c
 *
 */


// environ declared in unistd
#include <unistd.h>

#include <stdint.h>
#include <stdbool.h>

#include <stdlib.h> // malloc/free
#include <stdio.h> // fprintf

#include <wait.h> // waitpid

#include <sys/types.h>
#include <sys/stat.h>

#include <string.h> // strlen

#include <errno.h>

#include "slice.h"
#include "process.h"

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

Process _process_new(const dev_t device, const ino_t inode,
                     const char* full_path, const char* args[],
                     char* env[]) {
  return (Process) { 0, device, inode, full_path, args, env };
}

// TODO: Manage other errors...
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

extern
Process process_empty(void) {
  return _process_new(0, 0, NULL, NULL, NULL);
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
extern
Process process_new(const char* args[], const char* paths, char** env) {
  // TODO: Test if progname contain / if so must be absolute.
  // Search for a valid path.
  const str_slice progname = slice_new(args[0], strlen(args[0]));
  for (str_slice s = slice_at(paths, ':');
       !slice_empty(s);
       s = slice_next(s, ':')) {
    const char *full_path = build_full_path(progname, s);
    if (access(full_path, X_OK) == 0) {
      struct stat prog_stat;
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
  if (errno == ENOENT) {
    fprintf(stderr, "Aucune commande ou dossier ne porte ce nom. ¯\\_(ツ)_/¯ \n");
  }
  // Unable to find a proper program associated with theses progname/paths
  return process_empty();
}

// return true if background process has resumed.
// else false.
extern
bool process_check_status(const Process *p) {
  int status = 0;
  int child_pid = process_pid(p);
  if (waitpid(child_pid, &status, WNOHANG) == -1) {
    fprintf(stderr, "(child %d resumed)\n", child_pid);
    return true;
  } else {
    return false;
  }
}

// based on process pid.
extern
bool process_equals(const Process* p, const Process* b) {
  return process_pid(p) == process_pid(b);
}

/*
 * Free ressources allocated within process_new.
 */
extern
void process_drop(Process* p) {
  free((char*)process_full_path(p));
}

/*
 * Don't mess up with the pointer!
 */
extern
const char* process_name(const Process* p) {
  return p->args[0];
}

extern
const char** process_args(const Process* p) {
  return p->args;
}

extern
const char* process_full_path(const Process* p) {
  return p->full_path;
}

extern
char** process_env(const Process* p) {
  return p->env;
}

extern
pid_t process_pid(const Process* p) {
  return p->pid;
}

extern
bool process_is_valid(const Process* p) {
  return p->full_path != NULL;
}

/*
 * See: https://www.securecoding.cert.org/confluence/display/c/FIO45-C.+Avoid+TOCTOU+race+conditions+while+accessing+files
 * Maybe we should test acess and related stuff?
 */
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

    struct stat child_stat;
    stat(process_full_path(p), &child_stat);
    if (_check_toctou(p, &child_stat)) {
      // Switch child to other process group see man 2 setpgid.
      // Think about forwarding SIGINT also.
      setpgid(0, 0);
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
extern
bool process_launch(const Process* p) {
  bool status = _process_launch(p);
  if (status) {
    fprintf(stderr, "(child pid: %d)\n",  process_pid(p));
  }
  return status;
}

/*
 * Block process/thread until command succeed.
 */
extern
bool process_launch_wait(const Process* p) {
  bool process_status = _process_launch(p);
  if (process_status) {
    int status = 0;
    waitpid(process_pid(p), &status, 0);
  }
  return process_status;
}

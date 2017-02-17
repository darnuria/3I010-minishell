// For strdup
#define _POSIX_C_SOURCE 200809L

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


extern char **environ;

/*
 * Little slice struct for avoiding copy and
 * getting cool with string usage! all inline.
 */
typedef struct _str_slice {
  const char* data;
  const uint32_t len; // end - data len of slice.
} str_slice;

inline
str_slice slice_at(const char* data, const char c) {
  const char* end = strchr(data, c);
  return (const str_slice) {
    data,
      end - data
      };
}

inline
str_slice slice_next(const str_slice slice, const char c) {
  const char* data = slice.data + slice.len + 1; // skip c
  const char* end = strchr(data, c);
  if (end != NULL) {
    return (str_slice) {
      data,
      end - data
    };
  } else {
    return (str_slice) {0, 0};
  }
}

inline
bool slice_empty(const str_slice s) {
  return s.len == 0;
}

const char* build_full_path(const char* progname, const str_slice slice) {
  // null + / so + 2.
  size_t full_path_size = strlen(progname) + slice.len + 1 + 1;
  char *full_path = malloc(sizeof(char*) * full_path_size);

  full_path[full_path_size] = '\0';
  strncpy(full_path, slice.data, slice.len);
  full_path[slice.len] = '/';
  strcpy(full_path + slice.len + 1, progname);

  return (const char *) full_path;
}

char const* find_proper_path(const char* progname, const char* paths) {
  for (str_slice slice = slice_at(paths, ':');
       !slice_empty(slice);
       slice = slice_next(slice, ':')) {
    const char *full_path = build_full_path(progname, slice);
    struct stat file_stat = {};
    if (stat(full_path, &file_stat) == -1) {
      // perror("stat error"); It's OK if file not exist.
    } else {
      if ((file_stat.st_mode & S_IXUSR)
          || (file_stat.st_mode & S_IXGRP)
          || (file_stat.st_mode & S_IXGRP)) {
        return full_path;
        break; // Fine we have our executable exit loop.
      }
    }
  }
  // Found nothing :(
  return NULL;
}

/* 
 * TODO: - relative path > $PATHS
 *       - noWait &
 *       - fg/bg?
 *
 */
int main(int argc, char *argv[]) {
  // Try to lookup program in PATH.
  if (argc > 1) {
    const char* paths = strdup(getenv("PATH"));
    assert(paths);
    const char* progname = argv[1];
    const char *full_path = find_proper_path(progname, paths);
    if (full_path != NULL) {
      // Test if wait or not.
      if (true) {
        // Managing real work issues EAGAIN ENOMEM ENOSYS
        int status; // Init when child exit.
        const pid_t pid = fork();
        if (pid < 0) {
          perror("fork failed");
        } else if (pid > 0) {
          waitpid(pid, &status, 0);
        } else {
          execve(full_path, &argv[1], environ);
        }
      }

      free((void *)full_path); // lifetime dans ce block.
      free((void *)paths); // lifetime dans ce block.
    }
  }
  return EXIT_SUCCESS;
}

#define _POSIX_C_SOURCE 200809L
/*
 * Author: Axel Viala
 * In 3I010 Systeme course at Université Pierre et Marie Curie
 * Year 2016 - 2017
 *
 * Mini-shell
 * * A minishell who can launch a command in background or wait for it.
 *
 * Possibles improvements:
 *
 *   - Managing Pipes
 *
 * main.c
 */

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


#include "slice.h"
#include "process.h"

extern char **environ;

char** parse_args(char** args, char* line);

// TODO: use flex/bison
// It's really naive.
// Not really handy.
char** parse_args(char** args, char* line) {
  char* buffer_strtok = NULL;
  // buff is null on subsequent call of strtok_r see man.
  char *buff = line;
  size_t i = 0;
  for (; true ; i += 1, buff = NULL) {
    char* token = strtok_r(buff, " \n", &buffer_strtok);
    if (token != NULL) {
      args[i] = token;
    } else {
      break;
    }
  }

  fprintf(stderr, "i: %ld;\n", i);
  if (i == 0) {
    return NULL;
  } else {
    return args;
  }
}



// Implementing a process list.
int main(void) {
  // TODO: Checking if argv[1] = ./process_name.
  size_t len = 0;
  ssize_t read = 0;
  char* line = NULL;

  const size_t size = 1 + 1;
  char** args = malloc(sizeof(*args) * (size));
  args[size - 1] = NULL;
  const char* prompt = "$ ";

  // Simple management of jobs.
  // There is only one backgorund process.
  Process background_process = process_empty();

  while (true) {
    (void) fputs(prompt, stdout);
    read = getdelim(&line, &len, '\n', stdin);
    if (read != -1) {
      size_t count = 0;
      // count spaces for aproximation of args size.
      // Safe to cast due to previous test.
      for (size_t i = 0; i < (size_t) read; i += 1) {
        if (line[i] == ' ') { count += 1; }
      }

      // Somewhat clunky...
      size_t approx_args = (count + 1);
      if (approx_args > size) {
        args = realloc(args, sizeof(*args) * (approx_args + 1));
        for (size_t i = 0; i <= approx_args; i += 1) {
          args[i] = NULL;
        }
      }
      fprintf(stderr, "size_args: %ld\n", approx_args);

      // Search for background job symbol
      bool should_wait = true;
      char* pos_wait = strchr(line, '&');
      if (pos_wait != NULL) {
        *pos_wait = ' '; // remove it.
        should_wait = false;
      } else {
        should_wait = true;
      }

      // Nothing parsed.
      if (parse_args(args, line) == NULL) {
        args[0] = NULL;
        continue;
      }


      const char* paths = strdup(getenv("PATH"));
      Process p = process_new((const char**)args, paths, environ);
      if (process_is_valid(&p)) {
        bool launch_status = true;
        if (should_wait) {
          launch_status = process_launch_wait(&p);
        } else {
          launch_status = process_launch(&p);
          background_process = p;
        }
        if (!launch_status) {
          perror("");
          fprintf(stderr, "Something went wrong.\n");
        }
      }


      if (!process_equals(&p, &background_process)) {
        process_drop(&p);
      }
      free((void *)paths); // lifetime dans ce block.
    } else {
      if (errno == EINVAL) { perror("getline"); }
      // else getline didn't read fine.
    }

    // Check for terminated process in background.
    if (process_is_valid(&background_process) && process_check_status(&background_process)) {
      process_drop(&background_process);
      background_process = process_empty();
    }
  }
  // free(args);

  return EXIT_SUCCESS;
}

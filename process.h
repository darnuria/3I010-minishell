#ifndef _PROCESS_H
#define _PROCESS_H

#include <unistd.h>
/*
 * A way to define a process maybe considering adding stuff about pipes etc.
 * All fields should be considered private and managed by the caller of process_new!
 *
 * Stat information comes from stat command keep for avoiding toctou racecondition problem.
 * get grouppid also?
 * pipes?
 */
typedef struct _process {
  pid_t pid;    // Initialized when launched by parent.
  dev_t st_dev; // from stat(full_path, ...)
  ino_t st_ino; // from stat(full_path, ...)
  const char* full_path;
  const char** args;
  char** env;
} Process;

/* public interface */
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
Process process_empty(void);
bool process_check_status(const Process* p);
bool process_equals(const Process* p, const Process* b);

/* private interface */
bool _check_toctou(const Process* p, const struct stat *c);
const char* build_full_path(const str_slice prog, const str_slice slice);
Process _process_new(const dev_t device, const ino_t inode,
                     const char* full_path, const char* args[],
                     char* env[]);
void _process_new_errors(int err);
bool _process_launch(const Process* p);

#endif // _PROCESS_H

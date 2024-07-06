#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

typedef int pid_t;

struct process_control_block {
  pid_t pid;                              /* Process ID, same as thread ID. */
  struct list_elem elem;                  /* List element for child processes list. */
  char *command;                          /* Command running in process. */

  bool waiting;                           /* Is process being waited on. */
  bool exited;                            /* Has process exited. */
  int exit_code;                          /* Exit code of process. */

  struct semaphore waiting_sema;          /* Semaphore to synchronize `process_wait`. */
  struct semaphore initialization_sema;   /* Semaphore to synchronize process initialization. */
};

struct file_desc {
  int id;                 /* fd value. */
  struct list_elem elem;  /* List element for open files list. */
  struct file *file;      /* File. */
};

pid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */

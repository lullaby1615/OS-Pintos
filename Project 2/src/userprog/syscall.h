#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <threads/synch.h>

void syscall_init (void);

static struct lock file_lock;
#endif /* userprog/syscall.h */

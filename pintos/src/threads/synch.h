#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore 
  {
    unsigned value;             /* Current value. */
    struct list waiters;        /* List of waiting threads. */
  };

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);
bool wait_cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux);

/* Lock. */
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
    int max;                    /* 最大的优先级 */
    struct list_elem elem;      /*  */
    struct list acquirers;//try to acquire the lock
  };

void lock_init (struct lock *);
void lock_acquire (struct lock *);
void update_thread(struct lock *lock);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);

void thread_update_priority (struct thread *t);
bool lock_cmp_priority(const struct list_elem *a,const struct list_elem *b,void *aux);
bool lock_remove_acquirer(struct lock* lock);
int lock_get_priority(struct lock* lock);

/* Condition variable. */
struct condition 
  {
    struct list waiters;        /* List of waiting threads. */
  };

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);
bool cond_compare (struct list_elem *a, struct list_elem *b, void *aux);

/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */

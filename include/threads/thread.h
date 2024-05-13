#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>

#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status {
    THREAD_RUNNING, /* Running thread. */
    THREAD_READY,   /* Not running but ready to run. */
    THREAD_BLOCKED, /* Waiting for an event to trigger. */
    THREAD_DYING    /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) - 1) /* Error value for tid_t. */

/* Thread priorities. */
/* input: pintos -v -k -T 300  -m 20   --fs-disk=10 -p tests/userprog/no-vm/multi-oom:multi-oom -- -q   -f run multi-oom < /dev/null 2> tests/userprog/no-vm/multi-oom.errors >
 * tests/userprog/no-vm/multi-oom.output*/
/* page, limit =
                 P(1, 48): OOM Success but *Exception: 1628 page faults
                 P(1, 64): OOM Success but *Exception: 1870 page faults
                 P(2, 32): OOM Success but *Exception: 1628 page faults
                 P(3, 16): OOM Success but *Exception: 198 page faults
                 P(3, 48): OOM Success but *Exception: 1518 page faults
                 P(48, 3): OOM Success but *Exception: 264 page faults
                 P(60, 3): OOM Success but *Exception: 198 page faults
                 P(100,3)

                 P(600,4, m=200MB)

                 F(1, 128): child_210_X: exit(-1) //TIMEOUT
                 F(4, 8)
                 F(100,1): child_0_O: exit(1) //!spawned at least 10 children
                 F(600,4, m=120MB): //!spawned at least 10 children */

#define PRI_MIN 0      /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63     /* Highest priority. */
#define FDT_PAGES 3    
#define FDT_COUNT_LIMIT 16

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
    /* Owned by thread.c. */
    tid_t tid;                 /* Thread identifier. */
    enum thread_status status; /* Thread state. */
    char name[16];             /* Name (for debugging purposes). */
    int priority;              /* Priority. */
    int init_priority;
    int64_t wakeup_tick; /* 깨어나야 할 tick 디스크립터 필드 추가*/
    /* Shared between thread.c and synch.c. */
    struct list_elem elem; /* List element. */
    struct lock *wait_on_lock;
    struct list donations;
    struct list_elem donation_elem;

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
    /* Table for whole virtual memory owned by thread. */
    struct supplemental_page_table spt;
#endif

    /* Owned by thread.c. */
    struct intr_frame tf; /* Information for switching */

    struct intr_frame parent_if;
    struct list child_list;
    struct list_elem child_elem;

    struct file *running;

    struct file **fdt;
    int next_fd;
    int exit_status;

    struct semaphore load_sema;
    struct semaphore exit_sema;
    struct semaphore wait_sema;

    unsigned magic; /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

bool thread_wakeup_tick_asc(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);
void thread_sleep(int64_t ticks);

void thread_block(void);
void thread_unblock(struct thread *);

void donate_priority(void);
void remove_with_lock(struct lock *lock);
void refresh_priority(void);

bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux);
void test_max_priority(void);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);

#endif /* threads/thread.h */

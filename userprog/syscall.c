#include "userprog/syscall.h"

#include <stdio.h>
#include <syscall-nr.h>

#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/thread.h"
#include "userprog/gdt.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void check_address(void *uaddr);
void halt(void);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
tid_t fork(const char *thread_name, struct intr_frame *f);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void) {
    write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 | ((uint64_t)SEL_KCSEG) << 32);
    write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* The interrupt service rountine should not serve any interrupts
     * until the syscall_entry swaps the userland stack to the kernel
     * mode stack. Therefore, we masked the FLAG_FL. */
    write_msr(MSR_SYSCALL_MASK, FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED) {
    check_address(f->R.rax);  // 임시
    uint64_t syscall_n = f->R.rax;

    switch (syscall_n) {
        case SYS_HALT:
            halt();
            break;

        case SYS_EXIT:
            exit(f->R.rdi);
            break;

        case SYS_FORK:
            fork(f->R.rdi, f);
            break;

        case SYS_EXEC:
            break;

        case SYS_WAIT:
            break;

        case SYS_CREATE:
            f->R.rax = create(f->R.rdi, f->R.rsi);
            break;

        case SYS_REMOVE:
            f->R.rax = remove(f->R.rdi);
            break;

        case SYS_OPEN:
            break;

        case SYS_FILESIZE:
            break;

        case SYS_READ:
            break;

        case SYS_WRITE:
            printf(f->R.rdi);
            break;

        case SYS_SEEK:
            break;

        case SYS_TELL:
            break;

        case SYS_CLOSE:
            break;

        default:
            exit(-1);
            break;
    }
}

void check_address(void *uaddr) {
    struct thread *cur = thread_current();
    if (uaddr == NULL || is_kernel_vaddr(uaddr) || pml4_get_page(cur->pml4, uaddr) == NULL) {
        exit(-1);
    }
}

void exit(int status) {
    printf("%s: exit(%d)\n", thread_name(), status);
    thread_exit();
}

void halt(void) {
    power_off();
}

bool create(const char *file, unsigned initial_size) {
    check_address(file);
    bool is_success = filesys_create(file, initial_size);

    return is_success;
}

bool remove(const char *file) {
    check_address(file);
    bool is_success = filesys_remove(file);
    return is_success;
}

tid_t fork(const char *thread_name, struct intr_frame *f) {
    return process_fork(thread_name, f);
}
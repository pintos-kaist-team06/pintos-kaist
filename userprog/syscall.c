#include "userprog/syscall.h"

#include <stdio.h>
#include <syscall-nr.h>

#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/gdt.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void check_address(void *uaddr);
void halt(void);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
tid_t fork(const char *thread_name, struct intr_frame *f);
void seek(int fd, unsigned position);
unsigned tell(int fd);
int filesize(int fd);
void close(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned length);

struct lock filesys_lock;

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
    lock_init(&filesys_lock);
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
            f->R.rax = fork(f->R.rdi, f);
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
            f->R.rax = open(f->R.rdi);
            break;

        case SYS_FILESIZE:
            f->R.rax = filesize(f->R.rdi);
            break;

        case SYS_READ:
            f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
            break;

        case SYS_WRITE:
            f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
            break;

        case SYS_SEEK:
            seek(f->R.rdi, f->R.rsi);
            break;

        case SYS_TELL:
            f->R.rax = tell(f->R.rdi);
            break;

        case SYS_CLOSE:
            close(f->R.rdi);
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

int open(const char *file) {
    check_address(file);

    lock_acquire(&filesys_lock);
    struct file *f = filesys_open(file);
    if (f == NULL) {
        lock_release(&filesys_lock);
        return -1;
    }
    int fd = process_add_file(f);
    if (fd == -1)
        file_close(f);

    lock_release(&filesys_lock);
    return fd;
}

tid_t fork(const char *thread_name, struct intr_frame *f) {
    return process_fork(thread_name, f);
}

void seek(int fd, unsigned position) {
    struct file *file = process_get_file(fd);
    if (file == NULL)
        return -1;

    file_seek(process_get_file(file), position);
}

unsigned tell(int fd) {
    struct file *file = process_get_file(fd);
    if (file == NULL)
        return -1;

    return file_tell(file);
}

void close(int fd) {
    struct file *file = process_get_file(fd);
    file_close(file);
    process_close_file(fd);
}

int filesize(int fd) {
    struct file *file = process_get_file(fd);
    if (file == NULL)
        return -1;

    return file_length(file);
}

int read(int fd, void *buffer, unsigned size) {
    check_address(buffer);

    char *ptr = (char *)buffer;
    int bytes_read = 0;

    lock_acquire(&filesys_lock);
    if (fd == STDIN_FILENO) {
        for (int i = 0; i < size; i++) {
            *ptr++ = input_getc();
            bytes_read++;
        }
        lock_release(&filesys_lock);
    } else {
        struct file *file = process_get_file(fd);
        if (file == NULL) {
            lock_release(&filesys_lock);
            return -1;
        }
        bytes_read = file_read(file, buffer, size);
        lock_release(&filesys_lock);
    }
    return bytes_read;
}

int write(int fd, const void *buffer, unsigned length) {
    check_address(buffer);

    char *ptr = (char *)buffer;
    int bytes_write = 0;

    lock_acquire(&filesys_lock);
    if (fd == STDOUT_FILENO) {
        putbuf(buffer, length);
        lock_release(&filesys_lock);
    }

    else {
        struct file *file = process_get_file(fd);
        if (file == NULL) {
            lock_release(&filesys_lock);
            return -1;
        }
        bytes_write = file_write(file, buffer, length);
        lock_release(&filesys_lock);
    }
    return bytes_write;
}
#include "userprog/syscall.h"

#include <stdio.h>
#include <syscall-nr.h>

#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "intrinsic.h"
#include "lib/kernel/stdio.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/gdt.h"
#include "userprog/process.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *f UNUSED);
void check_address(void *uaddr);
void halt(void);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
tid_t fork(const char *thread_name);
void seek(int fd, unsigned position);
unsigned tell(int fd);
int filesize(int fd);
void close(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned length);
int exec(const char *file);
int wait(pid_t);
int open(const char *file);
void exit(int status);

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
    uint64_t syscall_n = f->R.rax;
    switch (syscall_n) {
        case SYS_HALT:
            halt();
            break;

        case SYS_EXIT:
            exit(f->R.rdi);
            break;

        case SYS_FORK:
            memcpy(&thread_current()->parent_if, f, sizeof(struct intr_frame));
            f->R.rax = fork(f->R.rdi);
            break;

        case SYS_EXEC:
            exec(f->R.rdi);
            break;

        case SYS_WAIT:
            f->R.rax = wait(f->R.rdi);
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
    thread_current()->exit_status = status;
    printf("%s: exit(%d)\n", thread_name(), status);
    thread_exit();
}

void halt(void) {
    power_off();
}

int exec(const char *file) {
    check_address(file);

    // process.c 파일의 process_create_initd 함수와 유사하다.
    // 단, 스레드를 새로 생성하는 건 fork에서 수행하므로
    // 이 함수에서는 새 스레드를 생성하지 않고 process_exec을 호출한다.

    // process_exec 함수 안에서 filename을 변경해야 하므로
    // 커널 메모리 공간에 cmd_line의 복사본을 만든다.
    // (현재는 const char* 형식이기 때문에 수정할 수 없다.)
    char *file_copy;
    file_copy = palloc_get_page(0);
    if (file_copy == NULL)
        exit(-1);                      // 메모리 할당 실패 시 status -1로 종료한다.
    strlcpy(file_copy, file, PGSIZE);  // cmd_line을 복사한다.

    // 스레드의 이름을 변경하지 않고 바로 실행한다.
    if (process_exec(file_copy) == -1)
        exit(-1);  // 실패 시 status -1로 종료한다.
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

int wait(pid_t) {
    return process_wait(pid_t);
}

tid_t fork(const char *thread_name) {
    return process_fork(thread_name, &thread_current()->parent_if);
}

void seek(int fd, unsigned position) {
    struct file *file = process_get_file(fd);
    if (file == NULL)
        return;

    file_seek(file, position);
}

unsigned tell(int fd) {
    struct file *file = process_get_file(fd);
    if (file == NULL)
        return;

    return file_tell(file);
}

void close(int fd) {
    struct file *file = process_get_file(fd);
    if (file == NULL)
        return;
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

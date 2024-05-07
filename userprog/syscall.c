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
void exit(int status);
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
// 누구의 f지??
void syscall_handler(struct intr_frame *f UNUSED) {
    /*유저 영역을벗어난 영역일 경우 프로세스 종료(exit(-1)) */
    if (is_kernel_vaddr(f->rsp) || thread_current()->pml4 < f->rsp || f->rsp == NULL)
        exit(-1);

    // TODO: Your implementation goes here.
    switch (f->R.rax) {
        case SYS_HALT: /* Halt the operating system. */
            power_off();

        case SYS_EXIT: /* Terminate this process. */
            exit(f->R.rdi);

        case SYS_FORK: /* Clone current process. */
            // f->R.rax = fork(f->R.rdi, f);
            // break;

        case SYS_EXEC: /* Switch current process. */
            /*현재의 프로세스가 cmd_line에서 이름이 주어지는 실행가능한 프로세스로 변경됩니다. 이때 주어진 인자들을 전달합니다. 성공적으로 진행된다면 어떤 것도 반환하지 않습니다.
             * 만약 프로그램이 이 프로세스를 로드하지 못하거나 다른 이유로 돌리지 못하게 되면 exit state -1을 반환하며 프로세스가 종료됩니다. 이 함수는 exec 함수를 호출한 쓰레드의
             * 이름은 바꾸지 않습니다. file descriptor는 exec 함수 호출 시에 열린 상태로 있다는 것을 알아두세요. */

        case SYS_WAIT:   /* Wait for a child process to die. */
        case SYS_CREATE: /* Create a file. */
            f->R.rax = create((f->R.rdi), (f->R.rsi));

        case SYS_REMOVE: /* Delete a file. */
            f->R.rax = remove((f->R.rdi));

        case SYS_OPEN:               /* Open a file. */
        case SYS_FILESIZE:           /* Obtain a file's size. */
        case SYS_READ:               /* Read from a file. */
        case SYS_WRITE:              /* Write to a file. */
            printf("%s", f->R.rdi);  // 가짜값

        case SYS_SEEK:  /* Change position in a file. */
        case SYS_TELL:  /* Report current position in a file. */
        case SYS_CLOSE: /* Close a file. */
        default:
            printf("system call!\n");
            exit(-1);
    }
}

void exit(int status) {
    printf("%s: exit(%d)\n", thread_current()->name, status);
    thread_exit();
}

tid_t fork(const char *thread_name, struct intr_frame *f) {
    return process_fork(thread_name, f);
}
void halt(void) {
}
int exec(const char *file) {
}
int wait(pid_t) {
}
bool create(const char *file, unsigned initial_size) {
    return filesys_create(file, initial_size);
}

bool remove(const char *file) {
    return filesys_remove(file);   
}
int open(const char *file) {
}
int filesize(int fd) {
}
int read(int fd, void *buffer, unsigned length) {
}
int write(int fd, const void *buffer, unsigned length) {
}
void seek(int fd, unsigned position) {
}
unsigned tell(int fd) {
}
void close(int fd) {
}
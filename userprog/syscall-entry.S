#include "threads/loader.h"

.text
.globl syscall_entry
.type syscall_entry, @function
syscall_entry:
	movq %rbx, temp1(%rip)
	movq %r12, temp2(%rip)     /* callee saved registers */
	movq %rsp, %rbx            /* Store(stash) userland rsp    */
	movabs $tss, %r12			/*load Task State Segment to r12 */
	movq (%r12), %r12			/*read Kernel StackPointer at TSS */
	movq 4(%r12), %rsp         /* Read ring0 rsp from the tss , 커널 스택 영역으로 진입*/
	/* Now we are in the kernel stack */

	/* push to stack seg-registers value in userland*/
	push $(SEL_UDSEG)      /* if->ss */
	push %rbx              /* if->rsp */
	push %r11              /* if->eflags */
	push $(SEL_UCSEG)      /* if->cs */
	push %rcx              /* if->rip */
	subq $16, %rsp         /* skip error_code, vec_no */
	push $(SEL_UDSEG)      /* if->ds */
	push $(SEL_UDSEG)      /* if->es */

	push %rax
	movq temp1(%rip), %rbx
	push %rbx
	pushq $0

	/*시스템 호출에 필요한 레지스터들을 스택에 백업(rbx, rdx, rbp, rdi, rsi, r8, r9, r10, r11, r12, r13, r14, r15).*/
	push %rdx
	push %rbp
	push %rdi
	push %rsi
	push %r8
	push %r9
	push %r10
	pushq $0     /* skip r11 */
	movq temp2(%rip), %r12
	push %r12
	push %r13
	push %r14
	push %r15
	movq %rsp, %rdi

check_intr:
	btsq $9, %r11          /* Check whether we recover the interrupt */
	jnb no_sti			   /*no_sti는 직전에 btsq $9, %r11 명령을 통해 인터럽트 플래그를 검사하고, 조건에 따라 인터럽트를 설정(sti)할지 말지 결정한 후의 	경로입니다. 여기서 jnb no_sti (Jump if Not Below) 명령은 r11 레지스터의 9번째 비트가 설정되어 있지 않다면 no_sti 라벨로 점프합니다. 즉, 인터럽트를 다시 활성화하지 않고 시스템 호출 처리를 계속합니다.*/

	sti                    /* restore interrupt */

no_sti:                    /*no set interrupt*/
	movabs $syscall_handler, %r12
	call *%r12
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rsi
	popq %rdi
	popq %rbp
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rax
	addq $32, %rsp  	/*백업한 레지스터 복원하고 rsp 원상복구*/

	/*recover necessary information (RIP, EFLAGS, RSP) when returning to user space from the stack.*/
	popq %rcx              /* if->rip */
	addq $8, %rsp
	popq %r11              /* if->eflags */
	popq %rsp              /* if->rsp */

	/*sysretq command is used by the kernel to efficiently return to user space. This command is used to recover [rip, cs, rflags, rsp, ss] and optimize the return process.*/
	sysretq

.section .data
.globl temp1
temp1:
.quad	0
.globl temp2
temp2:
.quad	0

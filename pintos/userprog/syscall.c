#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/vaddr.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

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

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	printf ("system call!\n");
	thread_exit ();
}

void
check_addr(void * addr){
	struct thread *curr = thread_current();

	// NULL 포인터인지
	// 커널 공간을 가리키고 있는지
	// 페이지 테이블(pml4)에 매핑되어 있는지
	if (addr == NULL || !is_user_vaddr (addr) || pml4_get_page (curr->pml4, addr) == NULL)  exit (-1);
	return;
}

// 페이지 단위 검사하는(check_addr()함수 사용)함수 구현해야함
// -> read, write에서 사용
void
check_addr_page(void * buffer, size_t size){
	// 예외사항
	if(buffer == NULL) exit(-1);
	if(size ==0) return;

	const char *str_start = (const char *)buffer;
    const char *str_end = str_start + size - 1;	

	// 시작주소 ~ 끝주소까지 페이지 단위로 검사
	for(char *page = pg_round_down(str_start); page <= str_end; page += PGSIZE) check_addr(page);

	return;
}

// 1byte 단위로 검사하는 함수 구현해야함?
// -> fork, exec, create, remove, open에서 사용
void
check_addr_byte(char *string){
	// while(*string != '\0'){
	// 	check_addr((void *)string);
	// 	string++;
	// }

	// 
	check_addr((void *)string);
	while(*string != '\0'){
		string++;
		check_addr((void *)string);
	}
	
	return;
}

// pg_round_down(페이지에 들어가있는 주소?) -> 페이지의 시작점을 알려줌
// pg_round_up () -> 페이지의 끝을 알려줌



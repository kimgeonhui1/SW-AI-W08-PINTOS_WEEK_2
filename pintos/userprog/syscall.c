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
#include "threads/init.h"
#include "threads/synch.h"
#include "userprog/process.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

void check_addr(void * addr);
void check_addr_page(void * buffer, size_t size);
void check_addr_byte(char *string);

void halt(void);
void exit(int status);
int write(int fd, const void *buffer, unsigned size);

tid_t fork (const char *thread_name, struct intr_frame *f);
int exec (const char *cmd_line);
int wait (tid_t pid);

bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

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

void
syscall_init (void) {
	lock_init(&filesys_lock);
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
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.
	switch(f->R.rax){
        // 인자 0개
        case SYS_HALT:
            halt();
            break;

        // 인자 1개 (int status)
        case SYS_EXIT:
            exit((int)f->R.rdi);
            break;

        // 인자 3개 (int fd, void *buffer, unsigned size)
        case SYS_WRITE:
            f->R.rax = write((int)f->R.rdi, (void *)f->R.rsi, (unsigned)f->R.rdx);
            break;

        // 인자 2개 (const char *thread_name, struct intr_frame *f)
        case SYS_FORK:
            f->R.rax = fork((const char *)f->R.rdi, f);
            break;

        // 인자 1개 (const char *cmd_line)
        case SYS_EXEC:
            f->R.rax = exec((const char *)f->R.rdi);
            break;

        // 인자 1개 (tid_t pid)
        case SYS_WAIT:
            f->R.rax = wait((tid_t)f->R.rdi);
            break;

        // 인자 2개 (const char *file, unsigned initial_size)
        case SYS_CREATE:
            f->R.rax = create((const char *)f->R.rdi, (unsigned)f->R.rsi);
            break;

        // 인자 1개 (const char *file)
        case SYS_REMOVE:
            f->R.rax = remove((const char *)f->R.rdi);
            break;

        // 인자 1개 (const char *file)
        case SYS_OPEN:
            f->R.rax = open((const char *)f->R.rdi);
            break;

        // 인자 1개 (int fd)
        case SYS_FILESIZE:
            f->R.rax = filesize((int)f->R.rdi);
            break;

        // 인자 3개 (int fd, void *buffer, unsigned size)
        case SYS_READ:
            f->R.rax = read((int)f->R.rdi, (void *)f->R.rsi, (unsigned)f->R.rdx);
            break;

        // 인자 2개 (int fd, unsigned position)
        case SYS_SEEK:
            seek((int)f->R.rdi, (unsigned)f->R.rsi);
            break;

        // 인자 1개 (int fd)
        case SYS_TELL:
            f->R.rax = tell((int)f->R.rdi);
            break;

        // 인자 1개 (int fd)
        case SYS_CLOSE:
            close((int)f->R.rdi);
            break;

        default:
            exit(-1);
            break;
    }

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
	// pg_round_down(페이지에 들어가있는 주소?) -> 페이지의 시작점을 알려줌
	// pg_round_up () -> 페이지의 끝을 알려줌
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

void halt(void){
	power_off();
}

void exit(int status){
	thread_current() ->status_exit = status;



	// 현재 실행중인 스레드를 강제 종료시킴
	thread_exit();
}

int write(int fd, const void *buffer, unsigned size){
	check_addr_page((void *)buffer, size);

	if (fd == 1){
		putbuf((char *)buffer, size);
		return size;
	}
	else if (fd == 0){
		return -1;
	}
	
	// struct file *file = fd_to_file(fd);
    // if (file == NULL) {
    //     return -1;
    // }

    // lock_acquire(&filesys_lock); 
    // int64_t write_size = file_write(file, buffer, size);
    // lock_release(&filesys_lock);

    // return write_size;
	return -1;
}

tid_t fork (const char *thread_name, struct intr_frame *f) { return -1; }
int exec (const char *cmd_line) { return -1; }
int wait (tid_t pid) { 
	// if(!is_my_direct_child(pid)) return -1;

	return process_wait(pid);
}

bool create (const char *file, unsigned initial_size) { return false; }
bool remove (const char *file) { return false; }
int open (const char *file) { return -1; }
int filesize (int fd) { return -1; }
int read (int fd, void *buffer, unsigned size) { return -1; }
void seek (int fd, unsigned position) {}
unsigned tell (int fd) { return 0; }
void close (int fd) {}

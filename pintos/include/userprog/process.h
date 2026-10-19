#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

struct child_status{
    tid_t tid;                      // 어떤 자식인지 구별하기 위한 ID
    int exit_status;                // 자식이 남긴 종료 코드, 종료면 0 
    struct semaphore exit_sema;     // 자식이 끝날 때까지 부모를 기다리게 함
    struct list_elem elem;          // 부모의 자식 목록에 넣기 위한 연결 고리
};
#endif /* userprog/process.h */

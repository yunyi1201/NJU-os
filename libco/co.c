#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "common.h"

#define STACK_SIZE 64*1024


void Init() __attribute__((constructor));

enum co_status {
	CO_NEW = 1,      //新创建， 还未执行过
	CO_RUNNING,      //已经执行过
	CO_WAITING,      //在等待
	CO_DEAD,         //结束， 但未释放资源
};

struct co {
	const char* name;
	void (*entry) (void *arg);
	void *arg;
	enum co_status status;
	struct co *waiter;      //是否有其他线程在等待当前线程
	jmp_buf context;
	void *stack_bottom;
	void *stack_top;
	struct list_head link;
};

LIST_HEAD(head);
struct co pri;
struct co* current;

void Init() 
{
	//printf("before init\n");
	pri.name = "pri";
	pri.status = CO_RUNNING;
	//list_add(&pri.link, &head);
	current = &pri;
	//printf("after init\n");
}

struct co *co_start( const char* name, void (*entry) (void *), void *arg ) {
	struct co *task = (struct co*)malloc(sizeof(struct co));
	if( task == NULL ) 
	{
		fprintf(stderr, "mem out of limit\n");
		return NULL;
	}
	task->name = name;
	task->entry = entry;
	task->arg = arg;
	task->status = CO_NEW;
	task->waiter = NULL;
	task->stack_bottom = malloc(sizeof(char)*STACK_SIZE);
	if( task->stack_bottom == NULL )
	{
		fprintf(stderr, "mem out of limit\n");
		return NULL;
	}
	task->stack_top = (char*)(task->stack_bottom) + STACK_SIZE;
	list_add(&task->link, &head);	
	return task;
}

static struct co* choose_task( ) {
	struct co* task = NULL;
	list_for_each_entry(task, &head, link) {
		if(task->status != CO_DEAD && task->status != CO_WAITING) 
				break;
	}
	if( &task->link == &head )
			return NULL;
	list_del(&task->link);
	list_add_tail(&task->link, &head);
	return task;
}

static void free_task( struct co* co ) {
	list_del(&co->link);
	free(co->stack_bottom);
	free(co);
}

void co_wait( struct co *co ) {
	static int flag = 0;
	while(co->status != CO_DEAD) {	
		if(!flag) {
			co->waiter = current;
			current->status = CO_WAITING;
			flag = 1;
		}
		co_yield();
	}
	if(co->status == CO_DEAD) {
		printf("free: %s\n", co->name);
		current->status = CO_RUNNING;
		free_task(co);
		return ;
	}
}


void co_yield() {
	
	struct co* next = choose_task();
	if( next == NULL )
			return ;
	if(setjmp(current->context))
		return ;
	else {
		current = next;
		if( current->status == CO_NEW ) 
		{
			register void *sp = current->stack_top;
			current->status = CO_RUNNING;
			asm volatile (
				"movq %0, %%rsp \n"
				: :"b" ((uintptr_t)sp) 
			);
			current->entry(current->arg);
			current->status = CO_DEAD;		
			current = &pri;
			longjmp(current->context,1);
		} 
		else  
			longjmp(current->context, 1);
	}
}


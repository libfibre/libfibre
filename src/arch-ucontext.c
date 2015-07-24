#include <ucontext.h>
#include <errno.h>
#include "private.h"

#ifndef FIBRE_STACK_SIZE
#define FIBRE_STACK_SIZE (64*1024)
#endif

struct fibre_arch {
	ucontext_t ctx;
	int is_origin;
};

int fibre_arch_init(void)
{
	return 0;
}

void fibre_arch_finish(void)
{
}

int fibre_arch_origin(struct fibre_arch **aa)
{
	struct fibre_arch *a = malloc(sizeof(struct fibre_arch));
	if (!a)
		return -ENOMEM;
	a->is_origin = 1;
	*aa = a;
	return 0;
}

int fibre_arch_create(struct fibre_arch **aa, void (*fn)(void))
{
	void *stackspace;
	int ret;
	struct fibre_arch *a = malloc(sizeof(struct fibre_arch));
	if (!a)
		return -ENOMEM;
	ret = getcontext(&a->ctx);
	if (ret) {
		free(a);
		return ret;
	}
	a->is_origin = 0;
	stackspace = malloc(FIBRE_STACK_SIZE);
	if (!stackspace) {
		free(a);
		return -ENOMEM;
	}
	a->ctx.uc_stack.ss_sp = stackspace;
	a->ctx.uc_stack.ss_size = FIBRE_STACK_SIZE;
	a->ctx.uc_link = NULL;
	makecontext(&a->ctx, fn, 0);
	*aa = a;
	return 0;
}

void fibre_arch_destroy(struct fibre_arch *a)
{
	if (!a->is_origin)
		free(a->ctx.uc_stack.ss_sp);
	free(a);
}

void fibre_arch_switch(struct fibre_arch *dest, struct fibre_arch *src)
{
	FUNUSED int ret = swapcontext(&src->ctx, &dest->ctx);
	FCHECK(!ret);
}

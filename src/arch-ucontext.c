#include "private.h"

#ifndef FIBRE_STACK_SIZE
#define FIBRE_STACK_SIZE (64*1024*1024)
#endif

int fibre_arch_origin(struct fibre_arch *a)
{
	a->is_origin = 1;
	return getcontext(&a->ctx);
}

int fibre_arch_create(struct fibre_arch *a, void (*fn)(void))
{
	void *stackspace;
	int ret = getcontext(&a->ctx);
	if (ret)
		return ret;
	a->is_origin = 0;
	stackspace = malloc(FIBRE_STACK_SIZE);
	if (!stackspace)
		return -ENOMEM;
	a->ctx.uc_stack.ss_sp = stackspace;
	a->ctx.uc_stack.ss_size = FIBRE_STACK_SIZE;
	a->ctx.uc_link = NULL;
	makecontext(&a->ctx, fn, 0);
	return 0;
}

void fibre_arch_destroy(struct fibre_arch *a)
{
	if (!a->is_origin)
		free(a->ctx.uc_stack.ss_sp);
}

void fibre_arch_switch(struct fibre_arch *dest, struct fibre_arch *src)
{
	FUNUSED int ret = swapcontext(&src->ctx, &dest->ctx);
	FCHECK(!ret);
}

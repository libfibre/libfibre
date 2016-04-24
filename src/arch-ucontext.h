#include <ucontext.h>
#include <errno.h>

#ifndef FIBRE_STACK_SIZE
#define FIBRE_STACK_SIZE (64*1024)
#endif

struct fibre_arch {
	ucontext_t ctx;
	int is_origin;
};

static inline int fibre_arch_init(void)
{
	return 0;
}

static inline void fibre_arch_finish(void)
{
}

static inline int fibre_arch_origin(struct fibre_arch *a)
{
	a->is_origin = 1;
	return 0;
}

static inline int fibre_arch_create(struct fibre_arch *a, void (*fn)(void))
{
	void *stackspace;
	int ret;
	ret = getcontext(&a->ctx);
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

static inline void fibre_arch_destroy(struct fibre_arch *a)
{
	if (!a->is_origin)
		free(a->ctx.uc_stack.ss_sp);
}

static inline void fibre_arch_switch(struct fibre_arch *dest,
				     struct fibre_arch *src)
{
	FUNUSED int ret = swapcontext(&src->ctx, &dest->ctx);
	FCHECK(!ret);
}

#undef _FORTIFY_SOURCE /* Otherwise our longjmp()s break */
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <semaphore.h>

#ifndef FIBRE_STACK_SIZE
#define FIBRE_STACK_SIZE SIGSTKSZ
#endif

struct fibre_arch {
	jmp_buf jbuf;
	void (*fn)(void);
	stack_t stack;
	int is_origin;
};

struct global_state {
	/* We lazy initialise the semaphore when thread_count goes non-zero,
	 * and destroy it when it goes to zero. The mutex synchronizes the
	 * per-thread init/finish calls through this logic. */
	pthread_mutex_t lock;
	unsigned int thread_count;
	/* The semaphore used to serialize fibre creation, because it relies on
	 * global signal-handling tricks. */
	sem_t sem;
	/* And the per-fibre details (e.g. function to call) used when the
	 * signal-handler is creating the fibre. NB: the signal handler has to
	 * copy this to a local stack variable during initialization, because
	 * when it is later resumed this global data will be different. */
	struct fibre_arch *fibre;
	/* In order to get a clean stack frame despite the signal handler
	 * returning (which invalidates the stack frame), we longjmp() back
	 * into the trampoline post-signal and it calls a subroutine to create
	 * the required stack frame. This jmp_buf is used in that second step,
	 * to get back to the caller context from the parameter-less trampoline
	 * sub-routine. */
	jmp_buf callerctx;
};

#ifndef IN_FIBRE_C

extern struct global_state fibre_arch_global_state;
extern void fibre_arch_trampoline(int arg);

#else

struct global_state fibre_arch_global_state = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.thread_count = 0
};

static void local_trampoline_clean_stack_frame(void)
{
	struct fibre_arch *fibre = fibre_arch_global_state.fibre;
	if (setjmp(fibre->jbuf)) {
		fibre->fn();
		fprintf(stderr, "Critical: shouldn't get this far!!\n");
	}
	longjmp(fibre_arch_global_state.callerctx, 1);
}

void fibre_arch_trampoline(int arg)
{
	struct fibre_arch *fibre = fibre_arch_global_state.fibre;
	FCHECK(arg == SIGUSR1);
	if (!fibre) {
		fprintf(stderr, "Warning: spurious SIGUSR1 in libfibre\n");
		return;
	}
	if (setjmp(fibre->jbuf))
		local_trampoline_clean_stack_frame();
}

#endif

static inline int fibre_arch_init(void)
{
	int ret = pthread_mutex_lock(&fibre_arch_global_state.lock);
	if (ret)
		return ret;
	if (++fibre_arch_global_state.thread_count == 1) {
		ret = sem_init(&fibre_arch_global_state.sem, 0, 1);
		if (ret)
			fibre_arch_global_state.thread_count--;
	}
	pthread_mutex_unlock(&fibre_arch_global_state.lock);
	return ret;
}

static inline void fibre_arch_finish(void)
{
	FUNUSED int ret = pthread_mutex_lock(&fibre_arch_global_state.lock);
	FCHECK(!ret);
	if (--fibre_arch_global_state.thread_count == 0) {
		ret = sem_destroy(&fibre_arch_global_state.sem);
		FCHECK(!ret);
	}
	pthread_mutex_unlock(&fibre_arch_global_state.lock);
}

static inline int fibre_arch_origin(struct fibre_arch *a)
{
	a->is_origin = 1;
	return 0;
}

static inline int fibre_arch_create(struct fibre_arch *a, void (*fn)(void))
{
	struct sigaction sa, osa;
	stack_t ostack;
	int ret;
	FCHECK(fibre_arch_global_state.thread_count > 0);
	a->is_origin = 0;
	a->fn = fn;
	a->stack.ss_flags = 0;
	a->stack.ss_size = FIBRE_STACK_SIZE;
	a->stack.ss_sp = malloc(FIBRE_STACK_SIZE);
	if (!a->stack.ss_sp)
		return -ENOMEM;
	ret = sem_wait(&fibre_arch_global_state.sem);
	if (ret) {
		free(a->stack.ss_sp);
		return ret;
	}
	fibre_arch_global_state.fibre = a;
	ret = sigaltstack(&a->stack, &ostack);
	if (ret) {
		fibre_arch_global_state.fibre = NULL;
		sem_post(&fibre_arch_global_state.sem);
		free(a->stack.ss_sp);
		return ret;
	}
	sa.sa_handler = fibre_arch_trampoline;
	sa.sa_flags = SA_ONSTACK;
	sigemptyset(&sa.sa_mask);
	ret = sigaction(SIGUSR1, &sa, &osa);
	if (ret) {
		sigaltstack(&ostack, NULL);
		fibre_arch_global_state.fibre = NULL;
		sem_post(&fibre_arch_global_state.sem);
		free(a->stack.ss_sp);
		return ret;
	}
	/* Raise the signal and wait for the handler to finish. This triggers
	 * the trampoline sequence that creates the fibre. */
	ret = raise(SIGUSR1);
	sigaction(SIGUSR1, &osa, NULL);
	sigaltstack(&ostack, NULL);
	if (ret) {
		fibre_arch_global_state.fibre = NULL;
		sem_post(&fibre_arch_global_state.sem);
		free(a->stack.ss_sp);
		return ret;
	}
	/* The trampoline sequence through the signal handler has created a
	 * fibre context. But for weird "clean stack frame" reasons we now need
	 * to resume the fibre and have it caller a subroutine with no
	 * arguments, before it's bootstrapped and ready to be resumed at a
	 * later date (with no more global state and semaphoring required). */
	if (!setjmp(fibre_arch_global_state.callerctx))
		longjmp(a->jbuf, 1);
	fibre_arch_global_state.fibre = NULL;
	sem_post(&fibre_arch_global_state.sem);
	return 0;
}

static inline void fibre_arch_destroy(struct fibre_arch *a)
{
	if (!a->is_origin)
		free(a->stack.ss_sp);
}

static inline void fibre_arch_switch(struct fibre_arch *dest,
				     struct fibre_arch *src)
{
	if (!setjmp(src->jbuf))
		longjmp(dest->jbuf, 1);
}

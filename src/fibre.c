#include "private.h"

#include <stdio.h>

static __thread struct tls_fibre {
	int inited;
	struct fibre_selector *sstack;
	unsigned int async_atomic;
} tls_fibre;

struct fibre_selector {
	/* NULL when this selector is the bottom of the stack */
	struct fibre_selector *parent;
	/* The implementation of the selector */
	const struct fibre_selector_vtable *vtable;
	/* The implementation's per-selector state goes here */
	void *vtable_data;
	uint32_t async_mask;
};

int fibre_init(void)
{
	int ret;

	if (tls_fibre.inited)
		return -EALREADY;
	ret = fibre_arch_init();
	if (ret)
		return ret;
	tls_fibre.inited = 1;
	tls_fibre.sstack = NULL;
	tls_fibre.async_atomic = 0;
	return 0;
}

void fibre_finish(void)
{
	FCHECK(tls_fibre.inited);
	FCHECK(!tls_fibre.sstack);
	FCHECK(!tls_fibre.async_atomic);
	fibre_arch_finish();
	tls_fibre.inited = 0;
}

static void fibre_bootstrap(void)
{
	struct fibre *f = fibre_get_current();
	FCHECK(f);
	FCHECK(!(f->flags & FIBRE_FLAGS_STARTED));
	FCHECK(!(f->flags & FIBRE_FLAGS_COMPLETED));
	f->flags |= FIBRE_FLAGS_STARTED;
	f->fn(f->fn_arg);
	f->flags |= FIBRE_FLAGS_COMPLETED;
	fibre_schedule();
	FCHECK(NULL == "Should never reach here!");
}

int fibre_create(struct fibre **foo, void (*fn)(void *), void *d)
{
	struct fibre *f;
	int ret;
	f = malloc(sizeof(*f));
	if (!f)
		return -ENOMEM;
	f->flags = 0;
	ret = fibre_arch_create(&f->arch, fibre_bootstrap);
	if (ret) {
		free(f);
		return ret;
	}
	f->fn = fn;
	f->fn_arg = d;
	/* NB: we do *not* initialise f->userdata here. If the user calls
	 * fibre_get_userdata() before calling fibre_set_userdata(), we want the
	 * "valgrind"s and "purify"s of this world to notice it, which won't
	 * happen if we preinitialise the value here. */
	*foo = f;
	return 0;
}

int fibre_recreate(struct fibre *f, void (*fn)(void *), void *d)
{
	int ret;
	FCHECK(f->flags & FIBRE_FLAGS_COMPLETED);
	fibre_arch_destroy(f->arch);
	f->flags = 0;
	ret = fibre_arch_create(&f->arch, fibre_bootstrap);
	if (ret)
		return ret;
	f->fn = fn;
	f->fn_arg = d;
	return 0;
}

void fibre_destroy(struct fibre *f)
{
	FCHECK(!f->flags || (f->flags & FIBRE_FLAGS_COMPLETED));
	if (f->flags & FIBRE_FLAGS_COMPLETED)
		fibre_arch_destroy(f->arch);
	free(f);
}

void fibre_set_userdata(struct fibre *f, void *d)
{
	f->userdata = d;
}

void *fibre_get_userdata(struct fibre *f)
{
	return f->userdata;
}

struct fibre *fibre_get_current(void)
{
	struct fibre_selector *s = tls_fibre.sstack;
	FCHECK(s);
	return s->vtable->get_current(s->vtable_data);
}

int fibre_started(struct fibre *f)
{
	return (f->flags & FIBRE_FLAGS_STARTED);
}

int fibre_completed(struct fibre *f)
{
	return (f->flags & FIBRE_FLAGS_COMPLETED);
}

int fibre_push(struct fibre_selector *s)
{
	int ret;
	FCHECK(tls_fibre.inited);
	s->parent = tls_fibre.sstack;
	tls_fibre.sstack = s;
	ret = s->vtable->post_push(s->vtable_data);
	if (ret) {
		tls_fibre.sstack = s->parent;
		s->parent = NULL;
	}
	return ret;
}

int fibre_pop(struct fibre_selector **foo)
{
	struct fibre_selector *s = tls_fibre.sstack;
	int ret;
	FCHECK(s);
	ret = s->vtable->pre_pop(s->vtable_data);
	if (ret)
		return ret;
	tls_fibre.sstack = s->parent;
	s->parent = NULL;
	if (foo)
		*foo = s;
	return 0;
}

void fibre_selector_free(struct fibre_selector *s)
{
	FCHECK(!s->parent);
	s->vtable->destroy(s->vtable_data);
	free(s);
}

int fibre_can_switch_explicit(void)
{
	FCHECK(tls_fibre.sstack);
	return tls_fibre.sstack->vtable->can_switch_explicit(
				tls_fibre.sstack->vtable_data);
}

int fibre_can_switch_implicit(void)
{
	FCHECK(tls_fibre.sstack);
	return tls_fibre.sstack->vtable->can_switch_implicit(
				tls_fibre.sstack->vtable_data);
}

void fibre_schedule_to(struct fibre *f)
{
	FCHECK(tls_fibre.sstack);
	FCHECK(fibre_can_switch_explicit());
	FCHECK(!(f->flags & FIBRE_FLAGS_COMPLETED));
	tls_fibre.sstack->vtable->schedule(tls_fibre.sstack->vtable_data, f);
}

void fibre_schedule(void)
{
	FCHECK(tls_fibre.sstack);
	FCHECK(fibre_can_switch_implicit());
	tls_fibre.sstack->vtable->schedule(tls_fibre.sstack->vtable_data, NULL);
}

struct fibre_selector *fibre_selector_alloc(
				const struct fibre_selector_vtable *v,
				void *vd)
{
	struct fibre_selector *f = malloc(sizeof(*f));
	if (f) {
		f->parent = NULL;
		f->vtable = v;
		f->vtable_data = vd;
		f->async_mask = 0;
	}
	return f;
}

void fibre_async_set_mask(uint32_t mask)
{
	FCHECK(tls_fibre.sstack);
	tls_fibre.sstack->async_mask = mask;
}

int fibre_async_can_suspend(uint32_t method)
{
	struct fibre_selector *s = tls_fibre.sstack;
	return (!tls_fibre.async_atomic && s && (s->async_mask & method) &&
				s->vtable->can_switch_implicit(s->vtable_data));
}

void fibre_async_atomicity_up(void)
{
	tls_fibre.async_atomic++;
}

void fibre_async_atomicity_down(void)
{
	FCHECK(tls_fibre.async_atomic);
	tls_fibre.async_atomic--;
}

int fibre_async_suspend_poll(void)
{
	struct fibre *f;
	FCHECK(fibre_async_can_suspend(FIBRE_ASYNC_POLL));
	f = fibre_get_current();
	FCHECK(!f->async);
	f->async = FIBRE_ASYNC_POLL;
	f->async_abort = 0;
	fibre_schedule();
	f->async = 0;
	return f->async_abort ? -EINTR : 0;
}

int fibre_async_suspend_fd_readable(int fd)
{
	struct fibre *f;
	FCHECK(fibre_async_can_suspend(FIBRE_ASYNC_FD_READABLE));
	f = fibre_get_current();
	FCHECK(!f->async);
	f->async = FIBRE_ASYNC_FD_READABLE;
	f->async_fd_readable.fd = fd;
	f->async_abort = 0;
	fibre_schedule();
	f->async = 0;
	return f->async_abort ? -EINTR : 0;
}

int fibre_async_suspend_use_cb(void *arg, int (*cb)(void *))
{
	struct fibre *f;
	FCHECK(fibre_async_can_suspend(FIBRE_ASYNC_CHECK_CB));
	f = fibre_get_current();
	FCHECK(!f->async);
	f->async = FIBRE_ASYNC_CHECK_CB;
	f->async_check_cb.cb_arg = arg;
	f->async_check_cb.cb = cb;
	f->async_abort = 0;
	fibre_schedule();
	f->async = 0;
	return f->async_abort ? -EINTR : 0;
}

uint32_t fibre_async_type(struct fibre *f)
{
	return f->async;
}

void fibre_async_get_fd_readable(struct fibre *f, int *fd)
{
	FCHECK(f->async == FIBRE_ASYNC_FD_READABLE);
	*fd = f->async_fd_readable.fd;
}

void fibre_async_get_use_cb(struct fibre *f, void **arg, int (**cb)(void *))
{
	FCHECK(f->async == FIBRE_ASYNC_CHECK_CB);
	*arg = f->async_check_cb.cb_arg;
	*cb = f->async_check_cb.cb;
}

void fibre_async_abort(struct fibre *f)
{
	FCHECK(f->async);
	f->async_abort = 1;
}

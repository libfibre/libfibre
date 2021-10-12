#ifdef HEADER_SRC_PRIVATE_H
#error "Why is src/private.h being included twice?"
#endif
#define HEADER_SRC_PRIVATE_H

#include <fibre.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef FIBRE_RUNTIME_CHECK
#include <assert.h>
#define FCHECK(cond) assert(cond)
#define FUNUSED
#else
#define FCHECK(cond)
#define FUNUSED __attribute__((unused))
#endif

/* The arch-<whatever> implementations will define this; */
struct fibre_arch;

/* That platform-specific support in fibre_arch.h will provide the following
 * hooks; NB: we are inlining the fibre_arch struct definition and these hooks,
 * because it helps performance, therefore we can't actually predeclare these.
 * But it's useful to have the prototypes. these are commented out */
#if 0
int fibre_arch_init(void);
void fibre_arch_finish(void);
int fibre_arch_origin(struct fibre_arch *);
int fibre_arch_create(struct fibre_arch *, void (*fn)(void));
void fibre_arch_destroy(struct fibre_arch *);
void fibre_arch_switch(struct fibre_arch *dest, struct fibre_arch *src);
#endif
/* This file is auto-generated into the output directory from the appropriate
 * src/arch-<foo>.h, where <foo>=$(FIBRE_ARCH). */
#include <fibre_arch.h>

/* The fibre structure;
 *  arch: the platform-specific meat.
 *  flags: FIBRE_FLAGS_* bitmask.
 */
struct fibre {
	struct fibre_arch arch;
	unsigned int flags;
	void (*fn)(void *);
	void *fn_arg;
	void *userdata;
	uint32_t async; /* Zero if not suspended, otherwise FIBRE_ASYNC_* */
	int async_abort;
	union {
		struct fibre_async_fd_readable {
			int fd;
		} async_fd_readable;
		struct fibre_async_check_cb {
			void *cb_arg;
			int (*cb)(void *);
		} async_check_cb;
	};
};
#define FIBRE_FLAGS_STARTED   0x1
#define FIBRE_FLAGS_COMPLETED 0x2

struct fibre_selector_vtable {
	void (*destroy)(void *vtable_data);
	/* When the selector is pushed, this post-processing hook runs
	 * before the fibre_push() call returns). */
	int (*post_push)(void *vtable_data);
	/* When fibre_pop() is called, this pre-processing hook can deny the
	 * operation if the state is inconsistent with a pop. Return zero to
	 * succeed. */
	int (*pre_pop)(void *vtable_data);
	/* Return non-zero for TRUE */
	int (*can_switch_explicit)(void *vtable_data);
	int (*can_switch_implicit)(void *vtable_data);
	/* For implicit scheduling, f==NULL */
	void (*schedule)(void *vtable_data, struct fibre *f);
	/* Should return NULL iff the currently-executing fibre is the
	 * "origin" that pushed this selector on to the stack. */
	struct fibre *(*get_current)(void *vtable_data);
};

/* When a vtable implementation implements its API constructor, it uses this
 * internal API to allocate a new selector struct and latch its vtable and
 * vtable_data to it. */
struct fibre_selector *fibre_selector_alloc(
			 	const struct fibre_selector_vtable *,
				void *);

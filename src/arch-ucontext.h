#include <ucontext.h>
#include <errno.h>

/* These are the arch-specific dependences we need to provide. The non-trivial
 * ones are implemented in arch-ucontext.c. */
static inline int fibre_arch_init(void)
{
	return 0;
}
static inline void fibre_arch_finish(void)
{
}
int fibre_arch_origin(struct fibre_arch *);
int fibre_arch_create(struct fibre_arch *, void (*fn)(void));
void fibre_arch_destroy(struct fibre_arch *);
void fibre_arch_switch(struct fibre_arch *dest, struct fibre_arch *src);

struct fibre_arch {
	ucontext_t ctx;
	int is_origin;
};

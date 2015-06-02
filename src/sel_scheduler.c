#include "private.h"

struct vd {
	struct fibre_arch origin;
	struct fibre *current;
	struct fibre *(*cb)(void *);
	void *cb_arg;
	int allow_explicit;
};

static void ss_destroy(void *__vd)
{
	FUNUSED struct vd *vd = __vd;
	FCHECK(!vd->current);
}

static int ss_post_push(void *__vd)
{
	struct vd *vd = __vd;
	int ret = fibre_arch_origin(&vd->origin);
	vd->current = NULL;
	return ret;
}

static int ss_pre_pop(void *__vd)
{
	struct vd *vd = __vd;
	if (vd->current)
		return -EBUSY;
	return 0;
}

static int ss_can_switch_explicit(void *__vd)
{
	struct vd *vd = __vd;
	return vd->allow_explicit;
}

static int ss_can_switch_implicit(void *__vd)
{
	return 1;
}

static void ss_schedule(void *__vd, struct fibre *f)
{
	struct vd *vd = __vd;
	struct fibre_arch *s, *d;
	FCHECK(vd->allow_explicit || !f);
	if (vd->current)
		s = &vd->current->arch;
	else
		s = &vd->origin;
	if (!f)
		f = vd->cb(vd->cb_arg);
	if (f) {
		d = &f->arch;
		vd->current = f;
	} else {
		if (!vd->current)
			/* We're being asked to switch to the origin, but we're
			 * already the origin... */
			return;
		d = &vd->origin;
		vd->current = NULL;
	}
	fibre_arch_switch(d, s);
}

static struct fibre *ss_get_current(void *__vd)
{
	struct vd *vd = __vd;
	return vd->current;
}

static const struct fibre_selector_vtable ss_vt = {
	.destroy = ss_destroy,
	.post_push = ss_post_push,
	.pre_pop = ss_pre_pop,
	.can_switch_explicit = ss_can_switch_explicit,
	.can_switch_implicit = ss_can_switch_implicit,
	.schedule = ss_schedule,
	.get_current = ss_get_current
};

int fibre_selector_scheduler(struct fibre_selector **foo,
			     struct fibre *(*cb_scheduler)(void *cb_arg),
			     void *cb_arg,
			     int allow_explicit)
{
	struct fibre_selector *s;
	struct vd *vd = malloc(sizeof(*vd));
	if (!vd)
		return -ENOMEM;
	vd->cb = cb_scheduler;
	vd->cb_arg = cb_arg;
	vd->allow_explicit = allow_explicit;
	s = fibre_selector_alloc(&ss_vt, vd);
	if (!s) {
		free(vd);
		return -ENOMEM;
	}
	*foo = s;
	return 0;
}

#include "private.h"

struct vd {
	struct fibre_arch *origin;
	struct fibre *current;
};

static void so_destroy(void *__vd)
{
	FUNUSED struct vd *vd = __vd;
	FCHECK(!vd->current);
	free(vd);
}

static int so_post_push(void *__vd)
{
	struct vd *vd = __vd;
	int ret = fibre_arch_origin(&vd->origin);
	vd->current = NULL;
	return ret;
}

static int so_pre_pop(void *__vd)
{
	struct vd *vd = __vd;
	if (vd->current)
		return -EBUSY;
	fibre_arch_destroy(vd->origin);
	return 0;
}

static int so_can_switch_explicit(void *vd)
{
	return 1;
}

static int so_can_switch_implicit(void *__vd)
{
	struct vd *vd = __vd;
	return !!vd->current;
}

static void so_schedule(void *__vd, struct fibre *f)
{
	struct vd *vd = __vd;
	struct fibre_arch *s, *d;
	FCHECK(vd->current || f);
	if (vd->current)
		s = vd->current->arch;
	else
		s = vd->origin;
	if (f) {
		d = f->arch;
		vd->current = f;
	} else {
		d = vd->origin;
		vd->current = NULL;
	}
	fibre_arch_switch(d, s);
}

static struct fibre *so_get_current(void *__vd)
{
	struct vd *vd = __vd;
	return vd->current;
}

static const struct fibre_selector_vtable so_vt = {
	.destroy = so_destroy,
	.post_push = so_post_push,
	.pre_pop = so_pre_pop,
	.can_switch_explicit = so_can_switch_explicit,
	.can_switch_implicit = so_can_switch_implicit,
	.schedule = so_schedule,
	.get_current = so_get_current
};

int fibre_selector_origin(struct fibre_selector **foo)
{
	struct fibre_selector *s;
	struct vd *vd = malloc(sizeof(*vd));
	if (!vd)
		return -ENOMEM;
	s = fibre_selector_alloc(&so_vt, vd);
	if (!s) {
		free(vd);
		return -ENOMEM;
	}
	*foo = s;
	return 0;
}

#include <fibre.h>

#include <stdio.h>
#include <assert.h>

#define DO_TRACE

#ifdef DO_TRACE
#define TRACE(a)	printf a
#define TEST_FIBRE_TARGET 300
#else
#define TRACE(a)	do { ; } while (0)
#define TEST_FIBRE_TARGET 10000000
#endif

struct testfoo {
	unsigned int tgt;
	unsigned int c;
	struct fibre *f1;
	struct fibre *f2;
};

static void f1(void *__foo)
{
	struct testfoo *foo = (struct testfoo *)__foo;
	TRACE(("f1 starting\n"));
	while ((++foo->c) < foo->tgt) {
		TRACE(("f1 (%d) switching to f2\n", foo->c));
		fibre_schedule_to(foo->f2);
	}
	TRACE(("f1 ending\n"));
}

static void f2(void *__foo)
{
	struct testfoo *foo = (struct testfoo *)__foo;
	TRACE(("f2 starting\n"));
	while ((++foo->c) < foo->tgt) {
		TRACE(("f2 (%d) switching to f1\n", foo->c));
		fibre_schedule_to(foo->f1);
	}
	TRACE(("f2 ending\n"));
}

int main(int argc, char *argv[])
{
	struct testfoo foo = {
		.tgt = TEST_FIBRE_TARGET,
		.c = 0
	};
	struct fibre_selector *se;
	int ret;

	ret = fibre_init();
	assert(!ret);

	ret = fibre_selector_origin(&se);
	assert(!ret && se);

	ret = fibre_create(&foo.f1, f1, &foo);
	assert(!ret);
	ret = fibre_create(&foo.f2, f2, &foo);
	assert(!ret);
	assert(foo.f1 && foo.f2);

	ret = fibre_push(se);
	assert(!ret);

	TRACE(("Switching to f1\n"));
	fibre_schedule_to(foo.f1);
	TRACE(("Control returned to main()\n"));

	if (fibre_started(foo.f1) && !fibre_completed(foo.f1)) {
		TRACE(("f1 incomplete, rerunning\n"));
		fibre_schedule_to(foo.f1);
	}
	if (fibre_started(foo.f2) && !fibre_completed(foo.f2)) {
		TRACE(("f2 incomplete, rerunning\n"));
		fibre_schedule_to(foo.f2);
	}

	ret = fibre_pop(NULL);
	assert(!ret);

	fibre_destroy(foo.f1);
	fibre_destroy(foo.f2);
	fibre_selector_free(se);
	return 0;
}

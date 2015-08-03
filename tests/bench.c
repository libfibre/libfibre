/* Benchmark logic

 * We create 'n' fibres (n is command-line overridable), n-1 of which are
 * blindly forwarding control to the next fibre (we want these to add the
 * minimum possible overhead relative to the fibre-switching we are trying to
 * measure), and 1 fibre is "counting".
 *
 * Furthermore, we compile in support for two instances of this logic. One
 * instance uses real fibres, the other performs a straw-man comparison that
 * replaces the fibre_schedule_to() function call with a stub function call
 * that does a bare minimum of "state-machine" updating. This state-machine
 * update code is in a different file to protect against optimisation/inlining,
 * so the function calling overhead is comparable to fibre_schedule_to().
 *
 * - The benchmark results will include the overheads of context-switching as
 *   well as the overheads of function entry/exit, so the straw-man will help
 *   to compare these overheads.
 * - We are ultimately comparing two methods of implementing an asynchronous
 *   stack (or converting a synchronous stack to asynchronous). One involves
 *   conventional function calls where state-machine data-structures "track"
 *   asynchronous states, and the other involves "run-to-completion"-like
 *   code executing in a fibre and using the fibre-switching to provide
 *   asynchronicity. From this perspective, it makes sense for the straw-man
 *   (which is a function that is called and returns with each timeslice) to
 *   update a state-machine data-structure between invocations, because this
 *   is the overhead that the fibre-based approach does not have.
 *
 * So we have 2 modes;
 *   - fibre       - do the processing in fibres
 *   - straw       - do straw-man processing without fibres
 *
 * And for both of those modes, there are two "fibre" routines;
 *   -  blind      - blindly move to the next "fibre" ((n-1) of these)
 *   -  counter    - increment counter and test for termination (1 of these)
 */

#include <fibre.h>
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>

//#define TRACE_ME


/* Type-safety */
#define MALLOC(t)    (t *)malloc(sizeof(t))
#define MALLOCn(t,n) (t *)malloc((n) * sizeof(t))
#define FREE(t,p) \
do { \
	t *__tmp_p = (p); \
	free(__tmp_p); \
} while (0)
#define MEMCPY(t,d,s,n) \
do { \
	t *__tmp_d = (d); \
	const t *__tmp_s = (s); \
	memcpy(__tmp_d, __tmp_s, (n) * sizeof(*__tmp_d)); \
} while (0)

/****************/
/* Mode "fibre" */
/****************/

struct ctx_fibre_blind {
	struct fibre *me;
	struct fibre *next;
#ifdef TRACE_ME
	unsigned int whoami;
#endif
};

struct ctx_fibre_counter {
	unsigned int countdown;
	struct fibre *me;
	struct fibre *next;
};

static void fn_fibre_blind(void *__foo)
{
	struct ctx_fibre_blind *ctx = (struct ctx_fibre_blind *)__foo;
	struct fibre *next = ctx->next;
	while (1) {
#ifdef TRACE_ME
		printf("fn_fibre_blind(%u)\n", ctx->whoami);
#endif
		fibre_schedule_to(next);
	}
}

static void fn_fibre_counter(void *__foo)
{
	struct ctx_fibre_counter *ctx = (struct ctx_fibre_counter *)__foo;
	struct fibre *next = ctx->next;
	unsigned int countdown = ctx->countdown;
	do {
#ifdef TRACE_ME
		printf("fn_fibre_counter(), countdown=%u\n", countdown);
#endif
		fibre_schedule_to(next);
	} while (--countdown);
}

/* Globals. Initialised in setup_fibre(), used in start_fibre() */
static struct ctx_fibre_blind *ctxf_b;
static struct ctx_fibre_counter *ctxf_c;

static void setup_fibre(unsigned long num_fibres, unsigned long num_loops)
{
	int ret;
	unsigned long loop;
	struct fibre_selector *se;

	assert(num_fibres >= 2);

	ctxf_b = MALLOCn(struct ctx_fibre_blind, num_fibres - 1);
	ctxf_c = MALLOC(struct ctx_fibre_counter);
	assert(ctxf_b && ctxf_c);

	ret = fibre_init();
	assert(!ret);

	/* Allocate the fibres */
	for (loop = 0; loop < num_fibres - 1; loop++) {
		struct ctx_fibre_blind *bb = &ctxf_b[loop];
		ret = fibre_create(&bb->me, fn_fibre_blind, bb);
		assert(!ret);
#ifdef TRACE_ME
		bb->whoami = loop;
#endif
	}
	ret = fibre_create(&ctxf_c->me, fn_fibre_counter, ctxf_c);
	assert(!ret);

	/* Configure the fibres (the skipping) */
	for (loop = 0; loop < num_fibres - 2; loop++)
		ctxf_b[loop].next = ctxf_b[loop + 1].me;
	ctxf_b[num_fibres - 2].next = ctxf_c->me;
	ctxf_c->next = ctxf_b[0].me;
	ctxf_c->countdown = num_loops;

	ret = fibre_selector_origin(&se);
	assert(!ret && se);
	ret = fibre_push(se);
	assert(!ret);
}

static void start_fibre(void)
{
	/* Start! */
	fibre_schedule_to(ctxf_b[0].me);

	/* Note, we don't gracefully tear anything down. In particular, the
	 * fibre routines are not implemented to support a shutdown, because
	 * that would necessarily involve some conditional check in those fibre
	 * routines to support it. We are interested only in measuring the raw
	 * fibre-switching speeds.
	 */
}

/****************/
/* Mode "straw" */
/****************/

struct ctx_straw {
	struct ctx_straw *(*fn)(struct ctx_straw *);
	/* The 'fn' returns the next context for the dispatcher to turn to.
	 * When 'fn' is the blind function, it simply returns 'next'. When 'fn'
	 * is the counter function, it returns 'next' until the countdown
	 * terminates, at which point it returns NULL. */
	struct ctx_straw *next;
	struct state_machine sm;
	/* Only used in the 'counter' */
	unsigned int countdown;
#ifdef TRACE_ME
	unsigned int whoami;
#endif
};

static struct ctx_straw *fn_straw_blind(struct ctx_straw *p)
{
	state_machine_update(&p->sm);
#ifdef TRACE_ME
	printf("fn_straw_blind(%u)\n", p->whoami);
#endif
	return p->next;
}

static struct ctx_straw *fn_straw_counter(struct ctx_straw *p)
{
	state_machine_update(&p->sm);
#ifdef TRACE_ME
	printf("fn_straw_counter(), countdown=%u\n", p->countdown);
#endif
	if (--p->countdown)
		return p->next;
	return NULL;
}

static struct ctx_straw *ctxs;

static void setup_straw(unsigned long num_fibres, unsigned num_loops)
{
	unsigned long loop;

	assert(num_fibres >= 1);
	ctxs = MALLOCn(struct ctx_straw, num_fibres);
	assert(ctxs);

	for (loop = 0; loop < num_fibres; loop++) {
		ctxs[loop].fn = (loop + 1) < num_fibres ?
			fn_straw_blind : fn_straw_counter;
		ctxs[loop].next = (loop + 1) < num_fibres ?
			&ctxs[loop + 1] : &ctxs[0];
		ctxs[loop].countdown = num_loops;
#ifdef TRACE_ME
		ctxs[loop].whoami = loop;
#endif
	}
}

static void start_straw(void)
{
	struct ctx_straw *ctx = ctxs;

	while (1) {
		ctx = ctx->fn(ctx);
		if (!ctx)
			return;
	}
}

/********/
/* Main */
/********/

/* Annoying. I want a printf that puts commas between thousands, millions, etc.
 * Seeing as I'm doing this, do the right-aligning stuff too. */
#define MY_PRINTF_ALIGN 40
static void my_padding(const char *prefix)
{
	unsigned int pref = strlen(prefix);
	if (pref > MY_PRINTF_ALIGN)
		pref = 0;
	else
		pref = MY_PRINTF_ALIGN - pref;
	while (pref--)
		putchar(' ');
}
static void my_ul_printf(const char *prefix, unsigned long arg)
{
	char sfull[32], spartial[5];
	unsigned int lfull = 0, lpartial;
	unsigned long partial;
	sfull[31] = '\0';
	do {
		partial = arg % 1000;
		arg /= 1000;
		if (arg)
			lpartial = sprintf(spartial, ",%03lu", partial);
		else
			lpartial = sprintf(spartial, "%lu", partial);
		lfull += lpartial;
		memcpy(&sfull[31 - lfull], spartial, lpartial);
	} while (arg);
	my_padding(prefix);
	printf("%s: %s\n", prefix, &sfull[31 - lfull]);
}
static void my_str_printf(const char *prefix, const char *arg)
{
	my_padding(prefix);
	printf("%s: %s\n", prefix, arg);
}

#define ARG_INC() ({++argv; --argc; (argc ? *argv : NULL);})
#define NEED_ARG(__p) \
do { \
	const char *p = (__p); \
	s = ARG_INC(); \
	if (!s) { \
		fprintf(stderr, "'%s' needs argument\n", p); \
		return -1; \
	} \
} while (0)
int main(int argc, char *argv[])
{
	struct rusage before, after;
	int res, is_straw = 0;
	unsigned long num_fibres, num_loops, num_switch;
	unsigned long utime, stime;
	double persec;
	const char *s;

#ifdef TRACE_ME
	num_fibres = 4;
	num_loops = 5;
#else
	num_fibres = 100;
	num_loops = 1000000;
#endif

	/* TODO: getopt this */
	while ((s = ARG_INC())) {
		if (!strcmp(s, "-f") || !strcmp(s, "--fibres")) {
			NEED_ARG(s);
			num_fibres = atoi(s);
		} else if (!strcmp(s, "-l") || !strcmp(s, "--loops")) {
			NEED_ARG(s);
			num_loops = atoi(s);
		} else if (!strcmp(s, "-s") || !strcmp(s, "--straw")) {
			is_straw = 1;
		} else {
			fprintf(stderr, "Unrecognised option: %s\n", s);
			return -1;
		}
	}

	if (is_straw)
		setup_straw(num_fibres, num_loops);
	else
		setup_fibre(num_fibres, num_loops);

	printf("Starting...\n");
	res = getrusage(RUSAGE_SELF, &before);
	assert(!res);

	if (is_straw)
		start_straw();
	else
		start_fibre();

	res = getrusage(RUSAGE_SELF, &after);
	assert(!res);
	printf("Complete\n");

	num_switch = num_loops * num_fibres;
	printf("Config:\n");
	my_str_printf("Run-time model", is_straw ? "straw-man" : "fibres");
	my_ul_printf("Number of contexts", num_fibres);
	my_ul_printf("Number of loops", num_loops);
	printf("Measurements:\n");
	utime = (unsigned long)after.ru_utime.tv_sec * 1e6 +
		after.ru_utime.tv_usec -
		(unsigned long)before.ru_utime.tv_sec * 1e6 -
		before.ru_utime.tv_usec;
	stime = (unsigned long)after.ru_stime.tv_sec * 1e6 +
		after.ru_stime.tv_usec -
		(unsigned long)before.ru_stime.tv_sec * 1e6 -
		before.ru_stime.tv_usec;
	my_ul_printf("Number of usecs in user", utime);
	my_ul_printf("Number of usecs in system", stime);
	printf("Results:\n");
	my_ul_printf("Number of context switches", num_switch);
	/* Calculate switches-per-second, using usertime + systime */
	persec = (double)num_switch / (utime + stime) * 1000000;
	/* I know, this rounds down, it's not an oversight. */
	my_ul_printf("context switches per sec", (unsigned long)persec);

	return 0;	
}

#include <assert.h>

/*******************
 * FULL DISCLOSURE *
 *******************
 *
 * The following excerpt of code is taken verbatim from evanjones.ca,
 * specifically;
 *    http://www.evanjones.ca/software/threading.html
 *
 * Thanks to its use of American english ("fiber"), I can just leave the
 * "fiber" typedef and the associated low-level primitives untouched, and
 * implement the libfibre stuff on top of that. (See after the excerpt
 * section.) Note, this excerpt is from the "libfiber-asm.c" file of Evan's
 * tarball, cropped to leave only the lowest-level ASM stuff.
 *
 * Actually, the excerpt is not quite verbatim. To avoid;
 *    "warning: declaration of ‘fiber’ shadows a global declaration [-Wshadow]"
 *    I changed the typedef from 'fiber' to '_fiber'.
 * And to allow this code to be a header and multiply-included, we #ifdef
 * the static declarations using IN_FIBRE_C.
 * And to allow _that_, we change "create_stack" to not be static, and to
 * give it better name-spacing, like fibre_arch_create_stack.
 */

/*****************/
/* BEGIN EXCERPT */
/*****************/

/* The Fiber Structure
*  Contains the information about individual fibers.
*/
typedef struct
{
	void** stack; /* The stack pointer */
	void* stack_bottom; /* The original returned from malloc. */
	int active;
} _fiber;

/* Prototype for the assembly function to switch processes. */
extern int asm_switch(_fiber* next, _fiber* current, int return_value);
void fibre_arch_create_stack(_fiber* fiber, int stack_size, void (*fptr)(void));
extern void* asm_call_fiber_exit;

#ifdef IN_FIBRE_C

#ifdef __APPLE__
#define ASM_PREFIX "_"
#else
#define ASM_PREFIX ""
#endif

/* Used to handle the correct stack alignment on Mac OS X, which requires a
16-byte aligned stack. The process returns here from its "main" function,
leaving the stack at 16-byte alignment. The call instruction then places a
return address on the stack, making the stack correctly aligned for the
process_exit function. */
asm(".globl " ASM_PREFIX "asm_call_fiber_exit\n"
ASM_PREFIX "asm_call_fiber_exit:\n"
/*"\t.type asm_call_fiber_exit, @function\n"*/
"\tcall " ASM_PREFIX "fiber_exit\n");

void fibre_arch_create_stack(_fiber* fiber, int stack_size, void (*fptr)(void)) {
	int i;
#ifdef __x86_64
	/* x86-64: rbx, rbp, r12, r13, r14, r15 */
	static const int NUM_REGISTERS = 6;
#else
	/* x86: ebx, ebp, edi, esi */
	static const int NUM_REGISTERS = 4;
#endif
	assert(stack_size > 0);
	assert(fptr != NULL);

	/* Create a 16-byte aligned stack which will work on Mac OS X. */
	assert(stack_size % 16 == 0);
	fiber->stack_bottom = malloc(stack_size);
	if (fiber->stack_bottom == 0) return;
	fiber->stack = (void**)((char*) fiber->stack_bottom + stack_size);
#ifdef __APPLE__
	assert((uintptr_t) fiber->stack % 16 == 0);
#endif

	/* 4 bytes below 16-byte alignment: mac os x wants return address here
	so this points to a call instruction. */
	*(--fiber->stack) = (void*) ((uintptr_t) &asm_call_fiber_exit);
	/* 8 bytes below 16-byte alignment: will "return" to start this function */
	*(--fiber->stack) = (void*) ((uintptr_t) fptr);  /* Cast to avoid ISO C warnings. */
	/* push NULL words to initialize the registers loaded by asm_switch */
	for (i = 0; i < NUM_REGISTERS; ++i) {
		*(--fiber->stack) = 0;
	}
}

#ifdef __x86_64
/* arguments in rdi, rsi, rdx */
asm(".globl " ASM_PREFIX "asm_switch\n"
ASM_PREFIX "asm_switch:\n"
#ifndef __APPLE__
"\t.type asm_switch, @function\n"
#endif
/* Move return value into rax */
"\tmovq %rdx, %rax\n"

/* save registers: rbx rbp r12 r13 r14 r15 (rsp into structure) */
"\tpushq %rbx\n"
"\tpushq %rbp\n"
"\tpushq %r12\n"
"\tpushq %r13\n"
"\tpushq %r14\n"
"\tpushq %r15\n"
"\tmovq %rsp, (%rsi)\n"

/* restore registers */
"\tmovq (%rdi), %rsp\n"
"\tpopq %r15\n"
"\tpopq %r14\n"
"\tpopq %r13\n"
"\tpopq %r12\n"
"\tpopq %rbp\n"
"\tpopq %rbx\n"

/* return to the "next" fiber with eax set to return_value */
"\tret\n");
#else
/* static int asm_switch(fiber* next, fiber* current, int return_value); */
asm(".globl " ASM_PREFIX "asm_switch\n"
ASM_PREFIX "asm_switch:\n"
#ifndef __APPLE__
"\t.type asm_switch, @function\n"
#endif
/* Move return value into eax, current pointer into ecx, next pointer into edx */
"\tmov 12(%esp), %eax\n"
"\tmov 8(%esp), %ecx\n"
"\tmov 4(%esp), %edx\n"

/* save registers: ebx ebp esi edi (esp into structure) */
"\tpush %ebx\n"
"\tpush %ebp\n"
"\tpush %esi\n"
"\tpush %edi\n"
"\tmov %esp, (%ecx)\n"

/* restore registers */
"\tmov (%edx), %esp\n"
"\tpop %edi\n"
"\tpop %esi\n"
"\tpop %ebp\n"
"\tpop %ebx\n"

/* return to the "next" fiber with eax set to return_value */
"\tret\n");
#endif

#endif

/***************/
/* END EXCERPT */
/***************/

#ifndef FIBRE_STACK_SIZE
#define FIBRE_STACK_SIZE (64*1024)
#endif

/* This hook is needed from the above code, but it should never be executed in
 * our model, so we craft it appropriately. */
#ifdef IN_FIBRE_C
void fiber_exit(void)
{
	fprintf(stderr, "Critical: 'fiber_exit' hook called?\n");
	abort();
}
#endif

struct fibre_arch {
	_fiber ctx;
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
	a->is_origin = 0;
	fibre_arch_create_stack(&a->ctx, FIBRE_STACK_SIZE, fn);
	if (!a->ctx.stack_bottom)
		return -ENOMEM;
	return 0;
}

static inline void fibre_arch_destroy(struct fibre_arch *a)
{
	if (!a->is_origin)
		free(a->ctx.stack_bottom);
}

static inline void fibre_arch_switch(struct fibre_arch *dest,
				     struct fibre_arch *src)
{
	asm_switch(&dest->ctx, &src->ctx, 0);
}

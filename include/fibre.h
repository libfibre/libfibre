#ifndef HEADER_FIBRE_H
#define HEADER_FIBRE_H

#include <stdint.h>

/* Reference-counted opaque data-structure */
struct fibre;

/* Each thread should call this before any calls to other fibre APIs. */
int fibre_init(void);
/* If a thread is to exit without leaks, it should call this from the 'origin'
 * fibre prior to termination. */
void fibre_finish(void);

/* Create a fibre (and prepare it to run the given function with the given
 * argument).
 *
 * Note, when the given fibre function returns, the fibre library
 * will perform the equivalent of fibre_schedule(), which implies that the
 * top-most selector in the selector-stack must support implicit switching.
 * 
 * WARNING: there is *NO* soft-handling possible for such an error condition,
 * so undefined behaviour would result. If the fibre library is built with
 * runtime debugging, then appropriate diagnostics will be emitted and an
 * assertion will result.  If your fibre function is executing within a
 * questionable app (or library), and it needs to catch and report this buggy
 * circumstance even in non-debugging builds (prior to the unavoidable
 * nastiness that will follow if you don't trigger some kind of abort), then
 * call fibre_can_switch_implicit() before the fibre function returns and
 * treat a zero return value as a bug.
 */
int fibre_create(struct fibre **, void (*fn)(void *), void *);
/* If a fibre has completed, it can be reinitialised for reuse (equivalent to
 * calling fibre_destroy() and then fibre_create(), but saves on memory
 * (re)allocation). */
int fibre_recreate(struct fibre *, void (*fn)(void *), void *);
/* Destroy a fibre, should only occur on a fibre that was never invoked or
 * that has completed. */
void fibre_destroy(struct fibre *);

/* Associate user-defined data with a fibre. The assumption is that the
 * application and all participating libraries have a common understanding of
 * what this per-fibre data represents. Care should be used if multiple
 * libraries/modules define their own interpretations of this per-fibre data.
 * NB: one good use of this interface is to permit constant-time lookup when
 * implementing a "scheduler" selector. Ie. it may want to record something
 * about the existing fibre before scheduling to another, in which case a call
 * to fibre_get_current() followed by fibre_get_userdata() could track back to
 * the scheduler-specific data for the current fibre. */
void fibre_set_userdata(struct fibre *, void *);
void *fibre_get_userdata(struct fibre *);

/* Return a handle to the currently-scheduled fibre. This will return NULL
 * iff the currently-executing fibre is the "origin" of the top-most selector
 * in the selector stack. */
struct fibre *fibre_get_current(void);

/* Returns zero if the fibre has not yet been invoked. */
int fibre_started(struct fibre *);
/* Returns zero if the fibre has not yet completed. */
int fibre_completed(struct fibre *);

/* Fibre selectors provide different kinds of "scheduler"/switching decisions
 * and options. They can be pushed and popped to a thread-local stack to
 * allow for reuse and stacking (and independence) by different coding
 * layers. No switching is possible until at least one selector is on the
 * stack. */
struct fibre_selector;

/* Push/pop selectors to/from the thread-local selector stack. */
int fibre_push(struct fibre_selector *);
int fibre_pop(struct fibre_selector **);

/* This function can destroy any type of selector. */
void fibre_selector_free(struct fibre_selector *);

/* Some selectors insist on choosing the switched-to fibre, in which case
 * fibre_can_switch_explicit() will return 0/FALSE. Similarly, a selector may
 * require explicit switching, in which case fibre_can_switch_implicit()
 * returns 0/FALSE. */
int fibre_can_switch_explicit(void);
int fibre_can_switch_implicit(void);

/* Explicit and implicit context-switching. */
void fibre_schedule_to(struct fibre *);
void fibre_schedule(void);

/*
 * Different selector types.
 */

/* An "origin" selector. Once pushed, and so long as this selector is
 * top-most in the selector stack, explicit scheduling via
 * fibre_schedule_to() is always allowed, and implicit scheduling via
 * fibre_schedule() (which includes the case where a fibre returns rather
 * than calling any scheduling API) will cause control to return to the
 * 'origin' fibre, which is the one that pushed the selector onto the stack
 * and made the first call to fibre_schedule_to(). As such, implicit
 * scheduling is not allowed from the 'origin' fibre itself. */
int fibre_selector_origin(struct fibre_selector **);

/* A "scheduler" selector. Once pushed, and so long as this selector is
 * top-most in the selector stack, the given scheduling callback will be
 * invoked to determine the appropriate destination fibre. */
int fibre_selector_scheduler(struct fibre_selector **,
			     struct fibre *(*cb_scheduler)(void *cb_arg),
			     void *cb_arg,
			     int allow_explicit);

/*
 * Fibre "async" support
 *
 * The APIs declared so far provide the fibre implementation. The following APIs
 * provide an "async" extension, which is a technique for "asynchronising"
 * otherwise-synchronous code.
 */

/* At some higher API level (perhaps the app on top of a stack) advertises what
 * "types" of asynchronous events it will accept. It pushes a selector that
 * allows implicit switching, and then calls fibre_async_set_mask() with a
 * non-zero FIBRE_ASYNC_* mask to indicate the kinds of completion/resumption
 * methods the application/dispatcher can handle (these will be used by the
 * "asynchronous events" to suspend/resume fibres of activity). */

#define FIBRE_ASYNC_POLL        0x01
#define FIBRE_ASYNC_FD_READABLE 0x02
#define FIBRE_ASYNC_CHECK_CB    0x04

void fibre_async_set_mask(uint32_t mask);

/* At some lower API level, a function with synchronous semantics can be
 * modified to insert an asynchronous point. Eg. for functions that;
 *   * are to be offloaded,
 *   * but where blocking to wait for completion and results would negate the
 *     point of offloading,
 *   * and yet the rest of the codebase assumes completion once that function
 *     returns...
 * It calls fibre_async_can_suspend() with the type of completion method it
 * wishes to use, and this will return non-zero if;
 *   * the current fibre can be suspended and control handed back (implicit
 *     scheduling only) to a higher-level selector,
 *   * the completion method is supported by the same layer that pushed the
 *     selector,
 *   * that critical-section locking doesn't currently prevent suspension of
 *     the fibre. (See following section.)
 * Async support is scoped to the top-most selector in the selector stack. If
 * you locally push a new selector, but want to benefit from the async support
 * of the parent selector in the stack, you should support "proxying". */

int fibre_async_can_suspend(uint32_t method);

/* To avoid deadlocks, any thread-locking strategy (that synchronises multiple
 * threads against race conditions) that can be expressed in terms of critical
 * sections or mutual exclusions turns out to be a reasonable fibre-locking
 * strategy too. Consider, if all threads were preemptively scheduling onto a
 * single CPU core (hardware thread) and the size and order of the timeslices
 * for each thread were irregular, then the same "when it's safe and not safe to
 * suspend the current fibre and run another" problem applies to "when it's safe
 * and not safe to preempt the current thread and resume another". The steps to
 * temporarily disable thread-preemption in the latter are sufficient to
 * temporarily disable fibre-suspension in the former.
 *
 * Any exclusive locking or critical section code should increment the atomicity
 * counter when it's not safe to suspend, and decrement when it is (or might
 * be). This will influence the result of fibre_async_can_suspend().
 */

void fibre_async_atomicity_up(void);
void fibre_async_atomicity_down(void);

/* Suspend the current fibre, with different prototypes for each kind of
 * completion method that a suspension can use. */

/* FIBRE_ASYNC_POLL: there is no completion method, per se, the higher API level
 * can/should just resume the fibre whenever it likes, in a retry fashion. This
 * function returns zero when the fibre is resumed normally after first being
 * suspended, or -EINTR if resumed abnormally following fibre_async_abort(). */
int fibre_async_suspend_poll(void);

/* FIBRE_ASYNC_FD_READABLE: the higher API level should resume this fibre
 * normally only once the given file-descriptor is readable. Return value of
 * zero or -EINTR as with _poll(). In the error case, readability of the FD is
 * not guaranteed. */
int fibre_async_suspend_fd_readable(int fd);

/* FIBRE_ASYNC_CHECK_CB: the higher API level should resume this fibre normally
 * only once a call to the given callback (with the argument that is to be
 * passed to it) returns non-zero. Higher API level code promises to stop
 * invoking the callback for this suspension event as soon as it has returned
 * non-zero once. Return value zero or -EINTR as with _poll(). */
int fibre_async_suspend_use_cb(void *arg, int (*cb)(void *));

/* For a fibre that has been suspended due to a fibre_async_suspend_*() API,
 * this obtains the completion method that was used. */
uint32_t fibre_async_type(struct fibre *);

/* For methods besides FIBRES_ASYNC_POLL, these APIs can extract the completion
 * details. */
void fibre_async_get_fd_readable(struct fibre *, int *fd);
void fibre_async_get_use_cb(struct fibre *, void **arg, int (**cb)(void *));

/* Prior to an "async" fibre being resumed, the 'abort' API can set an attribute
 * such that the fibre will see a +1 return value from its 'suspend' call. The
 * purpose of this feature is to provide cancellability of async operations. If
 * the suspending code sees this and the operation it is waiting on is not
 * complete, then it should assume an underlying failure rather than suspending
 * again for the same reason. */
void fibre_async_abort(struct fibre *);

#endif

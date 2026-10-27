#define _GNU_SOURCE
#include "scheduler.h"
#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <poll.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <unistd.h>

/* A logical worker owns one scheduling thread and a pool of execution KLTs.
 * A suspended ULT stays attached to its KLT until it finishes. */
enum { EXEC_IDLE, EXEC_RUNNING, EXEC_PREEMPTED, EXEC_YIELDED, EXEC_DONE, EXEC_STOP };
struct pco_executor {
    pthread_t thread;
    _Atomic int state;
    _Atomic int started;
    pid_t tid;
    int start_error;
    ult_t *ult;
    struct pco_worker_runtime *worker;
    struct pco_executor *next;
    void *signal_stack;
};
struct pco_worker_runtime {
    klt_t *klt;
    klt_config_t config;
    struct pco_executor *executors;
    size_t count;
    unsigned long total_created, peak_klts, preemptions;
    unsigned long preempt_switches, pool_exhaustions, signal_errors;
    int last_signal_error;
    size_t parked;
    int was_exhausted, growing;
    int wake_fd, manager_fd;
    pthread_t manager;
    int manager_stop, growth_requested, growth_error;
    struct list_node garbage;
    struct pco_worker_runtime *next;
};

static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static struct list_node global_queue;
static struct pco_worker_runtime *workers;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static int initialization_error;
static _Thread_local struct pco_executor *current_executor;
extern void pco_set_current_klt(klt_t *);

static int preempt_signal(void) { return SIGRTMIN + 6; }
static void wake_fd(int fd) { uint64_t one = 1; ssize_t n; do { n = write(fd, &one, sizeof(one)); } while (n < 0 && errno == EINTR); }
static void drain_fd(int fd) { uint64_t v; ssize_t n; do { n = read(fd, &v, sizeof(v)); } while (n == sizeof(v) || (n < 0 && errno == EINTR)); }
static int futex_wait(_Atomic int *p, int value)
{ return syscall(SYS_futex, (int *)p, FUTEX_WAIT_PRIVATE, value, NULL, NULL, 0); }
static void futex_wake(_Atomic int *p)
{ (void)syscall(SYS_futex, (int *)p, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0); }

static void block_preempt(void)
{
    sigset_t set;
    sigemptyset(&set); sigaddset(&set, preempt_signal());
    pthread_sigmask(SIG_BLOCK, &set, NULL);
}
void pco_preempt_unblock(void)
{
    sigset_t set;
    sigemptyset(&set); sigaddset(&set, preempt_signal());
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}
static void guard_enter(sigset_t *old)
{
    sigset_t set;
    sigemptyset(&set); sigaddset(&set, preempt_signal());
    pthread_sigmask(SIG_BLOCK, &set, old);
}
static void guard_leave(const sigset_t *old) { pthread_sigmask(SIG_SETMASK, old, NULL); }

static void preempt_handler(int signal_number)
{
    int saved_errno = errno;
    struct pco_executor *exec = current_executor;
    int expected = EXEC_RUNNING;
    (void)signal_number;
    if (exec && atomic_compare_exchange_strong_explicit(&exec->state, &expected,
                EXEC_PREEMPTED, memory_order_acq_rel, memory_order_relaxed)) {
        wake_fd(exec->worker->wake_fd);
        while (atomic_load_explicit(&exec->state, memory_order_acquire) == EXEC_PREEMPTED)
            futex_wait(&exec->state, EXEC_PREEMPTED);
    }
    errno = saved_errno;
}

static void initialize(void)
{
    struct sigaction sa = {0};
    init_list_node(&global_queue);
    struct sigaction previous;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = preempt_handler;
    sa.sa_flags = SA_RESTART | SA_ONSTACK;
    if (preempt_signal() > SIGRTMAX) initialization_error = EINVAL;
    else if (sigaction(preempt_signal(), NULL, &previous) != 0) initialization_error = errno;
    else if (previous.sa_handler != SIG_DFL) initialization_error = EBUSY;
    else if (sigaction(preempt_signal(), &sa, NULL) != 0) initialization_error = errno;
}
void init_globalq(void) { pthread_once(&init_once, initialize); }

ult_t *ult_current(void)
{ return current_executor ? current_executor->ult : NULL; }

static void notify_workers(void)
{
    for (struct pco_worker_runtime *r = workers; r; r = r->next) wake_fd(r->wake_fd);
}

int ult_put(ult_t *ult)
{
    sigset_t old;
    if (!ult) return EINVAL;
    init_globalq();
    guard_enter(&old);
    pthread_mutex_lock(&queue_lock);
    if (ult->queue_state != 0) {
        pthread_mutex_unlock(&queue_lock); guard_leave(&old); return EALREADY;
    }
    ult->queue_state = 1;
    list_push_front(&global_queue, &ult->queue_node);
    notify_workers();
    pthread_mutex_unlock(&queue_lock);
    guard_leave(&old);
    return 0;
}
int ult_put_localq(ult_t *ult, klt_t *klt)
{
    sigset_t old;
    if (!ult || !klt) return EINVAL;
    guard_enter(&old);
    pthread_mutex_lock(&queue_lock);
    if (ult->queue_state != 0 || klt->stopping) {
        int error = klt->stopping ? EINVAL : EALREADY;
        pthread_mutex_unlock(&queue_lock); guard_leave(&old); return error;
    }
    ult->queue_state = 1;
    list_push_front(&klt->local_list, &ult->queue_node);
    wake_fd(klt->runtime->wake_fd);
    pthread_mutex_unlock(&queue_lock);
    guard_leave(&old);
    return 0;
}
void klt_request_stop(klt_t *klt)
{
    sigset_t old;
    guard_enter(&old);
    pthread_mutex_lock(&queue_lock);
    klt->stopping = 1;
    wake_fd(klt->runtime->wake_fd);
    pthread_mutex_unlock(&queue_lock);
    guard_leave(&old);
}

static void *executor_main(void *arg)
{
    struct pco_executor *exec = arg;
    stack_t alt = { .ss_sp = exec->signal_stack, .ss_size = 65536, .ss_flags = 0 };
    block_preempt();
    if (sigaltstack(&alt, NULL) != 0) exec->start_error = errno;
    exec->tid = (pid_t)syscall(SYS_gettid);
    atomic_store_explicit(&exec->started, 1, memory_order_release);
    futex_wake(&exec->started);
    if (exec->start_error) return NULL;
    current_executor = exec;
    pco_set_current_klt(exec->worker->klt);
    for (;;) {
        int state;
        while ((state = atomic_load_explicit(&exec->state, memory_order_acquire)) != EXEC_RUNNING && state != EXEC_STOP)
            futex_wait(&exec->state, state);
        if (state == EXEC_STOP) break;
        ult_t *ult = exec->ult;
        tb_context_from_t from = tb_context_jump(ult->stk.sp, ult);
        /* The ULT blocks the signal before switching back here. */
        if (!ult->finished) ult->stk.sp = from.ctx;
        atomic_store_explicit(&exec->state, ult->finished ? EXEC_DONE : EXEC_YIELDED, memory_order_release);
        wake_fd(exec->worker->wake_fd);
    }
    current_executor = NULL;
    pco_set_current_klt(NULL);
    alt.ss_flags = SS_DISABLE;
    (void)sigaltstack(&alt, NULL);
    return NULL;
}

static struct pco_executor *new_executor(struct pco_worker_runtime *r, int *error)
{
    struct pco_executor *exec = calloc(1, sizeof(*exec));
    if (!exec) { *error = ENOMEM; return NULL; }
    exec->signal_stack = malloc(65536);
    if (!exec->signal_stack) { free(exec); *error = ENOMEM; return NULL; }
    exec->worker = r;
    atomic_init(&exec->state, EXEC_IDLE);
    atomic_init(&exec->started, 0);
    *error = pthread_create(&exec->thread, NULL, executor_main, exec);
    if (*error) { free(exec->signal_stack); free(exec); return NULL; }
    while (!atomic_load_explicit(&exec->started, memory_order_acquire)) futex_wait(&exec->started, 0);
    if (exec->start_error) {
        *error = exec->start_error; pthread_join(exec->thread, NULL);
        free(exec->signal_stack); free(exec); return NULL;
    }
    return exec;
}

/* Allocation/reclamation are kept off the coordinator: a suspended ULT may
 * have interrupted libc while holding its internal allocator lock. */
static void *manager_main(void *arg)
{
    struct pco_worker_runtime *r = arg;
    struct pollfd pfd = { .fd = r->manager_fd, .events = POLLIN };
    block_preempt();
    for (;;) {
        (void)poll(&pfd, 1, -1);
        drain_fd(r->manager_fd);
        for (;;) {
            struct list_node *node;
            int grow, stop;
            pthread_mutex_lock(&queue_lock);
            node = list_pop_back(&r->garbage);
            grow = !node && r->growth_requested && !r->manager_stop;
            if (grow) { r->growth_requested = 0; r->growing = 1; }
            stop = r->manager_stop;
            pthread_mutex_unlock(&queue_lock);
            if (node) {
                ult_t *ult = container_of(node, ult_t, queue_node);
                free(ult->stk.space); free(ult);
                continue;
            }
            if (grow) {
                int error = 0;
                struct pco_executor *exec = new_executor(r, &error);
                pthread_mutex_lock(&queue_lock);
                r->growing = 0;
                if (exec) {
                    exec->next = r->executors; r->executors = exec;
                    ++r->count; ++r->total_created;
                    if (r->count > r->peak_klts) r->peak_klts = r->count;
                    r->growth_error = 0;
                } else r->growth_error = error;
                wake_fd(r->wake_fd);
                pthread_mutex_unlock(&queue_lock);
                continue;
            }
            if (stop) return NULL;
            break;
        }
    }
}

int pco_worker_init(klt_t *klt, const klt_config_t *config)
{
    int err = 0;
    init_globalq();
    if (initialization_error) return initialization_error;
    struct pco_worker_runtime *r = calloc(1, sizeof(*r));
    if (!r) return ENOMEM;
    r->klt = klt; r->config = *config;
    init_list_node(&r->garbage);
    r->wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    r->manager_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (r->wake_fd < 0 || r->manager_fd < 0) {
        err = errno; if (r->wake_fd >= 0) close(r->wake_fd);
        if (r->manager_fd >= 0) close(r->manager_fd); free(r); return err;
    }
    struct pco_executor *exec = new_executor(r, &err);
    if (!exec) { close(r->wake_fd); close(r->manager_fd); free(r); return err; }
    r->executors = exec; r->count = r->total_created = r->peak_klts = 1;
    /* A spare makes cooperative yield useful without changing KLT identity. */
    if (!config->max_klts || config->max_klts > 1) {
        struct pco_executor *spare = new_executor(r, &err);
        if (spare) { spare->next = r->executors; r->executors = spare;
                     r->count = r->total_created = r->peak_klts = 2; }
    }
    klt->runtime = r;
    err = pthread_create(&r->manager, NULL, manager_main, r);
    if (err) {
        while (r->executors) { exec = r->executors; r->executors = exec->next;
            atomic_store(&exec->state, EXEC_STOP); futex_wake(&exec->state);
            pthread_join(exec->thread, NULL); free(exec->signal_stack); free(exec); }
        close(r->wake_fd); close(r->manager_fd); free(r); klt->runtime = NULL; return err;
    }
    sigset_t old;
    guard_enter(&old);
    pthread_mutex_lock(&queue_lock);
    r->next = workers; workers = r;
    pthread_mutex_unlock(&queue_lock);
    guard_leave(&old);
    return 0;
}

void pco_worker_dispose(klt_t *klt)
{
    struct pco_worker_runtime *r = klt->runtime;
    if (!r) return;
    sigset_t old;
    guard_enter(&old);
    pthread_mutex_lock(&queue_lock);
    struct pco_worker_runtime **link = &workers;
    while (*link && *link != r) link = &(*link)->next;
    if (*link) *link = r->next;
    r->manager_stop = 1;
    wake_fd(r->manager_fd);
    pthread_mutex_unlock(&queue_lock);
    guard_leave(&old);
    pthread_join(r->manager, NULL);
    struct pco_executor *exec = r->executors;
    while (exec) {
        struct pco_executor *next = exec->next;
        atomic_store_explicit(&exec->state, EXEC_STOP, memory_order_release);
        futex_wake(&exec->state); pthread_join(exec->thread, NULL);
        free(exec->signal_stack); free(exec); exec = next;
    }
    close(r->wake_fd); close(r->manager_fd); free(r); klt->runtime = NULL;
}

int pco_worker_stats(klt_t *klt, klt_stats_t *stats)
{
    sigset_t old;
    if (!klt || !stats) return EINVAL;
    guard_enter(&old);
    pthread_mutex_lock(&queue_lock);
    *stats = (klt_stats_t){klt->runtime->total_created, klt->runtime->peak_klts, klt->runtime->preemptions,
                           klt->runtime->preempt_switches, klt->runtime->pool_exhaustions,
                           klt->runtime->signal_errors, klt->runtime->last_signal_error,
                           klt->runtime->growth_error};
    pthread_mutex_unlock(&queue_lock);
    guard_leave(&old);
    return 0;
}

static struct list_node *pop_local_runnable(klt_t *klt, int have_idle)
{
    struct list_node *head = &klt->local_list;
    for (struct list_node *node = head->prev; node != head; node = node->prev) {
        ult_t *ult = container_of(node, ult_t, queue_node);
        if (ult->executor || have_idle) {
            node->prev->next = node->next;
            node->next->prev = node->prev;
            node->next = node->prev = NULL;
            return node;
        }
    }
    return NULL;
}
static int local_needs_executor(klt_t *klt)
{
    struct list_node *head = &klt->local_list;
    for (struct list_node *node = head->next; node != head; node = node->next)
        if (!container_of(node, ult_t, queue_node)->executor) return 1;
    return 0;
}

void klt_loop0(klt_t *klt)
{
    struct pco_worker_runtime *r = klt->runtime;
    struct pco_executor *active = NULL, *previous_preempted = NULL;
    struct timespec deadline = {0};
    int signal_sent = 0;
    block_preempt();
    for (;;) {
        struct pco_executor *idle = NULL, *selected = NULL;
        struct list_node *node = NULL;
        pthread_mutex_lock(&queue_lock);
        if (active) {
            int state = atomic_load_explicit(&active->state, memory_order_acquire);
            if (state == EXEC_PREEMPTED || state == EXEC_YIELDED) {
                ult_t *ult = active->ult;
                if (state == EXEC_PREEMPTED) { ++r->preemptions; previous_preempted = active; }
                if (state == EXEC_YIELDED && ult->parking == 1) {
                    ult->parking = 2;
                    ult->queue_state = 3;
                    ++r->parked;
                } else {
                    ult->parking = 0;
                    ult->queue_state = 1;
                    list_push_front(&klt->local_list, &ult->queue_node);
                }
                active = NULL;
            } else if (state == EXEC_DONE) {
                ult_t *ult = active->ult;
                active->ult = NULL;
                atomic_store_explicit(&active->state, EXEC_IDLE, memory_order_release);
                list_push_front(&r->garbage, &ult->queue_node);
                wake_fd(r->manager_fd);
                active = NULL;
            }
        }
        if (!active) {
            for (struct pco_executor *e = r->executors; e; e = e->next)
                if (atomic_load_explicit(&e->state, memory_order_acquire) == EXEC_IDLE) { idle = e; break; }
            int needs_executor = ((!klt->stopping && !list_empty(&global_queue)) || local_needs_executor(klt)) && !idle;
            int at_limit = needs_executor && r->config.max_klts && r->count >= r->config.max_klts;
            if (at_limit && !r->was_exhausted) ++r->pool_exhaustions;
            r->was_exhausted = at_limit;
            if (needs_executor && !at_limit && !r->growth_requested && !r->growing && !r->growth_error) {
                r->growth_requested = 1; wake_fd(r->manager_fd);
            }
            if (!klt->stopping && idle && klt->prefer_global) node = list_pop_back(&global_queue);
            if (!node) node = pop_local_runnable(klt, idle != NULL);
            if (!node && !klt->stopping && idle) node = list_pop_back(&global_queue);
            klt->prefer_global = !klt->prefer_global;
            if (node) {
                ult_t *ult = container_of(node, ult_t, queue_node);
                selected = ult->executor;
                if (!selected) { selected = idle; selected->ult = ult; ult->executor = selected; }
                ult->queue_state = 2; ult->klt = klt; ult->bound_klt = 1;
                if (previous_preempted) {
                    if (selected != previous_preempted) ++r->preempt_switches;
                    previous_preempted = NULL;
                }
                active = selected;
                signal_sent = 0;
                if (r->config.quantum_ns) {
                    clock_gettime(CLOCK_MONOTONIC, &deadline);
                    deadline.tv_sec += r->config.quantum_ns / 1000000000UL;
                    deadline.tv_nsec += r->config.quantum_ns % 1000000000UL;
                    if (deadline.tv_nsec >= 1000000000L) { ++deadline.tv_sec; deadline.tv_nsec -= 1000000000L; }
                }
                atomic_store_explicit(&active->state, EXEC_RUNNING, memory_order_release);
                futex_wake(&active->state);
            } else if (klt->stopping && list_empty(&klt->local_list) && !r->parked) {
                pthread_mutex_unlock(&queue_lock);
                return;
            }
        }
        pthread_mutex_unlock(&queue_lock);
        struct pollfd pfd = { .fd = r->wake_fd, .events = POLLIN };
        struct timespec timeout, *timeoutp = NULL;
        if (active && r->config.quantum_ns && !signal_sent) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            timeout.tv_sec = deadline.tv_sec - now.tv_sec;
            timeout.tv_nsec = deadline.tv_nsec - now.tv_nsec;
            if (timeout.tv_nsec < 0) { --timeout.tv_sec; timeout.tv_nsec += 1000000000L; }
            if (timeout.tv_sec < 0) timeout.tv_sec = timeout.tv_nsec = 0;
            timeoutp = &timeout;
        }
        int ready = ppoll(&pfd, 1, timeoutp, NULL);
        if (ready > 0) drain_fd(r->wake_fd);
        if (active && r->config.quantum_ns && !signal_sent &&
            atomic_load_explicit(&active->state, memory_order_acquire) == EXEC_RUNNING) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (now.tv_sec > deadline.tv_sec ||
                (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) {
                if (syscall(SYS_tgkill, getpid(), active->tid, preempt_signal()) == 0) {
                    signal_sent = 1;
                } else {
                    int error = errno;
                    pthread_mutex_lock(&queue_lock);
                    ++r->signal_errors; r->last_signal_error = error;
                    pthread_mutex_unlock(&queue_lock);
                    /* Back off for one quantum instead of spinning or silently
                     * stopping the clock after a failed delivery. */
                    deadline = now;
                    deadline.tv_sec += r->config.quantum_ns / 1000000000UL;
                    deadline.tv_nsec += r->config.quantum_ns % 1000000000UL;
                    if (deadline.tv_nsec >= 1000000000L) {
                        ++deadline.tv_sec; deadline.tv_nsec -= 1000000000L;
                    }
                }
            }
        }
    }
}

/* Completion sets ready before calling unpark. The queue lock serializes
 * registration, transition to parked, and a wake arriving before the jump. */
void pco_park_if_pending(const _Atomic int *ready)
{
    ult_t *ult = ult_current();
    if (!ult || !ready) return;
    block_preempt();
    pthread_mutex_lock(&queue_lock);
    if (atomic_load_explicit(ready, memory_order_acquire)) {
        pthread_mutex_unlock(&queue_lock);
        pco_preempt_unblock();
        return;
    }
    ult->parking = 1;
    pthread_mutex_unlock(&queue_lock);
    tb_context_from_t from = tb_context_jump(ult->sched_ctx, NULL);
    ult->sched_ctx = from.ctx;
    pco_preempt_unblock();
}
void pco_unpark(ult_t *ult)
{
    sigset_t old;
    if (!ult) return;
    guard_enter(&old);
    pthread_mutex_lock(&queue_lock);
    if (ult->parking == 1) ult->parking = 3;
    else if (ult->parking == 2) {
        ult->parking = 0;
        ult->queue_state = 1;
        --ult->klt->runtime->parked;
        list_push_front(&ult->klt->local_list, &ult->queue_node);
        wake_fd(ult->klt->runtime->wake_fd);
    }
    pthread_mutex_unlock(&queue_lock);
    guard_leave(&old);
}

void sched(void)
{
    ult_t *ult = ult_current();
    if (!ult) return;
    block_preempt();
    tb_context_from_t from = tb_context_jump(ult->sched_ctx, NULL);
    ult->sched_ctx = from.ctx;
    pco_preempt_unblock();
}
void ult_finish(void)
{
    ult_t *ult = ult_current();
    if (!ult) abort();
    block_preempt();
    ult->finished = 1;
    tb_context_jump(ult->sched_ctx, NULL);
    abort();
}

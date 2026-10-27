#include "pco_io.h"
#include "scheduler.h"
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <unistd.h>

struct pco_io_request {
    struct pco_io *io;
    struct pco_io_request *next;
    pco_io_operation_t operation;
    pco_io_result_t result;
    atomic_int done;
    /* Protected by io->lock. */
    int running;
    ult_t *waiter;
};

struct pco_io {
    pthread_mutex_t lock;
    pthread_cond_t ready, timer_ready;
    pthread_t *workers, timer_thread;
    size_t worker_count, queue_capacity, queued, outstanding;
    struct pco_io_request *head, *tail, *timers;
    int stopping;
    int completion_fd;
};

extern void pco_park_if_pending(const _Atomic int *ready);
extern void pco_unpark(ult_t *ult);

/* The lock keeps the pool and notification fd alive until notification is
 * sent, even if another thread sees completion and releases the request. */
static void complete_locked(struct pco_io_request *r, ssize_t value, int error)
{
    r->result.value = value;
    r->result.error = error;
    atomic_store_explicit(&r->done, 1, memory_order_release);
    if (r->waiter) pco_unpark(r->waiter);
    uint64_t one = 1;
    ssize_t ignored;
    do { ignored = write(r->io->completion_fd, &one, sizeof(one)); }
    while (ignored < 0 && errno == EINTR);
    (void)ignored; /* A saturated nonblocking eventfd is already readable. */
}

static void execute(struct pco_io_request *r)
{
    const pco_io_operation_t *o = &r->operation;
    ssize_t value;
    switch (o->kind) {
    case PCO_IO_READ: value = read(o->fd, o->buffer, o->length); break;
    case PCO_IO_WRITE: value = write(o->fd, o->buffer, o->length); break;
    case PCO_IO_PREAD: value = pread(o->fd, o->buffer, o->length, o->offset); break;
    case PCO_IO_PWRITE: value = pwrite(o->fd, o->buffer, o->length, o->offset); break;
    case PCO_IO_RECV: value = recv(o->fd, o->buffer, o->length, o->flags); break;
    case PCO_IO_SEND: value = send(o->fd, o->buffer, o->length, o->flags); break;
    case PCO_IO_ACCEPT: value = accept(o->fd, o->address, o->address_length); break;
    case PCO_IO_CONNECT: value = connect(o->fd, o->address, o->connect_length); break;
    default: value = -1; errno = EINVAL; break;
    }
    int error = value < 0 ? errno : 0;
    pthread_mutex_lock(&r->io->lock);
    complete_locked(r, value, error);
    pthread_mutex_unlock(&r->io->lock);
}

static void *worker_main(void *arg)
{
    struct pco_io *io = arg;
    for (;;) {
        pthread_mutex_lock(&io->lock);
        while (!io->head && !io->stopping)
            pthread_cond_wait(&io->ready, &io->lock);
        if (io->stopping) {
            pthread_mutex_unlock(&io->lock);
            return NULL;
        }
        struct pco_io_request *r = io->head;
        io->head = r->next;
        if (!io->head) io->tail = NULL;
        --io->queued;
        r->running = 1;
        pthread_mutex_unlock(&io->lock);
        execute(r);
    }
}

static int deadline_before(const struct timespec *a, const struct timespec *b)
{
    return a->tv_sec < b->tv_sec ||
        (a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec);
}

/* A single timer manager sleeps until the earliest CLOCK_MONOTONIC deadline;
 * waiting timers never occupy a blocking I/O executor. */
static void *timer_main(void *arg)
{
    struct pco_io *io = arg;
    pthread_mutex_lock(&io->lock);
    while (!io->stopping) {
        if (!io->timers) {
            pthread_cond_wait(&io->timer_ready, &io->lock);
            continue;
        }
        struct timespec deadline = io->timers->operation.deadline, now;
        (void)pthread_cond_timedwait(&io->timer_ready, &io->lock, &deadline);
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (io->timers && !deadline_before(&now, &io->timers->operation.deadline)) {
            struct pco_io_request *r = io->timers;
            io->timers = r->next;
            --io->queued;
            complete_locked(r, 0, 0);
        }
    }
    pthread_mutex_unlock(&io->lock);
    return NULL;
}

int pco_io_create(size_t worker_count, size_t queue_capacity, pco_io_t **out)
{
    int err;
    if (!out || !worker_count || !queue_capacity) return EINVAL;
    *out = NULL;
    struct pco_io *io = calloc(1, sizeof(*io));
    if (!io) return ENOMEM;
    io->completion_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (io->completion_fd < 0) { err = errno; free(io); return err; }
    io->workers = calloc(worker_count, sizeof(*io->workers));
    if (!io->workers) { close(io->completion_fd); free(io); return ENOMEM; }
    err = pthread_mutex_init(&io->lock, NULL);
    if (err) { close(io->completion_fd); free(io->workers); free(io); return err; }
    err = pthread_cond_init(&io->ready, NULL);
    if (err) { pthread_mutex_destroy(&io->lock); close(io->completion_fd); free(io->workers); free(io); return err; }
    pthread_condattr_t attr;
    err = pthread_condattr_init(&attr);
    if (!err) {
        err = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
        if (!err) err = pthread_cond_init(&io->timer_ready, &attr);
        pthread_condattr_destroy(&attr);
    }
    if (err) { pthread_cond_destroy(&io->ready); pthread_mutex_destroy(&io->lock); close(io->completion_fd); free(io->workers); free(io); return err; }
    io->queue_capacity = queue_capacity;
    for (; io->worker_count < worker_count; ++io->worker_count) {
        err = pthread_create(&io->workers[io->worker_count], NULL, worker_main, io);
        if (err) {
            pthread_mutex_lock(&io->lock);
            io->stopping = 1;
            pthread_cond_broadcast(&io->ready);
            pthread_mutex_unlock(&io->lock);
            for (size_t i = 0; i < io->worker_count; ++i) pthread_join(io->workers[i], NULL);
            pthread_cond_destroy(&io->timer_ready);
            pthread_cond_destroy(&io->ready);
            pthread_mutex_destroy(&io->lock);
            close(io->completion_fd);
            free(io->workers); free(io);
            return err;
        }
    }
    err = pthread_create(&io->timer_thread, NULL, timer_main, io);
    if (err) {
        pthread_mutex_lock(&io->lock);
        io->stopping = 1;
        pthread_cond_broadcast(&io->ready);
        pthread_mutex_unlock(&io->lock);
        for (size_t i = 0; i < io->worker_count; ++i) pthread_join(io->workers[i], NULL);
        pthread_cond_destroy(&io->timer_ready);
        pthread_cond_destroy(&io->ready);
        pthread_mutex_destroy(&io->lock);
        close(io->completion_fd);
        free(io->workers); free(io);
        return err;
    }
    *out = io;
    return 0;
}

int pco_io_destroy(pco_io_t *io)
{
    if (!io) return EINVAL;
    pthread_mutex_lock(&io->lock);
    if (io->outstanding) { pthread_mutex_unlock(&io->lock); return EBUSY; }
    io->stopping = 1;
    pthread_cond_broadcast(&io->ready);
    pthread_cond_broadcast(&io->timer_ready);
    pthread_mutex_unlock(&io->lock);
    pthread_join(io->timer_thread, NULL);
    for (size_t i = 0; i < io->worker_count; ++i) pthread_join(io->workers[i], NULL);
    pthread_cond_destroy(&io->timer_ready);
    pthread_cond_destroy(&io->ready);
    pthread_mutex_destroy(&io->lock);
    close(io->completion_fd);
    free(io->workers);
    free(io);
    return 0;
}

int pco_io_completion_fd(const pco_io_t *io)
{
    return io ? io->completion_fd : -1;
}

int pco_io_submit(pco_io_t *io, const pco_io_operation_t *operation,
                  pco_io_request_t **out)
{
    if (!io || !operation || !out) return EINVAL;
    *out = NULL;
    if (operation->kind < PCO_IO_READ || operation->kind > PCO_IO_TIMER ||
        (operation->kind != PCO_IO_TIMER && operation->fd < 0)) return EINVAL;
    if (operation->kind <= PCO_IO_SEND && operation->length && !operation->buffer)
        return EINVAL;
    if (operation->kind == PCO_IO_CONNECT && !operation->address) return EINVAL;
    if (operation->kind == PCO_IO_ACCEPT &&
        ((operation->address == NULL) != (operation->address_length == NULL))) return EINVAL;
    if (operation->kind == PCO_IO_TIMER && (operation->deadline.tv_sec < 0 ||
        operation->deadline.tv_nsec < 0 || operation->deadline.tv_nsec >= 1000000000L)) return EINVAL;
    struct pco_io_request *r = calloc(1, sizeof(*r));
    if (!r) return ENOMEM;
    r->io = io;
    r->operation = *operation;
    atomic_init(&r->done, 0);
    pthread_mutex_lock(&io->lock);
    if (io->stopping || io->queued == io->queue_capacity) {
        int err = io->stopping ? EINVAL : EAGAIN;
        pthread_mutex_unlock(&io->lock);
        free(r);
        return err;
    }
    if (operation->kind == PCO_IO_TIMER) {
        struct pco_io_request **link = &io->timers;
        while (*link && !deadline_before(&operation->deadline, &(*link)->operation.deadline))
            link = &(*link)->next;
        r->next = *link;
        *link = r;
        pthread_cond_signal(&io->timer_ready);
    } else {
        if (io->tail) io->tail->next = r;
        else io->head = r;
        io->tail = r;
        pthread_cond_signal(&io->ready);
    }
    ++io->queued;
    ++io->outstanding;
    pthread_mutex_unlock(&io->lock);
    *out = r;
    return 0;
}

int pco_io_poll(pco_io_request_t *r, pco_io_result_t *result)
{
    if (!r || !result) return -1;
    if (!atomic_load_explicit(&r->done, memory_order_acquire)) return 0;
    *result = r->result;
    return 1;
}

int pco_io_await(pco_io_request_t *r, pco_io_result_t *result)
{
    if (!r || !result) return EINVAL;
    ult_t *current = ult_current();
    if (!current) return EINVAL;
    struct pco_io *io = r->io;
    pthread_mutex_lock(&io->lock);
    if (r->waiter) { pthread_mutex_unlock(&io->lock); return EBUSY; }
    if (!atomic_load_explicit(&r->done, memory_order_acquire)) {
        r->waiter = current;
        pthread_mutex_unlock(&io->lock);
        while (!atomic_load_explicit(&r->done, memory_order_acquire))
            pco_park_if_pending(&r->done);
        pthread_mutex_lock(&io->lock);
        r->waiter = NULL;
    }
    *result = r->result;
    pthread_mutex_unlock(&io->lock);
    return 0;
}

int pco_io_cancel(pco_io_request_t *r)
{
    if (!r) return EINVAL;
    struct pco_io *io = r->io;
    pthread_mutex_lock(&io->lock);
    if (atomic_load_explicit(&r->done, memory_order_acquire)) {
        pthread_mutex_unlock(&io->lock); return EALREADY;
    }
    if (r->running) { pthread_mutex_unlock(&io->lock); return EBUSY; }
    struct pco_io_request *prev = NULL, *item =
        r->operation.kind == PCO_IO_TIMER ? io->timers : io->head;
    while (item && item != r) { prev = item; item = item->next; }
    if (!item) { pthread_mutex_unlock(&io->lock); return EINVAL; }
    if (prev) prev->next = r->next;
    else if (r->operation.kind == PCO_IO_TIMER) io->timers = r->next;
    else io->head = r->next;
    if (r->operation.kind == PCO_IO_TIMER) pthread_cond_signal(&io->timer_ready);
    else if (io->tail == r) io->tail = prev;
    --io->queued;
    complete_locked(r, -1, ECANCELED);
    pthread_mutex_unlock(&io->lock);
    return 0;
}

int pco_io_release(pco_io_request_t *r)
{
    if (!r) return EINVAL;
    if (!atomic_load_explicit(&r->done, memory_order_acquire)) return EBUSY;
    struct pco_io *io = r->io;
    pthread_mutex_lock(&io->lock);
    --io->outstanding;
    pthread_mutex_unlock(&io->lock);
    free(r);
    return 0;
}

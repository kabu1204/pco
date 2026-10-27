#ifndef PCO_IO_H
#define PCO_IO_H

#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A bounded pool for blocking system calls, independent of PCO workers.
 * Operations are submitted without waiting. See the lifetime rules below. */
typedef struct pco_io pco_io_t;
typedef struct pco_io_request pco_io_request_t;

typedef enum {
    PCO_IO_READ, PCO_IO_WRITE, PCO_IO_PREAD, PCO_IO_PWRITE,
    PCO_IO_RECV, PCO_IO_SEND, PCO_IO_ACCEPT, PCO_IO_CONNECT, PCO_IO_TIMER
} pco_io_kind_t;

typedef struct {
    pco_io_kind_t kind;
    int fd;
    void *buffer;
    size_t length;
    off_t offset;                 /* PREAD/PWRITE */
    int flags;                    /* RECV/SEND */
    struct sockaddr *address;     /* ACCEPT/CONNECT */
    socklen_t *address_length;    /* ACCEPT, optional with address */
    socklen_t connect_length;     /* CONNECT */
    struct timespec deadline;     /* TIMER: absolute CLOCK_MONOTONIC */
} pco_io_operation_t;

typedef struct {
    ssize_t value; /* syscall result, including accepted fd */
    int error;     /* 0 on success; errno or ECANCELED otherwise */
} pco_io_result_t;

/* API functions return 0 or an errno value unless otherwise documented. */
int pco_io_create(size_t worker_count, size_t queue_capacity, pco_io_t **out);
/* Refuses destruction with EBUSY until every request has been released.
 * The caller must serialize destruction with all other uses of the pool. */
int pco_io_destroy(pco_io_t *io);

/* Borrowed nonblocking eventfd, readable when one or more completions have
 * occurred. Integrate it with poll/epoll, read its uint64_t counter to clear
 * notifications, then poll all outstanding requests. Notifications may be
 * coalesced or stale. Do not close the fd; it lives until pco_io_destroy. */
int pco_io_completion_fd(const pco_io_t *io);

/* Copies the descriptor, not its pointed-to data. Buffers, socket addresses,
 * address_length and fd must remain valid until completion, including after
 * cancel races. The caller is responsible for synchronizing their access and
 * must not close/reuse the fd before completion. The queue returns EAGAIN
 * when full; submit never waits for a system call to finish. */
int pco_io_submit(pco_io_t *io, const pco_io_operation_t *operation,
                  pco_io_request_t **out);

/* 1 means finished and copies result; 0 means still pending; -1 means an
 * invalid argument. Completion is final even if the syscall failed. */
int pco_io_poll(pco_io_request_t *request, pco_io_result_t *result);
/* Wait for completion without blocking the logical worker or spinning.
 * Only valid inside a ULT. Each request supports at most one awaiting ULT;
 * returns EBUSY for a second waiter, or EINVAL outside a ULT. */
int pco_io_await(pco_io_request_t *request, pco_io_result_t *result);
/* Cancel a queued request or a pending timer: returns 0 and completes with ECANCELED.
 * EBUSY means execution has started; EALREADY means it already completed.
 * Cancellation does not undo side effects. */
int pco_io_cancel(pco_io_request_t *request);
/* Only completed requests can be released. Serialize release with all other
 * calls on the same request. All requests must be released before io_destroy. */
int pco_io_release(pco_io_request_t *request);

#ifdef __cplusplus
}
#endif
#endif

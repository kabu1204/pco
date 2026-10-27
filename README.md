# PCO

PCO is a Linux x86-64 research prototype for user-level scheduling. It supports
cooperative scheduling and experimental signal-driven preemption. It is not a
production runtime: libc, pthread locks, signal interactions and resource
exhaustion are still being investigated.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

GoogleTest is needed only for tests; configure with `-DBUILD_TESTING=OFF` to
build without it. The latency benchmark uses libzstd when it is installed; its
CPU workload does not require it. See [benchmark/README.md](benchmark/README.md)
for experiment commands and measurement limitations.

## Scheduling

Include `ult.h`, `klt.h`, and `scheduler.h`, and link `pco` and pthreads. Create
ULTs with `ult_create`, submit them with `ult_put`, and start logical workers
with `klt_create_config`. A zero `quantum_ns` uses cooperative scheduling;
a nonzero value enables timer-driven signal preemption. `max_klts` limits the
execution KLT pool per logical worker; zero means no configured upper bound.
`klt_create()` uses cooperative scheduling. `klt_stats()` reports KLT creation,
peak pool size, interruptions, switches following interruption and pool-limit
encounters.

The prototype reserves process-wide `SIGRTMIN+6`: application code must not
replace the handler or block that signal in a running ULT. A worker has a
coordinator and a manager thread in addition to its execution KLTs. On
preemption the interrupted KLT remains suspended with its signal frame; a
replacement KLT runs another ULT. A started ULT stays attached to its KLT until
it finishes, including after cooperative yields and I/O waits. Idle KLTs are
reused. The pool may approach one KLT per unfinished ULT; its limit can prevent
a queued task from running until another ULT finishes. Lock contention and
blocking calls can cause loss of progress, especially with a bounded pool. The
implementation does not provide cross-KLT ULT migration or guarantee arbitrary
C and pthread compatibility.

A successful `ult_put` or `ult_put_localq` transfers ownership of the ULT to the
runtime; it frees the object and stack when the entry function returns. Do not
access a submitted pointer. On failure, or when a ULT is never submitted, release
it with `ult_destroy`. The entry argument remains the caller's responsibility.
`klt_destroy` stops and joins the worker after its started and locally queued
ULTs finish; globally queued tasks remain for other or later workers. It can
block if a task never finishes. Do not race destruction with other uses of the
worker or concurrent submission/destruction of a ULT.

`ult_local.h` provides `pco_ult_self()` and ULT-local keys. IDs are stable for
the lifetime of a ULT; outside a ULT the ID is zero. Applications should use
ULT-local storage for state belonging to a ULT rather than pthread-local
storage. The current implementation allows 256 active keys, calls destructors
on the ULT before exit for up to four passes, and requires callers to synchronize
key deletion with access and ULT exit. Key deletion does not invoke destructors.

## Asynchronous I/O prototype

`pco_io.h` provides a bounded pool for read, write, pread, pwrite, recv, send,
accept, connect and monotonic timers. Submissions return requests; callers can
poll or await a request from a ULT. The pool also exposes a borrowed completion eventfd for
integration with poll/epoll. Underlying blocking calls run on separate I/O
threads. Buffers, addresses and file descriptors must remain valid until the
request completes, and all requests must be released before pool destruction.

Cancellation currently works only for queued requests and pending timers.
Running-operation cancellation, I/O deadlines, close coordination and a complete
io_uring interface are not yet implemented. See `pco_io.h` for completion,
ownership and cancellation rules.
Ordinary blocking syscalls made outside this interface can block an execution
KLT. The lock and progress behavior of ULTs that await I/O while holding a
pthread lock remains a research target.

# PCO development guide

PCO is a fast-evolving research prototype for user-level scheduling on Linux
x86-64. Favor clear, focused changes and rapid iteration over production-style
process or compatibility scaffolding. Do not add gates, smoke tests, or
unnecessary fallback tests. Use targeted verification when it provides useful
correctness or measurement evidence; report what was actually verified.

## Research direction

- Study signal-driven preemption and short-task latency in the presence of
  long CPU-bound tasks. Compare cooperative PCO, preemptive PCO, a fixed pthread
  pool, and one pthread per task.
- Aim to support ordinary libc and pthread locks; critical blocking syscalls and
  asynchronous I/O may use PCO interfaces. Treat compatibility and progress as
  research questions, not established guarantees.
- Provide stable ULT identifiers and ULT-local storage. Application pthread
  identity and pthread-local storage are not ULT identity or storage.
- KLT pool growth and limits are configurable and belong in experiments. The
  current implementation resumes started ULTs on their original execution KLT;
  future cross-KLT migration must address runtime and libc TLS, pthread lock
  ownership, signal state, and context safety.
- The agreed research plan is at https://chatgpt.com/space/page_ad077faa01108191b8378647d50d7ebb.

## Code map

- `include/`: public runtime, ULT-local storage, and asynchronous I/O interfaces.
- `src/sched.c` and `src/klt.c`: logical workers, executor KLTs, preemption,
  pool management, and park/unpark.
- `src/ult.c`, `src/ult_local.c`, `src/stack.c`, and `src/context/`: ULT
  lifecycle, storage, stacks, and context switching.
- `src/pco_io.c`: asynchronous I/O prototype and timer handling.
- `benchmark/`: open-loop latency benchmark, optional Zstandard workload, and
  repeated comparison driver. See its README for options and measurement limits.
- `test/`: existing correctness tests.

## Working on the runtime

- The current preemption implementation reserves process-wide `SIGRTMIN+6` and
  preserves the interrupted KLT and signal frame. Signal handlers must use
  operations appropriate to signal context; avoid allocation and pthread locks
  there. Reason explicitly about signal masks, scheduler locks, wakeup races,
  and object lifetimes when changing scheduling or I/O.
- A finite KLT pool, blocking calls, and ordinary locks can prevent progress;
  do not present pool growth or KLT-switching as proof of universal safety.
  Keep limitations and any behavior changes visible in documentation.
- Preserve ULT ownership and I/O request, buffer, and file-descriptor lifetime
  contracts, or update the public headers and documentation when changing them.
- Benchmark with comparable CPU allocations, retain raw results and experiment
  configuration, and distinguish exploratory runs from established conclusions.
  Avoid committing generated benchmark data or large corpora.

## Build and verification

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

GoogleTest is used by the existing tests; `-DBUILD_TESTING=OFF` builds without
it. Zstandard is optional for the benchmark. Select checks appropriate to the
change; see `README.md` and `benchmark/README.md` for runtime and experiment
details.

# PCO latency experiment

`pco_latency` submits long tasks immediately and short tasks on an open-loop,
absolute monotonic schedule. It records planned arrival, actual submission,
first execution, completion and thread CPU time for each task. A late generator
is visible in submission lateness and planned-to-finish latency. Offered and
actual submission rates are reported separately.

Build with CMake; the benchmark is in `build/benchmark/pco_latency`. Zstandard
support is enabled if the development headers and library are found at configure
time. Use `--help` for options.

```sh
# Use the SAME available CPU set for every mode. Adjust 2 to a CPU allowed
# by the current container or cgroup, e.g. see `taskset -pc $$`.
taskset -c 2 build/benchmark/pco_latency --mode pco-coop --csv coop.csv
taskset -c 2 build/benchmark/pco_latency --mode pco-preempt --quantum-us 1000 --max-klts 8 --csv preempt.csv
taskset -c 2 build/benchmark/pco_latency --mode pthread-pool --workers 1 --csv pool.csv
taskset -c 2 build/benchmark/pco_latency --mode pthread-task --csv pthread.csv
```

Pin all modes to the same CPU set or cgroup quota. `--workers` sets PCO logical
workers or pool threads; `pthread-task` creates one pthread per task. CPU quota
and KLT count are different quantities. The load generator shares the CPU set in
these commands, so its contention and late submissions are measured, not removed;
use a dedicated generator CPU or separate harness if that interference matters.
The CSV contains absolute monotonic nanosecond timestamps and CPU time. Preserve
the command, compiler, machine, kernel, CPU set, corpus and raw CSV with each run.

CPU tasks consume a configured amount of `CLOCK_THREAD_CPUTIME_ID` and never
voluntarily yield. The counter is sampled periodically, so work is approximate.
Initialization is measured separately. Allocation and task or pthread creation
at submission remain in the measured path and differ by mode; submission-to-start
and planned-to-finish are reported separately for that reason. Max RSS is the
whole process high-water mark. KLT metrics count execution KLTs only: coordinator, manager and load generator
threads are excluded. The sum of per-worker KLT peaks is not a global
simultaneous peak. `preemptions` counts recorded interruptions, including cases
where the same ULT resumes; `preempt_switches` counts switches to a different
ULT after preemption. A fixed KLT cap can exhaust the pool; progress and latency
under exhaustion are research questions, not guarantees.

When libzstd is available, use a single local corpus file as `--input` (for example,
a file from Silesia). Each task compresses a slice; sizes larger than the file are
rejected. The corpus is preloaded,
each task owns a prewarmed compression context and output buffer, internal zstd
parallelism is disabled, and the result is verified by decompression after the
measurement. The `--long-bytes` and `--short-bytes` options select input sizes;
actual long and short task CPU time distributions are reported so they can be
calibrated on the machine. Input content and compression level also affect time.

```sh
taskset -c 2 build/benchmark/pco_latency --mode pco-preempt --workload zstd \
  --input /path/to/silesia-file --long-bytes 1048576 --short-bytes 65536 \
  --quantum-us 1000 --max-klts 8 --csv zstd.csv
```

The in-memory compression workload is not an end-to-end I/O application. Repeat
experiments with multiple inputs and runs, and report variability. PCO's signal
preemption and ordinary libc/pthread compatibility remain experimental.

Run repeated comparisons with a fixed CPU set and retain each run's CSV, stdout,
stderr, exact command, binary and input SHA256 and machine details in a new directory (existing results
are never overwritten):

```sh
python3 benchmark/compare.py --binary build/benchmark/pco_latency \
  --cpus 2 --output /workspace/scratch/pco-experiment-001 --repeats 3 -- \
  --long-us 200000 --short-count 100 --interval-us 1000
```

The driver runs cooperative PCO, preemptive PCO with caps 1 and 8 and without a
configured cap, a fixed pthread pool, and one pthread per task. Pass `--workload
zstd --input /path/to/file` and size options after `--` for compression. The
CPU set, worker count, quantum, mode, pool caps and output CSV are controlled by
the driver; choose an accessible CPU set and hold it constant. Failed and timed
out runs are recorded in `metadata.json`. This script collects raw repeated
results; analyze variability and test workloads before drawing conclusions.
The generator shares the selected CPU set and may affect the measurements.

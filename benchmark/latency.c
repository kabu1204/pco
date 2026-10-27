#define _GNU_SOURCE
#include "klt.h"
#include "scheduler.h"
#include "ult.h"
#ifdef PCO_HAVE_ZSTD
#include "zstd_workload.h"
#endif

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

typedef enum { PCO_COOP, PCO_PREEMPT, PTHREAD_POOL, PTHREAD_TASK } mode_t_bench;

typedef struct run run_t;
typedef struct task {
    run_t *run;
    uint64_t planned_ns, submitted_ns, started_ns, finished_ns;
    uint64_t cpu_ns, actual_cpu_ns, digest;
    int is_short, status;
#ifdef PCO_HAVE_ZSTD
    zstd_bench_job_t *zstd_job;
#endif
    struct task *next;
} task_t;

struct run {
    mode_t_bench mode;
    int workers, long_count, short_count;
    uint64_t long_ns, short_ns, first_ns, interval_ns, quantum_ns;
    size_t max_klts;
    const char *workload, *input, *csv_path;
    size_t long_bytes, short_bytes;
    int level;
    pthread_mutex_t lock;
    pthread_cond_t ready, done;
    task_t *head, *tail;
    int completed, stopped;
    pthread_t *threads;
    klt_t **klts;
};

static uint64_t ns(clockid_t clock)
{
    struct timespec ts;
    if (clock_gettime(clock, &ts)) { perror("clock_gettime"); exit(1); }
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static void wait_until(uint64_t deadline)
{
    struct timespec ts = { .tv_sec = (time_t)(deadline / UINT64_C(1000000000)),
                           .tv_nsec = (long)(deadline % UINT64_C(1000000000)) };
    int err;
    do { err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL); } while (err == EINTR);
    if (err) { fprintf(stderr, "clock_nanosleep: %s\n", strerror(err)); exit(1); }
}

/* CPU time, rather than elapsed wall time, keeps each task's work comparable
 * when the OS or PCO preempts it. The digest prevents eliminating the loop. */
static void execute(task_t *task)
{
    uint64_t cpu_start, x = UINT64_C(0x123456789abcdef);
    task->started_ns = ns(CLOCK_MONOTONIC);
    cpu_start = ns(CLOCK_THREAD_CPUTIME_ID);
#ifdef PCO_HAVE_ZSTD
    if (task->zstd_job) {
        task->status = zstd_bench_job_run(task->zstd_job);
        task->digest = (uint64_t)task->status;
    } else
#endif
    do {
        for (int i = 0; i < 4096; ++i) x = x * UINT64_C(6364136223846793005) + 1;
    } while (ns(CLOCK_THREAD_CPUTIME_ID) - cpu_start < task->cpu_ns);
#ifdef PCO_HAVE_ZSTD
    if (!task->zstd_job)
#endif
        task->digest = x;
    task->actual_cpu_ns = ns(CLOCK_THREAD_CPUTIME_ID) - cpu_start;
    task->finished_ns = ns(CLOCK_MONOTONIC);
    pthread_mutex_lock(&task->run->lock);
    ++task->run->completed;
    pthread_cond_signal(&task->run->done);
    pthread_mutex_unlock(&task->run->lock);
}

static void *task_entry(void *arg) { execute(arg); return NULL; }

static void *pool_entry(void *arg)
{
    run_t *run = arg;
    for (;;) {
        pthread_mutex_lock(&run->lock);
        while (!run->head && !run->stopped) pthread_cond_wait(&run->ready, &run->lock);
        if (!run->head && run->stopped) { pthread_mutex_unlock(&run->lock); return NULL; }
        task_t *task = run->head;
        run->head = task->next;
        if (!run->head) run->tail = NULL;
        pthread_mutex_unlock(&run->lock);
        execute(task);
    }
}

static void fail_pthread(const char *operation, int error)
{
    fprintf(stderr, "%s: %s\n", operation, strerror(error));
    exit(1);
}

static void submit(run_t *run, task_t *task, int index)
{
    int err;
    task->submitted_ns = ns(CLOCK_MONOTONIC);
    if (run->mode == PCO_COOP || run->mode == PCO_PREEMPT) {
        ult_t *ult = ult_create(65536, task_entry, task);
        if (!ult) { fprintf(stderr, "ult_create failed\n"); exit(1); }
        err = ult_put(ult);
        if (err) { ult_destroy(ult); fail_pthread("ult_put", err); }
    } else if (run->mode == PTHREAD_TASK) {
        err = pthread_create(&run->threads[index], NULL, task_entry, task);
        if (err) fail_pthread("pthread_create", err);
    } else {
        pthread_mutex_lock(&run->lock);
        task->next = NULL;
        if (run->tail) run->tail->next = task; else run->head = task;
        run->tail = task;
        pthread_cond_signal(&run->ready);
        pthread_mutex_unlock(&run->lock);
    }
}

static int compare_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static uint64_t percentile(const uint64_t *values, size_t count, size_t pct)
{
    size_t rank = (pct * count + 99) / 100;
    return values[rank ? rank - 1 : 0];
}

static void stats(const char *name, uint64_t *samples, size_t count)
{
    qsort(samples, count, sizeof(*samples), compare_u64);
    printf("%-28s p50=%9.3f p95=%9.3f p99=%9.3f max=%9.3f ms\n", name,
           percentile(samples, count, 50) / 1e6, percentile(samples, count, 95) / 1e6,
           percentile(samples, count, 99) / 1e6, samples[count - 1] / 1e6);
}

static void report(task_t *tasks, size_t count, uint64_t start)
{
    uint64_t *samples = malloc(count * sizeof(*samples));
    if (!samples) { perror("malloc"); exit(1); }
    uint64_t last = start, checksum = 0;
    for (size_t i = 0; i < count; ++i) {
        if (tasks[i].finished_ns > last) last = tasks[i].finished_ns;
        checksum ^= tasks[i].digest;
    }
    printf("completed=%zu makespan_ms=%.3f throughput_tasks_s=%.2f checksum=%" PRIu64 "\n",
           count, (last - start) / 1e6, count * 1e9 / (double)(last - start), checksum);
    size_t j = 0;
    for (size_t i = 0; i < count; ++i) if (!tasks[i].is_short) samples[j++] = tasks[i].actual_cpu_ns;
    if (j) stats("long actual CPU time", samples, j);
    j = 0;
    for (size_t i = 0; i < count; ++i) if (tasks[i].is_short) samples[j++] = tasks[i].actual_cpu_ns;
    stats("short actual CPU time", samples, j);
    j = 0;
    for (size_t i = 0; i < count; ++i) if (tasks[i].is_short) samples[j++] = tasks[i].submitted_ns - tasks[i].planned_ns;
    stats("generator submission lateness", samples, j);
    j = 0;
    for (size_t i = 0; i < count; ++i) if (tasks[i].is_short) samples[j++] = tasks[i].started_ns - tasks[i].submitted_ns;
    stats("short submit-to-start", samples, j);
    j = 0;
    for (size_t i = 0; i < count; ++i) if (tasks[i].is_short) samples[j++] = tasks[i].finished_ns - tasks[i].submitted_ns;
    stats("short submit-to-finish", samples, j);
    j = 0;
    for (size_t i = 0; i < count; ++i) if (tasks[i].is_short) samples[j++] = tasks[i].finished_ns - tasks[i].planned_ns;
    stats("short planned-to-finish", samples, j);
    free(samples);
}

static uint64_t number(const char *option, const char *value, uint64_t max)
{
    char *end;
    errno = 0;
    if (*value == '-') { fprintf(stderr, "invalid %s: %s\n", option, value); exit(2); }
    unsigned long long n = strtoull(value, &end, 10);
    if (errno || !*value || *end || n > max) { fprintf(stderr, "invalid %s: %s\n", option, value); exit(2); }
    return n;
}

static void usage(const char *program)
{
    printf("Usage: %s [--mode pco-coop|pco-preempt|pthread-pool|pthread-task] [options]\n"
           "  --workers N        PCO workers or pthread pool size (default 1)\n"
           "  --long-count N     long tasks submitted at start (default 1)\n"
           "  --short-count N    short tasks at open-loop arrival times (default 20)\n"
           "  --long-us N        CPU time per long task (default 100000)\n"
           "  --short-us N       CPU time per short task (default 100)\n"
           "  --first-us N       first short arrival after start (default 2000)\n"
           "  --interval-us N    short-task arrival interval (default 2000)\n"
           "  --quantum-us N     preemption time slice (default 1000)\n"
           "  --max-klts N       KLT cap per worker; 0 means no configured cap\n"
           "  --workload NAME    cpu or zstd (default cpu)\n"
           "  --input PATH       input corpus file for zstd\n"
           "  --long-bytes N     long zstd task input size (default 1048576)\n"
           "  --short-bytes N    short zstd task input size (default 65536)\n"
           "  --level N          zstd compression level (default 1)\n"
           "  --csv PATH         save per-task timestamps and CPU time\n", program);
}

int main(int argc, char **argv)
{
    uint64_t init_started = ns(CLOCK_MONOTONIC);
    run_t run = { .mode=PCO_COOP, .workers=1, .long_count=1, .short_count=20,
                  .long_ns=100000000, .short_ns=100000, .first_ns=2000000, .interval_ns=2000000,
                  .quantum_ns=1000000, .workload="cpu", .long_bytes=1048576, .short_bytes=65536, .level=1,
                  .lock=PTHREAD_MUTEX_INITIALIZER, .ready=PTHREAD_COND_INITIALIZER, .done=PTHREAD_COND_INITIALIZER };
    for (int i = 1; i < argc; ++i) {
        const char *key = argv[i];
        if (!strcmp(key, "--help")) { usage(argv[0]); return 0; }
        if (++i >= argc) { usage(argv[0]); return 2; }
        const char *value = argv[i];
        if (!strcmp(key, "--mode")) {
            if (!strcmp(value, "pco-coop")) run.mode = PCO_COOP;
            else if (!strcmp(value, "pco-preempt")) run.mode = PCO_PREEMPT;
            else if (!strcmp(value, "pthread-pool")) run.mode = PTHREAD_POOL;
            else if (!strcmp(value, "pthread-task")) run.mode = PTHREAD_TASK;
            else { fprintf(stderr, "unknown mode: %s\n", value); return 2; }
        } else if (!strcmp(key, "--workers")) run.workers = (int)number(key,value,1024);
        else if (!strcmp(key, "--long-count")) run.long_count = (int)number(key,value,100000);
        else if (!strcmp(key, "--short-count")) run.short_count = (int)number(key,value,100000);
        else if (!strcmp(key, "--long-us")) run.long_ns = number(key,value,UINT64_MAX/1000)*1000;
        else if (!strcmp(key, "--short-us")) run.short_ns = number(key,value,UINT64_MAX/1000)*1000;
        else if (!strcmp(key, "--first-us")) run.first_ns = number(key,value,UINT64_MAX/1000)*1000;
        else if (!strcmp(key, "--interval-us")) run.interval_ns = number(key,value,UINT64_MAX/1000)*1000;
        else if (!strcmp(key, "--quantum-us")) run.quantum_ns = number(key,value,UINT64_MAX/1000)*1000;
        else if (!strcmp(key, "--max-klts")) run.max_klts = (size_t)number(key,value,1000000);
        else if (!strcmp(key, "--workload")) run.workload = value;
        else if (!strcmp(key, "--input")) run.input = value;
        else if (!strcmp(key, "--csv")) run.csv_path = value;
        else if (!strcmp(key, "--long-bytes")) run.long_bytes = (size_t)number(key,value,SIZE_MAX);
        else if (!strcmp(key, "--short-bytes")) run.short_bytes = (size_t)number(key,value,SIZE_MAX);
        else if (!strcmp(key, "--level")) run.level = (int)number(key,value,22);
        else { fprintf(stderr, "unknown option: %s\n", key); return 2; }
    }
    if (!run.workers || !run.short_count || !run.long_ns || !run.short_ns) {
        fprintf(stderr, "workers, short-count, and task durations must be nonzero\n"); return 2;
    }
    if (run.short_count > 1 && run.interval_ns > (UINT64_MAX - run.first_ns) / (uint64_t)(run.short_count - 1)) {
        fprintf(stderr, "arrival schedule overflows\n"); return 2;
    }
    if (strcmp(run.workload, "cpu") && strcmp(run.workload, "zstd")) {
        fprintf(stderr, "unknown workload: %s\n", run.workload); return 2;
    }
    if (run.mode == PCO_PREEMPT && !run.quantum_ns) {
        fprintf(stderr, "pco-preempt requires a nonzero quantum\n"); return 2;
    }
    if (!strcmp(run.workload, "zstd") && (!run.input || !run.long_bytes || !run.short_bytes)) {
        fprintf(stderr, "zstd requires --input and nonzero task sizes\n"); return 2;
    }
#ifndef PCO_HAVE_ZSTD
    if (!strcmp(run.workload, "zstd")) { fprintf(stderr, "zstd support not available in this build\n"); return 2; }
#endif
    size_t count = (size_t)run.long_count + (size_t)run.short_count;
    task_t *tasks = calloc(count, sizeof(*tasks));
    run.threads = calloc(run.mode == PTHREAD_TASK ? count : (size_t)run.workers, sizeof(*run.threads));
    run.klts = calloc((size_t)run.workers, sizeof(*run.klts));
    if (!tasks || !run.threads || !run.klts) { perror("calloc"); return 1; }
#ifdef PCO_HAVE_ZSTD
    zstd_bench_t *zstd = NULL;
    if (!strcmp(run.workload, "zstd")) {
        zstd = zstd_bench_init(run.input);
        if (!zstd) { perror("zstd init"); return 1; }
        size_t size = zstd_bench_size(zstd);
        for (size_t i=0; i<count; ++i) {
            size_t length = i < (size_t)run.long_count ? run.long_bytes : run.short_bytes;
            tasks[i].zstd_job = zstd_bench_job_create(zstd, length, run.level, (i * (size_t)4096) % size);
            if (!tasks[i].zstd_job) { perror("zstd job"); return 1; }
        }
    }
#endif
    if (run.mode == PCO_COOP || run.mode == PCO_PREEMPT) {
        klt_config_t config = { .quantum_ns = run.mode == PCO_PREEMPT ? run.quantum_ns : 0, .max_klts = run.max_klts };
        for (int i=0; i<run.workers; ++i) if (!(run.klts[i] = klt_create_config(&config))) { perror("klt_create_config"); return 1; }
    } else if (run.mode == PTHREAD_POOL) {
        for (int i=0; i<run.workers; ++i) { int err = pthread_create(&run.threads[i], NULL, pool_entry, &run); if (err) fail_pthread("pthread_create", err); }
    }
    printf("workload=%s mode=%s workers=%d long_count=%d short_count=%d quantum_us=%" PRIu64 " max_klts=%zu\n",
           run.workload, run.mode == PCO_COOP ? "pco-coop" : run.mode == PCO_PREEMPT ? "pco-preempt" :
           run.mode == PTHREAD_POOL ? "pthread-pool" : "pthread-task", run.workers, run.long_count,
           run.short_count, run.quantum_ns / 1000, run.max_klts);
    fprintf(stderr, "For CPU-fair comparisons, use the same taskset/cgroup CPU quota across all modes; the generator shares that quota.\n");
    uint64_t start = ns(CLOCK_MONOTONIC);
    printf("initialization_ms=%.3f\n", (start - init_started) / 1e6);
    if (run.first_ns > UINT64_MAX - start || (run.short_count > 1 &&
        run.interval_ns > (UINT64_MAX - start - run.first_ns) / (uint64_t)(run.short_count - 1))) {
        fprintf(stderr, "arrival schedule overflows\n"); return 2;
    }
    for (int i=0; i<run.long_count; ++i) {
        tasks[i].run = &run; tasks[i].planned_ns = start; tasks[i].cpu_ns = run.long_ns;
        submit(&run, &tasks[i], i);
    }
    for (int i=0; i<run.short_count; ++i) {
        size_t index = (size_t)run.long_count + (size_t)i;
        tasks[index].run = &run; tasks[index].is_short = 1;
        tasks[index].cpu_ns = run.short_ns;
        tasks[index].planned_ns = start + run.first_ns + (uint64_t)i * run.interval_ns;
        wait_until(tasks[index].planned_ns);
        submit(&run, &tasks[index], (int)index);
    }
    pthread_mutex_lock(&run.lock);
    while (run.completed < (int)count) pthread_cond_wait(&run.done, &run.lock);
    run.stopped = 1;
    pthread_cond_broadcast(&run.ready);
    pthread_mutex_unlock(&run.lock);
    if (run.mode == PCO_COOP || run.mode == PCO_PREEMPT) {
        unsigned long total_created=0, peak_klts=0, preemptions=0, preempt_switches=0, pool_exhaustions=0, signal_errors=0;
        int last_signal_error=0, pool_growth_error=0;
        for (int i=0; i<run.workers; ++i) {
            klt_stats_t st;
            int err = klt_stats(run.klts[i], &st);
            if (err) fail_pthread("klt_stats", err);
            total_created += st.total_created; peak_klts += st.peak_klts; preemptions += st.preemptions;
            preempt_switches += st.preempt_switches; pool_exhaustions += st.pool_exhaustions;
            signal_errors += st.signal_errors; if (st.last_signal_error) last_signal_error = st.last_signal_error;
            if (st.pool_growth_error) pool_growth_error = st.pool_growth_error;
            err=klt_destroy(run.klts[i]); if (err) fail_pthread("klt_destroy", err);
        }
        printf("executor_klts_created=%lu sum_worker_peak_executor_klts=%lu preemptions=%lu preempt_switches=%lu pool_exhaustions=%lu signal_errors=%lu last_signal_error=%d pool_growth_error=%d\n",
               total_created, peak_klts, preemptions, preempt_switches, pool_exhaustions, signal_errors, last_signal_error, pool_growth_error);
    } else {
        size_t n = run.mode == PTHREAD_TASK ? count : (size_t)run.workers;
        for (size_t i=0; i<n; ++i) { int err=pthread_join(run.threads[i], NULL); if (err) fail_pthread("pthread_join", err); }
    }
    if (run.short_count > 1) {
        const task_t *first = &tasks[run.long_count], *last = &tasks[count - 1];
        double expected_s = (last->planned_ns - first->planned_ns) / 1e9;
        double actual_s = (last->submitted_ns - first->submitted_ns) / 1e9;
        printf("short_expected_rate_s=%.2f short_actual_submit_rate_s=%.2f\n",
               expected_s > 0 ? (run.short_count - 1) / expected_s : 0,
               actual_s > 0 ? (run.short_count - 1) / actual_s : 0);
    }
    report(tasks, count, start);
    struct rusage usage;
    if (!getrusage(RUSAGE_SELF, &usage)) printf("max_rss_kib=%ld\n", usage.ru_maxrss);
    if (run.csv_path) {
        FILE *out = fopen(run.csv_path, "w");
        if (!out) { perror("open CSV"); return 1; }
        fprintf(out, "task,type,planned_ns,submitted_ns,started_ns,finished_ns,cpu_ns,status\n");
        for (size_t i=0; i<count; ++i) fprintf(out, "%zu,%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%d\n",
            i, tasks[i].is_short ? "short" : "long", tasks[i].planned_ns, tasks[i].submitted_ns,
            tasks[i].started_ns, tasks[i].finished_ns, tasks[i].actual_cpu_ns, tasks[i].status);
        if (fclose(out)) { perror("close CSV"); return 1; }
    }
#ifdef PCO_HAVE_ZSTD
    if (zstd) {
        int failed = 0;
        for (size_t i=0; i<count; ++i) {
            if (tasks[i].status || zstd_bench_job_verify(tasks[i].zstd_job)) ++failed;
            zstd_bench_job_destroy(tasks[i].zstd_job);
        }
        zstd_bench_destroy(zstd);
        if (failed) { fprintf(stderr, "zstd verification failed for %d tasks\n", failed); return 1; }
        printf("zstd_verified=%zu\n", count);
    }
#endif
    free(run.klts); free(run.threads); free(tasks);
    pthread_cond_destroy(&run.ready); pthread_cond_destroy(&run.done); pthread_mutex_destroy(&run.lock);
    return 0;
}

#ifndef PCO_BENCH_ZSTD_WORKLOAD_H
#define PCO_BENCH_ZSTD_WORKLOAD_H

#include <stddef.h>

typedef struct zstd_bench zstd_bench_t;
typedef struct zstd_bench_job zstd_bench_job_t;

/* Load a corpus file once. The corpus must outlive its jobs. */
zstd_bench_t *zstd_bench_init(const char *path);
size_t zstd_bench_size(const zstd_bench_t *bench);
void zstd_bench_destroy(zstd_bench_t *bench);

/* Each job owns a context and output buffer; length must fit the corpus.
 * Offsets outside the range that fits the requested length wrap to zero. */
zstd_bench_job_t *zstd_bench_job_create(const zstd_bench_t *bench,
                                         size_t length, int level, size_t offset);
int zstd_bench_job_run(zstd_bench_job_t *job);
int zstd_bench_job_verify(zstd_bench_job_t *job);
const char *zstd_bench_job_error(const zstd_bench_job_t *job);
void zstd_bench_job_destroy(zstd_bench_job_t *job);

#endif

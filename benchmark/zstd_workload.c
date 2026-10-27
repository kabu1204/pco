#include "zstd_workload.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

struct zstd_bench {
    unsigned char *data;
    size_t size;
};

struct zstd_bench_job {
    const unsigned char *input;
    size_t input_size;
    void *compressed;
    size_t compressed_capacity;
    size_t compressed_size;
    ZSTD_CCtx *ctx;
    const char *error;
};

zstd_bench_t *zstd_bench_init(const char *path)
{
    FILE *file;
    long size = -1;
    zstd_bench_t *bench;
    if (!path) { errno = EINVAL; return NULL; }
    file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        if (size == 0) errno = EINVAL;
        fclose(file);
        return NULL;
    }
    bench = calloc(1, sizeof(*bench));
    if (!bench) { fclose(file); return NULL; }
    bench->size = (size_t)size;
    bench->data = malloc(bench->size);
    if (!bench->data) { fclose(file); free(bench); return NULL; }
    if (fread(bench->data, 1, bench->size, file) != bench->size) {
        int saved = ferror(file) ? EIO : EINVAL;
        fclose(file);
        free(bench->data);
        free(bench);
        errno = saved;
        return NULL;
    }
    fclose(file);
    return bench;
}

size_t zstd_bench_size(const zstd_bench_t *bench)
{
    return bench ? bench->size : 0;
}

void zstd_bench_destroy(zstd_bench_t *bench)
{
    if (bench) { free(bench->data); free(bench); }
}

zstd_bench_job_t *zstd_bench_job_create(const zstd_bench_t *bench,
                                         size_t length, int level, size_t offset)
{
    zstd_bench_job_t *job;
    size_t result;
    if (!bench || length == 0 || length > bench->size ||
        level < ZSTD_minCLevel() || level > ZSTD_maxCLevel()) {
        errno = EINVAL;
        return NULL;
    }
    job = calloc(1, sizeof(*job));
    if (!job) return NULL;
    if (offset > bench->size - length) offset = 0;
    job->input = bench->data + offset;
    job->input_size = length;
    job->compressed_capacity = ZSTD_compressBound(length);
    if (ZSTD_isError(job->compressed_capacity)) { errno = EINVAL; goto fail; }
    job->compressed = malloc(job->compressed_capacity);
    job->ctx = ZSTD_createCCtx();
    if (!job->compressed || !job->ctx) { errno = ENOMEM; goto fail; }
    result = ZSTD_CCtx_setParameter(job->ctx, ZSTD_c_compressionLevel, level);
    if (ZSTD_isError(result)) { errno = EINVAL; goto fail; }
    result = ZSTD_CCtx_setParameter(job->ctx, ZSTD_c_nbWorkers, 0);
    if (ZSTD_isError(result)) { errno = EINVAL; goto fail; }
    /* Prepare each context and output before measured execution. */
    if (zstd_bench_job_run(job) != 0) { errno = EIO; goto fail; }
    return job;
fail:
    zstd_bench_job_destroy(job);
    return NULL;
}

int zstd_bench_job_run(zstd_bench_job_t *job)
{
    size_t result;
    if (!job) { errno = EINVAL; return -1; }
    result = ZSTD_compress2(job->ctx, job->compressed, job->compressed_capacity,
                            job->input, job->input_size);
    if (ZSTD_isError(result)) {
        job->error = ZSTD_getErrorName(result);
        return -1;
    }
    job->error = NULL;
    job->compressed_size = result;
    return 0;
}

int zstd_bench_job_verify(zstd_bench_job_t *job)
{
    size_t result;
    void *decoded;
    if (!job) { errno = EINVAL; return -1; }
    decoded = malloc(job->input_size);
    if (!decoded) { job->error = "verification allocation failed"; return -1; }
    result = ZSTD_decompress(decoded, job->input_size,
                             job->compressed, job->compressed_size);
    if (ZSTD_isError(result)) {
        job->error = ZSTD_getErrorName(result);
        free(decoded);
        return -1;
    }
    if (result != job->input_size || memcmp(decoded, job->input, result) != 0) {
        job->error = "decompressed bytes do not match input";
        free(decoded);
        return -1;
    }
    free(decoded);
    job->error = NULL;
    return 0;
}

const char *zstd_bench_job_error(const zstd_bench_job_t *job)
{
    return job && job->error ? job->error : "no error";
}

void zstd_bench_job_destroy(zstd_bench_job_t *job)
{
    if (job) {
        ZSTD_freeCCtx(job->ctx);
        free(job->compressed);
        free(job);
    }
}

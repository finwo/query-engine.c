#ifndef __FINWO_QUERY_ENGINE_H__
#define __FINWO_QUERY_ENGINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "finwo/palloc.h"
#include "tidwall/buf.h"

/// Query engine
/// ============
///
/// Embeddable query engine for persistent storage

#define QUERY_ENGINE_RETURN_CODE   int
#define QUERY_ENGINE_RETURN_OK     0
#define QUERY_ENGINE_RETURN_ERR   -1

struct query_engine_t {
  PALLOC_FD fd;
  struct buf * (*serialize)(void *);
  void       * (*deserialize)(struct buf *);
  void         (*purge)(void *);
  void       * udata;
};

struct query_engine_result {
  void * idx;
  void * data;
};

struct query_engine_t * qe_init(const char **filename, struct buf * (*serialize)(void *), void * (*deserialize)(struct buf *), void (*purge)(void*), void *udata);
QUERY_ENGINE_RETURN_CODE qe_close(struct query_engine_t *instance);

QUERY_ENGINE_RETURN_CODE qe_index_add(struct query_engine_t *instance, const char *name, int (*cmp)(void *a, void *b, void *udata_qe, void *udata_index), void *udata);
QUERY_ENGINE_RETURN_CODE qe_index_del(struct query_engine_t *instance, const char *name);

struct query_engine_result * qe_query_run(struct query_engine_t *instance, void *pattern);

void qe_result_free(struct query_engine_t *instance, struct query_engine_result *result);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __FINWO_QUERY_ENGINE_H__

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
  struct buf * (*serialize)(void *, void *);
  void       * (*deserialize)(struct buf *, void *);
  void         (*purge)(void *, void *);
  void       * index;
  void       * udata;
};

struct query_engine_t * qe_init(const char *filename, struct buf * (*serialize)(void *, void *), void * (*deserialize)(struct buf *, void*), void (*purge)(void*, void*), void *udata, PALLOC_FLAGS flags);
QUERY_ENGINE_RETURN_CODE qe_close(struct query_engine_t *instance);

QUERY_ENGINE_RETURN_CODE qe_index_add(struct query_engine_t *instance, const char *name, int (*cmp)(const void *a, const void *b, void *udata_qe, void *udata_index), const char *filename, void *udata);
QUERY_ENGINE_RETURN_CODE qe_index_del(struct query_engine_t *instance, const char *name);

QUERY_ENGINE_RETURN_CODE qe_set(struct query_engine_t *instance, void *entry);
QUERY_ENGINE_RETURN_CODE qe_del(struct query_engine_t *instance, void *entry);
void * qe_query_run(struct query_engine_t *instance, const char *index, void *pattern);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __FINWO_QUERY_ENGINE_H__

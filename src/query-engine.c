#ifdef __cplusplus
extern "C" {
#endif

#include "finwo/mindex.h"

#include "query-engine.h"

struct qe_index {
  void            *next;
  char            *name;
  struct mindex_t *mindex;
};

/* struct query_engine_t { */
/*   PALLOC_FD fd; */
/*   struct buf * (*serialize)(void *, void *); */
/*   void       * (*deserialize)(struct buf *, void *); */
/*   void         (*purge)(void *); */
/*   void       * index; */
/*   void       * udata; */
/* }; */

struct query_engine_t * qe_init(const char **filename, struct buf * (*serialize)(void *), void * (*deserialize)(struct buf *, void*), void (*purge)(void*, void*), void *udata) {
  return NULL;
}

QUERY_ENGINE_RETURN_CODE qe_close(struct query_engine_t *instance) {
  return QUERY_ENGINE_RETURN_OK;
}

QUERY_ENGINE_RETURN_CODE qe_index_add(struct query_engine_t *instance, const char *name, int (*cmp)(void *a, void *b, void *udata_qe, void *udata_index), void *udata) {
  return QUERY_ENGINE_RETURN_OK;
}

QUERY_ENGINE_RETURN_CODE qe_index_del(struct query_engine_t *instance, const char *name) {
  return QUERY_ENGINE_RETURN_OK;
}

void * qe_query_run(struct query_engine_t *instance, const char *index, void *pattern) {
  return NULL;
}

#ifdef __cplusplus
} // extern "C"
#endif

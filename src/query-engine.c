#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>

#include "finwo/mindex.h"
#include "finwo/palloc.h"

#include "query-engine.h"

struct qe_index {
  void            *next;
  char            *name;
  void            *udata;
  struct mindex_t *mindex;
  const int (*cmp)(const void *a, const void *b, void *udata_qe, void *udata_idx);
  const struct query_engine_t *qe;
};

/* struct query_engine_t { */
/*   PALLOC_FD fd; */
/*   struct buf * (*serialize)(void *, void *); */
/*   void       * (*deserialize)(struct buf *, void *); */
/*   void         (*purge)(void *); */
/*   void       * index; */
/*   void       * udata; */
/* }; */

int cmp_internal(const void *a, const void *b, void *idx) {
  struct qe_index *index = (struct qe_index *)idx;
  return index->cmp(a, b, index->qe->udata, index->udata);
}

void purge_internal(void *subject, void *idx) {
  struct qe_index *index = (struct qe_index *)idx;
  return index->qe->purge(subject, index->qe->udata);
}

struct query_engine_t * qe_init(
  const char *filename,
  const struct buf * (*serialize)(void *, void *),
  const void * (*deserialize)(struct buf *, void*),
  const void (*purge)(void*, void*),
  void *udata,
  PALLOC_FLAGS flags
) {
  struct query_engine_t *instance = calloc(1, sizeof(struct query_engine_t));

  // Build our response
  instance->fd          = palloc_open(filename, flags);
  instance->serialize   = serialize;
  instance->deserialize = deserialize;
  instance->purge       = purge;
  instance->index       = NULL;
  instance->udata       = udata;

  // (Re)-initialize the medium
  palloc_init(instance->fd, flags);

  // Aanndd.. done
  return instance;
}

QUERY_ENGINE_RETURN_CODE qe_close(struct query_engine_t *instance) {
  return QUERY_ENGINE_RETURN_OK;
}

QUERY_ENGINE_RETURN_CODE qe_index_add(
  struct query_engine_t *instance,
  const char *name,
  const int (*cmp)(const void *a, const void *b, void *udata_qe, void *udata_index),
  void *udata
) {
  // Find if the index already exists
  struct qe_index *idx = instance->index;
  while(idx) {
    if (strcmp(idx->name,name) == 0) break;
    idx = idx->next;
  }
  if (idx) {
    /* fprintf(stderr,"Duplicate index '%s'", name); */
    return QUERY_ENGINE_RETURN_ERR;
  }

  idx = calloc(1, sizeof(struct qe_index));
  idx->next       = instance->index;
  idx->name       = strdup(name);
  idx->udata      = udata;
  idx->cmp        = cmp;
  idx->qe         = instance;

  idx->mindex = mindex_init(cmp_internal, purge_internal, idx);
  if (!idx->mindex) {
    free(idx->name);
    free(idx);
    return QUERY_ENGINE_RETURN_ERR;
  }

  instance->index = idx;
  return QUERY_ENGINE_RETURN_OK;
}

QUERY_ENGINE_RETURN_CODE qe_index_del(struct query_engine_t *instance, const char *name) {
  return QUERY_ENGINE_RETURN_OK;
}

void * qe_set(struct query_engine_t *instance, void *entry) {
  return NULL;
}

QUERY_ENGINE_RETURN_CODE qe_del(struct query_engine_t *instance, void *entry) {
  return QUERY_ENGINE_RETURN_OK;
}

void * qe_query_run(struct query_engine_t *instance, const char *index, void *pattern) {
  return NULL;
}

#ifdef __cplusplus
} // extern "C"
#endif

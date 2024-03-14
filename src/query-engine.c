#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "finwo/mindex.h"
#include "finwo/palloc.h"

#include "query-engine.h"

// OS-specific IO macros {{{
#if defined(_WIN32) || defined(_WIN64)
#define stat_os __stat64
#define fstat_os _fstat64
#define seek_os _lseeki64
#define open_os _open
#define write_os _write
#define read_os _read
#define close_os _close
#define unlink_os _unlink
#define O_CREAT _O_CREAT
#define O_RDWR  _O_RDWR
#define OPENMODE  (_S_IREAD | _S_IWRITE)
#define O_DSYNC 0
#define ssize_t SSIZE_T
#elif defined(__APPLE__)
#define stat_os stat
#define fstat_os fstat
#define seek_os lseek
#define open_os open
#define write_os write
#define read_os read
#define close_os close
#define unlink_os unlink
#define OPENMODE  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#else
#define stat_os stat64
#define fstat_os fstat64
#define seek_os lseek64
#define open_os open
#define write_os write
#define read_os read
#define close_os close
#define unlink_os unlink
#define OPENMODE  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#endif
// }}}

#if defined(_WIN32) || defined(_WIN64)
// Needs to be AFTER winsock2 which is used for endian.h
#include <windows.h>
#include <io.h>
#include <BaseTsd.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

struct qe_index {
  void            *next;
  char            *name;
  void            *udata;
  struct mindex_t *mindex;
  int (*cmp)(const void *a, const void *b, void *udata_qe, void *udata_idx);
  const struct query_engine_t *qe;
};

struct qe_index_entry {
  PALLOC_OFFSET ptr;
  void *hydrated;
};

/* struct query_engine_t { */
/*   PALLOC_FD fd; */
/*   struct buf * (*serialize)(void *, void *); */
/*   void       * (*deserialize)(struct buf *, void *); */
/*   void         (*purge)(void *); */
/*   void       * index; */
/*   void       * udata; */
/* }; */

// Read from medium, cmp, free deserialized entries
int cmp_internal(const void *a, const void *b, void *idx) {
  struct qe_index       *index   = (struct qe_index *)idx;
  struct qe_index_entry *entry_a = (struct qe_index_entry *)a;
  struct qe_index_entry *entry_b = (struct qe_index_entry *)b;
  struct buf *buf_a              = NULL;
  struct buf *buf_b              = NULL;

  int result = 0;

  void *hydrated_a = entry_a->hydrated;
  void *hydrated_b = entry_b->hydrated;

  // Hydrate if needed
  if (!(entry_a->hydrated)) {
    buf_a       = calloc(1, sizeof(struct buf));
    buf_a->cap  = palloc_size(index->qe->fd, entry_a->ptr);
    buf_a->len  = buf_a->cap;
    buf_a->data = malloc(buf_a->cap);
    seek_os(index->qe->fd, entry_a->ptr, SEEK_SET);
    if (read_os(index->qe->fd, buf_a->data, buf_a->len) != buf_a->len) {
      buf_clear(buf_a);
      free(buf_a);
      return 0;
    }
    hydrated_a = index->qe->serialize(buf_a, index->qe->udata);
  }
  if (!(entry_b->hydrated)) {
    buf_b       = calloc(1, sizeof(struct buf));
    buf_b->cap  = palloc_size(index->qe->fd, entry_b->ptr);
    buf_b->len  = buf_b->cap;
    buf_b->data = malloc(buf_b->cap);
    seek_os(index->qe->fd, entry_b->ptr, SEEK_SET);
    if (read_os(index->qe->fd, buf_b->data, buf_b->len) != buf_b->len) {
      if (!(entry_a->hydrated)) {
        buf_clear(buf_a);
        free(buf_a);
      }
      buf_clear(buf_b);
      free(buf_b);
      return 0;
    }
    hydrated_b = index->qe->serialize(buf_b, index->qe->udata);
  }

  // Run the actual comparison
  result = index->cmp(hydrated_a, hydrated_b, index->qe->udata, index->udata);

  // Don't hog memory
  if (!(entry_a->hydrated)) {
    buf_clear(buf_a);
    free(buf_a);
    index->qe->purge(hydrated_a, index->qe->udata);
  }
  if (!(entry_b->hydrated)) {
    buf_clear(buf_b);
    free(buf_b);
    index->qe->purge(hydrated_b, index->qe->udata);
  }

  // Return what the comparison said
  return result;
}

// Remove an entry from the in-memory index & persist
void purge_internal(void *sub, void *idx) {
  struct qe_index *index = (struct qe_index *)idx;
  struct qe_index_entry *subject = (struct qe_index_entry *)sub;

  if (subject->hydrated) {
    // This is a search pattern, do not free
  } else {
    pfree(index->qe->fd, subject->ptr);
    free(subject);
  }

  // Done
}

struct query_engine_t * qe_init(const char *filename, struct buf * (*serialize)(void *, void *), void * (*deserialize)(struct buf *, void*), void (*purge)(void*, void*), void *udata, PALLOC_FLAGS flags) {
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
  int (*cmp)(const void *a, const void *b, void *udata_qe, void *udata_index),
  void *udata
) {

  // TODO:
  // - check fs, maybe we have pre-indexed this
  // - add flags, we might have stored index
  // - index file MUST have "version", so we can detect if we need to rebuild
  // - then 8-byte (PALLOC_OFFSET) ordered list of entries

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

  // TODO: scan palloc & add to mindex

  instance->index = idx;
  return QUERY_ENGINE_RETURN_OK;
}

QUERY_ENGINE_RETURN_CODE qe_index_del(struct query_engine_t *instance, const char *name) {
  // Find if the index even exists
  struct qe_index *idx_prev = NULL;
  struct qe_index *idx      = instance->index;
  while(idx) {
    if (strcmp(idx->name,name) == 0) break;
    idx_prev = idx;
    idx      = idx->next;
  }
  if (!idx) {
    /* fprintf(stderr,"Duplicate index '%s'", name); */
    return QUERY_ENGINE_RETURN_OK;
  }

  // Remove the index from the list
  if (idx_prev) {
    idx_prev->next = idx->next;
  } else {
    instance->index = idx->next;
  }

  // Prevents the purge from removing data from medium
  idx->mindex->purge = NULL;

  // And free the index's memory
  mindex_free(idx->mindex);
  free(idx->name);
  free(idx);

  // Done
  return QUERY_ENGINE_RETURN_OK;
}

QUERY_ENGINE_RETURN_CODE qe_set(struct query_engine_t *instance, void *entry) {
  if (!(instance->index)) {
    return QUERY_ENGINE_RETURN_ERR;
  }

  // Turn into something we can write to disk
  struct buf *serialized = instance->serialize(entry, instance->udata);

  // Reserve persistent allocation
  PALLOC_OFFSET off = palloc(instance->fd, serialized->len);
  if (!off) {
    buf_clear(serialized);
    free(serialized);
    return QUERY_ENGINE_RETURN_ERR;
  }

  // Actually write to persistent storage
  seek_os(instance->fd, off, SEEK_SET);
  if (write_os(instance->fd, serialized->data, serialized->len) != serialized->len) {
    // TODO: handle gracefully
    buf_clear(serialized);
    free(serialized);
    return QUERY_ENGINE_RETURN_ERR;
  }

  // We no longer need the serialized data
  buf_clear(serialized);
  free(serialized);

  struct qe_index_entry *index_entry = calloc(1, sizeof(struct qe_index_entry));
  index_entry->ptr                   = off;

  // Add to all indexes
  // (auto-purges if duplicate found)
  struct qe_index *idx = instance->index;
  while(idx) {
    mindex_set(idx->mindex, index_entry);
    idx = idx->next;
  }

  return QUERY_ENGINE_RETURN_OK;
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

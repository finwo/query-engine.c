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
  const void *hydrated;
};

// Read from medium, cmp, free deserialized entries
int cmp_internal(const void *a, const void *b, void *idx) {
  struct qe_index       *index   = (struct qe_index *)idx;
  struct qe_index_entry *entry_a = (struct qe_index_entry *)a;
  struct qe_index_entry *entry_b = (struct qe_index_entry *)b;
  struct buf *buf_a              = NULL;
  struct buf *buf_b              = NULL;

  int result = 0;

  void *hydrated_a = (void *)entry_a->hydrated;
  void *hydrated_b = (void *)entry_b->hydrated;

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
    hydrated_a = index->qe->deserialize(buf_a, index->qe->udata);
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
    hydrated_b = index->qe->deserialize(buf_b, index->qe->udata);
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

struct query_engine_t * qe_init(const char *filename, struct buf * (*serialize)(const void *, void *), void * (*deserialize)(const struct buf *, void*), void (*purge)(void*, void*), void *udata, PALLOC_FLAGS flags) {
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
  if (!instance) return QUERY_ENGINE_RETURN_OK;

  palloc_close(instance->fd);
  free(instance);

  return QUERY_ENGINE_RETURN_OK;
}

QUERY_ENGINE_RETURN_CODE qe_index_add(
  struct query_engine_t *instance,
  const char *name,
  int (*cmp)(const void *a, const void *b, void *udata_qe, void *udata_index),
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

  // Initialize the index & register id
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

  // Scan entries and add to the index
  PALLOC_OFFSET entry = 0;
  struct qe_index_entry *idx_entry;
  while(1) {
    entry = palloc_next(instance->fd, entry);
    if (!entry) break;
    idx_entry      = calloc(1, sizeof(struct qe_index_entry));
    idx_entry->ptr = entry;
    mindex_set(idx->mindex, idx_entry);
  }

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

QUERY_ENGINE_RETURN_CODE qe_set(struct query_engine_t *instance, const void *entry) {
  if (!(instance->index)) {
    return QUERY_ENGINE_RETURN_ERR;
  }

  // Turn into something we can write to disk
  struct buf *serialized = instance->serialize(entry, instance->udata);
  if (!serialized) {
    return QUERY_ENGINE_RETURN_ERR;
  }

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


  // Add to all indexes
  // (auto-purges if duplicate found)
  struct qe_index_entry *index_entry = calloc(1, sizeof(struct qe_index_entry));
  index_entry->ptr                   = off;
  struct qe_index *idx = instance->index;
  while(idx) {
    mindex_set(idx->mindex, index_entry);
    idx = idx->next;
  }

  return QUERY_ENGINE_RETURN_OK;
}

QUERY_ENGINE_RETURN_CODE qe_del(struct query_engine_t *instance, const void *pattern) {
  struct qe_index       *idx              = NULL;
  struct qe_index_entry *pattern_internal = calloc(1, sizeof(struct qe_index_entry));
  pattern_internal->hydrated              = pattern;
  for( idx = instance->index ; idx ; idx = idx->next ) {
    mindex_delete(idx->mindex, pattern_internal);
  }
  free(pattern_internal);
  return QUERY_ENGINE_RETURN_OK;
}

void * qe_get(struct query_engine_t *instance, const char *index, void *pattern) {
  struct qe_index       *idx              = instance->index;
  struct qe_index_entry *pattern_internal = calloc(1, sizeof(struct qe_index_entry));
  pattern_internal->hydrated              = pattern;
  while(idx) {
    if (strcmp(idx->name, index) == 0) break;
    idx = idx->next;
  }
  if (!idx) {
    // No such index
    free(pattern_internal);
    return NULL;
  }

  struct qe_index_entry *entry = mindex_get(idx->mindex, pattern_internal);
  free(pattern_internal);
  if (!entry) {
    // Not found
    return NULL;
  }

  // Fetch the contents from the medium
  int n;
  struct buf *contents = calloc(1, sizeof(struct buf));
  contents->cap        = palloc_size(instance->fd, entry->ptr);
  contents->data       = malloc(contents->cap);
  while(contents->len < contents->cap) {
    seek_os(instance->fd, entry->ptr + contents->len, SEEK_SET);
    n = read_os(instance->fd, contents->data + contents->len, contents->cap - contents->len);
    if (n <= 0) {
      // Borked
      buf_clear(contents);
      free(contents);
      return NULL;
    }
    contents->len += n;
  }

  // Deserialize by the client
  void *deserialized = instance->deserialize(contents, instance->udata);

  buf_clear(contents);
  free(contents);

  return deserialized;
}

#ifdef __cplusplus
} // extern "C"
#endif

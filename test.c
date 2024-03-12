#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include "finwo/assert.h"
#include "tidwall/buf.h"

#include "query-engine.h"

#define QEUD_A ((void*)1)
#define QEUD_B ((void*)2)

struct entry {
  char *name;
  struct buf *data;
};

struct buf * serialize(void *entry_raw, void *udata) {
  ASSERT("_ser:: QE userdata is correct", udata == QEUD_A) NULL;
  struct entry *entry = (struct entry *)entry_raw;
  struct buf   *output = calloc(1, sizeof(struct buf));
  buf_append(output, entry->name, strlen(entry->name));
  buf_append(output, "\n", 1);
  buf_append(output, entry->data->data, entry->data->len);
  return output;
}

void * deserialize(struct buf *raw, void *udata) {
  ASSERT("_des:: QE userdata is correct", udata == QEUD_A) NULL;
  struct entry *output = calloc(1, sizeof(struct entry));
  struct buf   *dupped = calloc(1, sizeof(struct buf));
  output->data         = calloc(1, sizeof(struct buf));
  buf_append(dupped, raw->data, raw->len);
  buf_append(dupped, "\0", 1);
  char *name = strtok(dupped->data, "\n");
  output->name = strdup(name);
  buf_append(output->data, dupped->data + strlen(name) + 1, dupped->len - strlen(name) - 1 - 1);
  buf_clear(dupped);
  free(dupped);
  return output;
}

int cmp(const void *a, const void *b, void *udata_qe, void *udata_idx) {
  ASSERT("_cmp:: QE  userdata is correct", udata_qe  == QEUD_A) 0;
  ASSERT("_cmp:: IDX userdata is correct", udata_idx == QEUD_B) 0;
  struct entry *ea = (struct entry *)a;
  struct entry *eb = (struct entry *)b;
  return strcmp(ea->name, eb->name);
}

void purge(void *entry_raw, void *udata) {
  struct entry *entry = (struct entry *)entry_raw;
  if (entry->name) free(entry->name);
  if (entry->data) {
    buf_clear(entry->data);
    free(entry->data);
  }
  free(entry);
}

void test_main() {
  struct query_engine_t *qe = qe_init("pizza.db", &serialize, &deserialize, &purge, QEUD_A, PALLOC_DEFAULT | PALLOC_DYNAMIC);

  ASSERT("Adding the 'nam' index returns OK"       , qe_index_add(qe, "nam", &cmp, QEUD_B) == QUERY_ENGINE_RETURN_OK );
  ASSERT("Adding duplicate 'nam' index returns ERR", qe_index_add(qe, "nam", &cmp, QEUD_B) == QUERY_ENGINE_RETURN_ERR);

  ASSERT("Removing 'nam' index returns OK"       , qe_index_del(qe, "nam") == QUERY_ENGINE_RETURN_OK);
  ASSERT("Removing non-existing index returns OK", qe_index_del(qe, "nam") == QUERY_ENGINE_RETURN_OK);
  ASSERT("Re-adding 'nam' index return OK"       , qe_index_add(qe, "nam", &cmp, QEUD_B) == QUERY_ENGINE_RETURN_OK );

  struct entry *e_00 = calloc(1, sizeof(struct entry));
  e_00->name = strdup("pizza");
  e_00->data = calloc(1, sizeof(struct buf));
  buf_append(e_00->data, "calzone", 8);

  qe_set(qe, e_00);
}

int main() {
  RUN(test_main);
  return TEST_REPORT();
}

#ifdef __cplusplus
} // extern "C"
#endif

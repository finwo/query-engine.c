#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "finwo/benchmark.h"
#include "lib/finwo/canonical-path/src/canonical-path.h"
#include "query-engine.h"

struct entry {
  char *name;
  struct buf *data;
};

struct buf * serialize(const void *entry_raw, void *udata) {
  struct entry *entry = (struct entry *)entry_raw;
  struct buf   *output = calloc(1, sizeof(struct buf));
  buf_append(output, entry->name, strlen(entry->name));
  buf_append(output, "\n", 1);
  buf_append(output, entry->data->data, entry->data->len);
  return output;
}

void * deserialize(const struct buf *raw, void *udata) {
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

const char *alphabet = "0123456789abcdef";
char *random_str(int len) {
  if (!len) len = 16;
  char *output  = calloc(len + 1, sizeof(char));
  for(int i=0 ; i < len ; i++) {
    output[i] = alphabet[rand() % 16];
  }
  return output;
}

/* void test_main() { */
/*   struct query_engine_t *qe = qe_init("bmark.db", &serialize, &deserialize, &purge, NULL, PALLOC_DEFAULT | PALLOC_DYNAMIC); */
/*   qe_index_add(qe, "nam", &cmp, NULL); */

/*   // Build random entry */
/*   struct entry *e_00 = calloc(1, sizeof(struct entry)); */
/*   e_00->name         = random_str(8); */
/*   e_00->data         = calloc(1, sizeof(struct buf)); */
/*   e_00->data->len    = e_00->data->cap = 4; */
/*   e_00->data->data   = random_str(e_00->data->len - 1); */
/*   qe_set(qe, e_00); */

/*   // Build search pattern for that entry */
/*   struct entry *p_00 = &(struct entry){ */
/*     .name = e_00->name, */
/*   }; */

/*   // And retrieve it */
/*   struct entry *f_00 = qe_get(qe, "nam", p_00); */

/*   // Search pattern for non-existing entry */
/*   struct entry *p_01 = &(struct entry){ */
/*     .name = random_str(8), */
/*   }; */

/*   // And retrieve it */
/*   struct entry *f_01 = qe_get(qe, "nam", p_01); */
/*   ASSERT("get returns the null on known missing key", f_01 == NULL); */
/* } */

void mindex_bmark_rstr_1024() {
  for(int i=0; i<1024; i++) {
    free(random_str(16));
  }
}

void mindex_bmark_rstr_65536() {
  for(int i=0; i<65536; i++) {
    free(random_str(16));
  }
}

void mindex_bmark_assign_1024() {
  unlink(canonical_path("bmark.db"));
  struct query_engine_t *qe = qe_init("bmark.db", &serialize, &deserialize, &purge, NULL, PALLOC_DEFAULT | PALLOC_DYNAMIC);
  qe_index_add(qe, "nam", &cmp, NULL);
  struct entry *my_entry = calloc(1, sizeof(struct entry));
  my_entry->data         = calloc(1, sizeof(struct buf));
  my_entry->data->len    = my_entry->data->cap = 16;
  for(int i=0; i<1024; i++) {
    my_entry->name       = random_str(15);
    my_entry->data->data = random_str(my_entry->data->len - 1);
    qe_set(qe, my_entry);
    free(my_entry->name);
    free(my_entry->data->data);
  }
  qe_close(qe);
}

void mindex_bmark_assign_2048() {
  unlink(canonical_path("bmark.db"));
  struct query_engine_t *qe = qe_init("bmark.db", &serialize, &deserialize, &purge, NULL, PALLOC_DEFAULT | PALLOC_DYNAMIC);
  qe_index_add(qe, "nam", &cmp, NULL);
  struct entry *my_entry = calloc(1, sizeof(struct entry));
  my_entry->data         = calloc(1, sizeof(struct buf));
  my_entry->data->len    = my_entry->data->cap = 16;
  for(int i=0; i<2048; i++) {
    my_entry->name       = random_str(15);
    my_entry->data->data = random_str(my_entry->data->len - 1);
    qe_set(qe, my_entry);
    free(my_entry->name);
    free(my_entry->data->data);
  }
  qe_close(qe);
}


int main() {
  // Seed random
  srand(time(NULL));

  char percentiles[] = {
    1, 5, 50, 95, 99, 0
  };

  BMARK(mindex_bmark_assign_2048);
  BMARK(mindex_bmark_assign_1024);
  BMARK(mindex_bmark_rstr_65536);
  BMARK(mindex_bmark_rstr_1024);
  return bmark_run(100, percentiles);
  return 0;
}

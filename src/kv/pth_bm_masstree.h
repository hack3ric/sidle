#ifndef PTH_BM_MASSTREE_H
#define PTH_BM_MASSTREE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pth_bm_target_config {
  size_t table_size;
  int cxl_percentage;
  uint64_t max_local_memory_usage_mib;
} pth_bm_target_config;

pth_bm_target_config pth_bm_target_default_config(void);
void *pth_bm_target_create(void);
void *pth_bm_target_create_with_config(const pth_bm_target_config *config);
void pth_bm_target_init_thread(void *target);
void pth_bm_target_print_stat(void *target);
void pth_bm_target_destroy(void *target);
void pth_bm_target_read(void *target, uint64_t key);
void pth_bm_target_insert(void *target, uint64_t key);
void pth_bm_target_update(void *target, uint64_t key);
void pth_bm_target_delete(void *target, uint64_t key);

#ifdef __cplusplus
}
#endif

#endif

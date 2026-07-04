#ifndef MEMKIND_H
#define MEMKIND_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMKIND_ERROR_MESSAGE_SIZE 128

struct memkind;

int memkind_create_fixed(void *addr, size_t size, struct memkind **kind);
int memkind_destroy_kind(struct memkind *kind);
void memkind_error_message(int err, char *buf, size_t buf_size);
void *memkind_malloc(struct memkind *kind, size_t size);
void *memkind_calloc(struct memkind *kind, size_t num, size_t size);
void *memkind_realloc(struct memkind *kind, void *ptr, size_t size);
void memkind_free(struct memkind *kind, void *ptr);
int memkind_posix_memalign(struct memkind *kind, void **memptr,
                           size_t alignment, size_t size);

#ifdef __cplusplus
}
#endif

#endif

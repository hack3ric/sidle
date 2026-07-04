#include "memkind.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct memkind {
  void *base_addr;
  size_t size;
};

int memkind_create_fixed(void *addr, size_t size, struct memkind **kind) {
  if (kind == NULL) {
    return EINVAL;
  }

  *kind = malloc(sizeof(**kind));
  if (*kind == NULL) {
    return ENOMEM;
  }

  (*kind)->base_addr = addr;
  (*kind)->size = size;
  return 0;
}

int memkind_destroy_kind(struct memkind *kind) {
  free(kind);
  return 0;
}

void memkind_error_message(int err, char *buf, size_t buf_size) {
  if (buf == NULL || buf_size == 0) {
    return;
  }

  strerror_r(err, buf, buf_size);
  buf[buf_size - 1] = '\0';
}

void *memkind_malloc(struct memkind *kind, size_t size) {
  (void)kind;
  return malloc(size);
}

void *memkind_calloc(struct memkind *kind, size_t num, size_t size) {
  (void)kind;
  return calloc(num, size);
}

void *memkind_realloc(struct memkind *kind, void *ptr, size_t size) {
  (void)kind;
  return realloc(ptr, size);
}

void memkind_free(struct memkind *kind, void *ptr) {
  (void)kind;
  free(ptr);
}

int memkind_posix_memalign(struct memkind *kind, void **memptr,
                           size_t alignment, size_t size) {
  (void)kind;
  return posix_memalign(memptr, alignment, size);
}

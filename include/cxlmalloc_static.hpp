#include <cstddef>

extern "C" void *cxl_shm_malloc(size_t size);
extern "C" void cxl_shm_free(void *ptr);

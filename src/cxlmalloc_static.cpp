#include <cassert>
#include <cstdlib>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <numaif.h>

volatile static int init_count = 0;

#define REGION_SIZE (6*1024*1024*1024ULL + 24)

#include "cxlmalloc.h"
#include "cxlmalloc-internal.h" 
#include "mimalloc.h"
#include "numa-config.h"

// Note: cannot use static initializer due to LD_PRELOAD
// not running initializers in the correct order.
void init();
static void* heap = nullptr;

static thread_local cxl_shm _shm;
static __thread cxl_shm* shm = nullptr;
static const size_t LENGTH = (ZU(1) << 34);

extern "C" void* cxl_shm_malloc(size_t s) {
    init();

    if (s > 1024 - sizeof(CXLObj)) {
        return mi_malloc(s);
    }

    CXLRef allocation = shm->cxl_malloc(s, 0);
    return allocation.take_addr();
}

extern "C" void cxl_shm_free(void* p) {
    init();

    if (mi_check_owned(p)) {
        mi_free(p);
    }

    auto start = shm->get_start();
    if (start != nullptr && start <= p && p < (start + LENGTH)) {
        shm->cxl_free(false, (cxl_block*) (p - sizeof(CXLObj)));
    } else {
        // Possible if init function allocates using system
        // `malloc` before we interpose (?), and tries to free
        // in its fini function. Happens in `libnuma`.
    }
}

static void init() {
    if (heap == nullptr) {
        void* data = mmap(
            nullptr,
            LENGTH,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        );

        if (data == MAP_FAILED) {
            printf( "mmap buf failed: %s\n", strerror(errno) );
            exit(1);
        }

        if (const char* node = std::getenv("CXL_NUMA_NODE")) {
            unsigned long mask = 1 << std::strtoul(node, nullptr, 10);
            if(mbind(data, LENGTH, MPOL_BIND, &mask, sizeof(mask) * 8, 0) < 0) {
                printf( "mbind buf failed: %s\n", strerror(errno) );
                exit(1);
            }
        }

        heap = data;
    }

    if (shm == nullptr) {
        new (&_shm) cxl_shm(LENGTH, heap);
        shm = &_shm;
        shm->thread_init();
    }
}

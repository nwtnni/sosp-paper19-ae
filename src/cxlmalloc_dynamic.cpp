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

extern "C" void* malloc(size_t s) {
    init();

    if (s > 1024 - sizeof(CXLObj)) {
        return mi_malloc(s);
    }

    CXLRef allocation = shm->cxl_malloc(s, 0);
    return allocation.take_addr();
}

extern "C" void free(void* p) {
    auto start = shm->get_start();

    if (start != nullptr && start <= p && p < start + LENGTH) {
        shm->cxl_free(false, (cxl_block*) (p - sizeof(CXLObj)));
    } else if (mi_check_owned(p)) {
        mi_free(p);
    } else {
        // Possible if init function allocates using system
        // `malloc` before we interpose (?), and tries to free
        // in its fini function. Happens in `libnuma`.
    }
}

extern "C" size_t malloc_usable_size(void* p) {
    std::cerr << "malloc_usable_size" << std::endl;
    std::exit(-1);
}

extern "C" size_t memalign(size_t align, size_t size) {
    std::cerr << "memalign" << std::endl;
    std::exit(-1);
}

extern "C" int posix_memalign(void** pointer, size_t align, size_t size) {
    std::cerr << "posix_memalign" << std::endl;
    std::exit(-1);
}

extern "C" void* realloc(void* p, size_t size) {
    std::cerr << "realloc" << std::endl;
    std::exit(-1);
}

inline void init() {
    if (heap == nullptr) {
        auto shm_id = shmget(10, LENGTH, IPC_CREAT|0664);
        if (shm_id == -1) {
            printf("shmget failed: %s\n", strerror(errno));
            exit(1);
        }
        void* data = shmat(shm_id, NULL, 0);
        if (data == (void*)-1) {
            printf("shmat failed: %s\n", strerror(errno));
            exit(1);
        }

        // int numaNode = 1;
        // unsigned long nodemask = 0;
        // nodemask |= 1 << numaNode;
        // if(mbind(data, length, MPOL_BIND, &nodemask, sizeof(nodemask) * 8, 0) < 0) {
        //     printf( "mbind buf failed: %s\n", strerror(errno) );
        //     exit(1);
        // }

        heap = data;
    }

    if (shm == nullptr) {
        new (&_shm) cxl_shm(LENGTH, heap);
        shm = &_shm;
        shm->thread_init();
    }
}

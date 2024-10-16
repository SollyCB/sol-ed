#ifndef PRG_H
#define PRG_H

#include "../solh/sol.h"

#define TOTAL_MEM mb(32)
#define MAX_THREADS 1
#define MAIN_THREAD 0

#define MAIN_THREAD_SCRATCH_SIZE (TOTAL_MEM >> 2)
#define THREAD_DEFAULT_SCRATCH_SIZE ((TOTAL_MEM - MAIN_THREAD_SCRATCH_SIZE) / MAX_THREADS)

// use page size == 0
#define MAIN_THREAD_BLOCK_SIZE 0
#define THREAD_DEFAULT_BLOCK_SIZE 0

extern struct program {
    struct {
        allocator_t scratch;
        allocator_t persist;
    } allocs[MAX_THREADS];
    
    u32 thread_count;
} prg;

#define def_create_prg(name) void name(void)
def_create_prg(create_prg);

#endif // PRG_H

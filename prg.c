#include "prg.h"

struct program prg;

static struct thread_config {
    u64 scratch_size;
    u64 persist_size;
} thread_configs[] = {
    [MAIN_THREAD] = {
        .scratch_size = MAIN_THREAD_SCRATCH_SIZE,
        .persist_size = MAIN_THREAD_BLOCK_SIZE,
    },
};

def_create_prg(create_prg)
{
    create_os();
    prg.thread_count = MAX_THREADS < (os.thread_count >> 1) ? MAX_THREADS : (os.thread_count >> 1);
    
    for(u32 i=0; i < prg.thread_count; ++i) {
        if (thread_configs[i].scratch_size == 0) {
            create_allocator_linear(NULL, THREAD_DEFAULT_SCRATCH_SIZE, &prg.allocs[i].scratch);
        } else {
            create_allocator_linear(NULL, thread_configs[i].scratch_size, &prg.allocs[i].scratch);
        }
        
        if (thread_configs[i].persist_size == 0) {
            create_allocator_arena(os.page_size, &prg.allocs[i].persist);
        } else {
            create_allocator_arena(thread_configs[i].persist_size, &prg.allocs[i].persist);
        }
    }
    
    create_win();
    create_gpu();
}
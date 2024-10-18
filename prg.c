#include "prg.h"

struct program *prg;

def_create_prg(create_prg);
def_should_prg_shutdown(should_prg_shutdown);
def_should_prg_reload(should_prg_reload);
def_prg_update(prg_update);

def_prg_load(prg_load)
{
    prg = p;
    gpu = &prg->gpu;
    win = &prg->win;
    vdt = &prg->vdt;
    
    prg->fn.create = create_prg;
    prg->fn.should_shutdown = should_prg_shutdown;
    prg->fn.should_reload = should_prg_reload;
    prg->fn.update = prg_update;
}

internal struct thread_config {
    u64 scratch_size;
    u64 persist_size;
} thread_configs[] = {
    [MT] = {
        .scratch_size = MAIN_THREAD_SCRATCH_SIZE,
        .persist_size = MAIN_THREAD_BLOCK_SIZE,
    },
};

def_create_prg(create_prg)
{
    create_os();
    prg->thread_count = MAX_THREADS < (os.thread_count >> 1) ? MAX_THREADS : (os.thread_count >> 1);
    
    for(u32 i=0; i < prg->thread_count; ++i) {
        if (thread_configs[i].scratch_size == 0) {
            create_allocator_linear(NULL, THREAD_DEFAULT_SCRATCH_SIZE, &prg->allocs[i].scratch);
        } else {
            create_allocator_linear(NULL, thread_configs[i].scratch_size, &prg->allocs[i].scratch);
        }
        
        if (thread_configs[i].persist_size == 0) {
            create_allocator_arena(os.page_size, &prg->allocs[i].persist);
        } else {
            create_allocator_arena(thread_configs[i].persist_size, &prg->allocs[i].persist);
        }
    }
    
    create_win();
    create_gpu();
}

def_should_prg_shutdown(should_prg_shutdown)
{
    return win_should_close();
}

def_should_prg_reload(should_prg_reload)
{
    local_persist u32 mod_check_timer = 0;
    
    if (win_ms() < mod_check_timer)
        return false;
    
    mod_check_timer = win_ms() + secs_to_ms(2);
    if (cmp_ftim(FTIM_MOD, LIB_SRC, LIB_SRC_TEMP) < 0) {
        println("Library code is flagged for reload");
        return true;
    }
    
    return false;
}

def_prg_update(prg_update)
{
    prg->frames.cnt++;
    
    prg->time.dms = SDL_GetTicks() - prg->time.ms;
    prg->time.ms += prg->time.dms;
    
    if (prg->frames.cnt > 5 && prg->time.dms > prg->frames.worst)
        prg->frames.worst = prg->time.dms;
    
    prg->frames.avg = prg->time.ms / prg->frames.cnt;
    println("frame avg dt %u", prg->frames.avg);
    
    // This comparison seems to be wrong but functions correctly;
    // see equivalent check in should_prg_reload().
    if (cmp_ftim(FTIM_MOD, SH_SRC_URI, SH_VERT_SRC_URI) > 0 ||
        cmp_ftim(FTIM_MOD, SH_SRC_URI, SH_FRAG_SRC_URI) > 0)
    {
        println("Recompiling shaders");
        if (gpu_compile_shaders()) {
            log_error("Failed to compile shader code");
            if (!gpu->sh.vert || !gpu->sh.frag)
                return -1;
        }
    }
    
    win_poll();
    struct keyboard_input ki;
    while(win_kb_next(&ki)) { // @Todo
        char c = win_key_to_char(ki);
        if (ki.mod & RELEASE) {
            continue;
        } else if (c > 0) {
            println("Got input %c", c);
        } else {
            println("Input is not a text char");
        }
    }
    
    return 0;
}

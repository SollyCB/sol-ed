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
    
    prg->flags &= ~PRG_RLD;
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
    return prg->flags & PRG_RLD;
}

#define RLD_WT secs_to_ms(2) /* Time the hot reloader waits before checking for source changes */

def_prg_update(prg_update)
{
    prg->frames.cnt++;
    
    /* timers */
    prg->time.dms = SDL_GetTicks() - prg->time.ms;
    prg->time.ms += prg->time.dms;
    
    if (prg->frames.cnt > 5 && prg->time.dms > prg->frames.worst)
        prg->frames.worst = prg->time.dms;
    
    prg->frames.avg = prg->time.ms / prg->frames.cnt;
    // println("frame avg dt %u", prg->frames.avg);
    
    /* hotloader */
    internal u32 rld_timer = 0;
    if (rld_timer == 0)
        rld_timer = win_ms();
    
    if (rld_timer < win_ms()) {
        rld_timer += RLD_WT;
        
        if (cmpftim(FTIM_MOD, LIB_SRC, LIB_SRC_TEMP) < 0)
            prg->flags |= PRG_RLD;
        
        if (cmpftim(FTIM_MOD, SH_VERT_SRC_URI, SH_SRC_URI) < 0 ||
            cmpftim(FTIM_MOD, SH_FRAG_SRC_URI, SH_SRC_URI) < 0)
        {
            println("Recompiling shaders");
            // spirv parser to recreate pipeline layout?
            if (gpu_create_sh()) {
                log_error("Failed to recompile shader code after source change");
                if (!gpu->sh.vert || !gpu->sh.frag)
                    return -1;
            }
        }
    }
    
    /* window */
    win_poll();
    
    switch(win->flags & WIN_SZ) {
        case 0: break;
        
        case WIN_MAX:
        case WIN_RSZ: {
            gpu_handle_win_resize();
            win->flags &= ~WIN_SZ;
        } break;
        
        case WIN_MIN: {
            local_persist u32 pause_msg_timer = 0;
            if (pause_msg_timer == 0)
                pause_msg_timer = win_ms();
            
            if (pause_msg_timer < win_ms()) {
                pause_msg_timer += secs_to_ms(2);
                println("Paused while minimized");
            }
            return 0;
        } break;
        
        default:
        invalid_default_case;
    }
    
    struct keyboard_input ki;
    while(win_kb_next(&ki)) { // @Todo
        char c = win_key_to_char(ki);
        if (ki.mod & RELEASE) {
            continue;
        } else if (ki.key == KEY_ESCAPE) {
            win->flags |= WIN_CLO;
            //vkDestroyDevice(gpu->dev, NULL); // check if objects are being leaked
        } else if (c > 0) {
            println("Got input %c", c);
        } else {
            println("Input is not a text char");
        }
    }
    
    return 0;
}

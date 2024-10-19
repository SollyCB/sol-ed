#ifndef PRG_H
#define PRG_H

#include "../solh/sol.h"

#include "gpu.h"
#include "win.h"
#include "vdt.h"

#define FONT_URI "consolab.ttf"
#define FONT_HEIGHT 20

#define TOTAL_MEM mb(32)
#define MAX_THREADS 1
#define MT 0

#define MAIN_THREAD_SCRATCH_SIZE (TOTAL_MEM >> 2)
#define THREAD_DEFAULT_SCRATCH_SIZE ((TOTAL_MEM - MAIN_THREAD_SCRATCH_SIZE) / MAX_THREADS)

// 0 == use page size
#define MAIN_THREAD_BLOCK_SIZE 0
#define THREAD_DEFAULT_BLOCK_SIZE 0

struct program;

#define def_prg_load(name) void name(struct program *p)
typedef def_prg_load(prg_load_t);
dll_export def_prg_load(prg_load);

#define def_create_prg(name) void name(void)
typedef def_create_prg(create_prg_t);

#define def_should_prg_shutdown(name) bool name(void)
typedef def_should_prg_shutdown(should_prg_shutdown_t);

#define def_should_prg_reload(name) bool name(void)
typedef def_should_prg_reload(should_prg_reload_t);

#define def_prg_update(name) int name(void)
typedef def_prg_update(prg_update_t);

enum program_flags {
    PRG_RLD = 0x01,
};

struct program {
    struct {
        create_prg_t (*create);
        should_prg_shutdown_t (*should_shutdown);
        should_prg_reload_t (*should_reload);
        prg_update_t (*update);
    } fn;
    
    struct {
        allocator_t scratch;
        allocator_t persist;
    } allocs[MAX_THREADS];
    
    struct gpu gpu;
    struct win win;
    struct vdt vdt;
    
    u32 flags;
    u32 thread_count;
    
    struct {
        u32 ms; // time elapsed
        u32 dms;
    } time;
    
    struct {
        u32 cnt;
        u32 avg; // 1ms
        u32 worst; // 7-10ms
    } frames;
};

#ifdef LIB
extern struct program *prg;

#define salloc(ti, sz) allocate(&prg->allocs[ti].scratch, sz)
#define palloc(ti, sz) allocate(&prg->allocs[ti].persist, sz)

#endif

#endif // PRG_H

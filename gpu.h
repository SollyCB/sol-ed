#ifndef GPU_H
#define GPU_H

#include <vulkan/vulkan_core.h>

#include "shader.h"
#include "chars.h"

#define SC_MAX_IMGS 4 /* Arbitrarily small size that I doubt will be exceeded */
#define SC_MIN_IMGS 2
#define FRAME_WRAP 2

extern u32 frm_i;

enum {
    DB_SI_T, // transfer complete
    DB_SI_G, // color output complete
    DB_SI_I, // glyph upload
    DB_SEM_CNT,
};

#define GPU_MAX_CMDS 16

enum gpu_flags {
    GPU_MEM_INI = 0x01, // mem.type is valid
    GPU_MEM_UNI = 0x02, // mem arch is unified
    GPU_MEM_OFS = 0x04, // buffers use the top half this frame
    
    GPU_MEM_BITS = GPU_MEM_INI|GPU_MEM_UNI,
};

enum gpu_mem_indices {
    GPU_MI_G,
    GPU_MI_T,
    GPU_MI_I,
    GPU_MEM_CNT,
};

enum gpu_buf_indices {
    GPU_BI_G,
    GPU_BI_T,
    GPU_BUF_CNT,
};

enum gpu_q_indices {
    GPU_QI_G,
    GPU_QI_T,
    GPU_QI_P,
    GPU_Q_CNT,
};

// represents queues that can be submitted to
enum gpu_cmdq_indices {
    GPU_CI_G,
    GPU_CI_T,
    GPU_CMD_CNT
};

struct gpu {
    VkInstance inst;
    VkSurfaceKHR surf;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceMemoryProperties memprops;
    VkPhysicalDevice phys_dev;
    VkDevice dev;
    
    u32 flags;
    u32 q_cnt;
    
    struct {
        VkQueue handle;
        u32 i;
        struct {
            u32 buf_cnt;
            VkCommandPool pool;
            VkCommandBuffer bufs[GPU_MAX_CMDS];
        } cmd[FRAME_WRAP];
    } q[GPU_Q_CNT];
    
    struct {
        VkDeviceMemory handle;
        u32 type;
    } mem[GPU_MEM_CNT];
    
    struct {
        VkBuffer handle;
        void *data;
        u64 size;
        u64 used;
    } buf[GPU_BUF_CNT];
    
    struct gpu_glyph {
        VkImage img;
        VkImageView view;
        int x,y;
    } glyph[CHT_SZ];
    
    struct {
        struct extent_u16 dim_px; // pixel dimensions of a character cell
        struct extent_u16 win_dim_cells; // window dimensions in cells
        struct extent_f32 rdim_px; // reciprocals
        struct extent_f32 rwin_dim_cells;
        u32 cnt;
    } cell;
    
    struct {
        VkSwapchainKHR handle;
        VkSwapchainCreateInfoKHR info;
        VkImage imgs[SC_MAX_IMGS];
        VkImageView views[SC_MAX_IMGS];
        VkSemaphore sem[SC_MAX_IMGS];
        u32 img_i[SC_MAX_IMGS];
        u32 img_cnt,i;
    } sc;
    
    struct {
        VkShaderModule vert;
        VkShaderModule frag;
    } sh;
    
    VkPipelineLayout pll;
    VkPipeline pl;
    VkRenderPass rp;
    VkFramebuffer fb[SC_MAX_IMGS];
    
    VkDescriptorSetLayout dsl;
    VkDescriptorPool dp;
    VkDescriptorSet ds;
    
    VkSampler sampler;
    
    struct draw_buffer { // size == gpu.cell.cnt
        VkSemaphore sem[DB_SEM_CNT];
        VkFence fence[FRAME_WRAP];
        VkCommandBuffer glyph_upload_commands[GPU_CMD_CNT];
        large_set_t occupado;
        struct draw_info {
            struct rect_u16 pd;
            struct rgba fg,bg;
        } *di;
        u32 used; // number of occupied draw infos
        u32 in_use_fences; // bit mask
    } db;
};

#ifdef LIB
extern struct gpu *gpu;

#define def_create_gpu(name) int name(void)
def_create_gpu(create_gpu);

#define def_gpu_create_sh(name) int name(void)
def_gpu_create_sh(gpu_create_sh);

#define def_gpu_update(name) int name(void)
def_gpu_update(gpu_update);

#define def_gpu_check_leaks(name) void name(void)
def_gpu_check_leaks(gpu_check_leaks);

/**********************************************************************/
// gpu.c and vdt.c helper stuff

// TODO(SollyCB): I would REALLY like to have a way to define typesafe
// integers...

enum gpu_cmd_opt {
    GPU_CMD_OT = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    GPU_CMD_RE = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT,
};

union gpu_memreq_info {
    VkBufferCreateInfo *buf;
    VkImageCreateInfo *img;
};

enum cell_vertex_fmts {
    CELL_PD_FMT = VK_FORMAT_R16G16B16A16_UNORM,
    CELL_FG_FMT = VK_FORMAT_R8G8B8A8_UNORM,
    CELL_BG_FMT = VK_FORMAT_R8G8B8A8_UNORM,
    CELL_GL_FMT = VK_FORMAT_R8_UNORM,
};

extern u32 gpu_bi_to_mi[GPU_BUF_CNT];
extern u32 gpu_ci_to_qi[GPU_CMD_CNT];
extern char* gpu_mem_names[GPU_MEM_CNT];
extern char *gpu_cmdq_names[GPU_CMD_CNT];

#define gpu_buf_name(bi) gpu_mem_names[gpu_bi_to_mi[bi]]
#define gpu_cmd_name(ci) gpu_cmdq_names[ci]
#define gpu_cmd(ci) gpu->q[gpu_ci_to_qi[ci]].cmd[frm_i]

// calculate the size in bytes of the draw buffer
#define gpu_dba_sz(cc) (sizeof(*gpu->db.di) * cc)

#endif // ifdef LIB

#endif

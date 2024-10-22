#ifndef GPU_H
#define GPU_H

#include <vulkan/vulkan_core.h>

#include "shader.h"
#include "chars.h"

#define SC_MAX_IMGS 4 /* Arbitrarily small size that I doubt will be exceeded */
#define SC_MIN_IMGS 2

#define GPU_MAX_CMDS 16

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
        } cmd;
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
        struct rect_u32 dim; // dimensions of a character cell
        u32 cnt;
    } cell;
    
    struct {
        VkSwapchainKHR handle;
        VkSwapchainCreateInfoKHR info;
        VkImage imgs[SC_MAX_IMGS];
        VkImageView views[SC_MAX_IMGS];
        u32 img_cnt;
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
        struct draw_info {
            struct { u16 x,y; } ofs;
            struct { u16 x,y; } scl;
            u8 fg[4],bg[4];
        } *di;
        u32 used;
    } db;
};

#ifdef LIB
extern struct gpu *gpu;

#define def_create_gpu(name) int name(void)
def_create_gpu(create_gpu);

#define def_gpu_create_sh(name) int name(void)
def_gpu_create_sh(gpu_create_sh);

#define def_gpu_handle_win_resize(name) int name(void)
def_gpu_handle_win_resize(gpu_handle_win_resize);

#define def_gpu_check_leaks(name) void name(void)
def_gpu_check_leaks(gpu_check_leaks);

/**********************************************************************/
// gpu.c and vdt.c helper stuff

enum gpu_flags {
    GPU_MEM_INI = 0x01, // mem.type is valid
    GPU_MEM_UNI = 0x02, // mem arch is unified
    
    GPU_MEM_BITS = GPU_MEM_INI|GPU_MEM_UNI,
};

enum gpu_cmd_opt {
    GPU_CMD_OT = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    GPU_CMD_RE = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT,
};

// represents queues that can be submitted to
enum gpu_cmdq_indices {
    GPU_CI_G,
    GPU_CI_T,
    GPU_CMDQ_CNT
};

// map queues that can be submitted to to regular queue index
internal u32 gpu_ci_to_qi[] = {
    [GPU_CI_G] = GPU_QI_G,
    [GPU_CI_T] = GPU_QI_T,
};
#define gpu_cmd(ci) gpu->q[gpu_ci_to_qi[ci]].cmd

internal char *gpu_cmdq_names[] = {
    [GPU_CI_G] = "Graphics",
    [GPU_CI_T] = "Transfer",
};
#define gpu_cmd_name(ci) gpu_cmdq_names[ci]

internal u32 gpu_bi_to_mi[GPU_BUF_CNT] = {
    [GPU_BI_G] = GPU_MI_G,
    [GPU_BI_T] = GPU_MI_T,
};

internal char* gpu_mem_names[GPU_MEM_CNT] = {
    [GPU_MI_G] = "Vertex",
    [GPU_MI_T] = "Transfer",
    [GPU_MI_I] = "Image",
};

union gpu_memreq_info {
    VkBufferCreateInfo *buf;
    VkImageCreateInfo *img;
};

// pd:
//   - x,y: offsets from the top left corner of the screen
//   - z,w: x and y scalars (1 is the width or height of the screen)
// fg:
//   - x,y,z,w: rgba color
// bg:
//   - x,y,z: rgb color
//   - w: char index
struct gpu_cell_data {
    u16 pd[4];
    u8 fg[4];
    u8 bg[4];
};

enum cell_vertex_fmts {
    CELL_PD_FMT = VK_FORMAT_R16G16B16A16_UNORM,
    CELL_FG_FMT = VK_FORMAT_R8G8B8A8_UNORM,
    CELL_BG_FMT = VK_FORMAT_R8G8B8A8_UNORM,
    CELL_GL_FMT = VK_FORMAT_R8_UNORM,
};

// calculate the size in bytes of the draw buffer
#define gpu_dba_sz(cc) (sizeof(*gpu->db.di) * cc)

#endif // ifdef LIB

#endif

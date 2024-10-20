#ifndef GPU_H
#define GPU_H

#include <vulkan/vulkan.h>

#include "shader.h"
#include "chars.h"

#define SC_MAX_IMGS 4 /* Arbitrarily small size that I doubt will be exceeded */
#define SC_MIN_IMGS 2

enum gpu_mem_indices {
    GPU_MI_V,
    GPU_MI_T,
    GPU_MI_I,
    GPU_MEM_CNT,
};

enum gpu_buf_indices {
    GPU_BI_V,
    GPU_BI_T,
    GPU_BUF_CNT,
};

enum gpu_flags {
    GPU_MEM_INI = 0x01, // mem.type is valid
    GPU_MEM_UNI = 0x02, // mem arch is unified
    
    GPU_MEM_BITS = GPU_MEM_INI|GPU_MEM_UNI,
};

struct gpu_glyph {
    VkImage img;
    VkImageView view;
    int x,y,w,h;
};

struct gpu {
    VkInstance inst;
    VkSurfaceKHR surf;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceMemoryProperties memprops;
    VkPhysicalDevice phys_dev;
    VkDevice dev;
    
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
    
    struct gpu_glyph glyphs[CHT_SZ];
    
    u32 flags;
    
    struct { u32 g,t,p; } qi; // queue indices
    struct { VkQueue g,t,p; } qh; // queue handles
    
    struct {
        VkSwapchainKHR handle;
        VkSwapchainCreateInfoKHR info;
        
        u32 img_cnt;
        VkImage imgs[SC_MAX_IMGS];
        VkImageView views[SC_MAX_IMGS];
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
    VkDescriptorSet ds[CHT_SZ];
};

#ifdef LIB
extern struct gpu *gpu;

#define def_create_gpu(name) int name(void)
def_create_gpu(create_gpu);

#define def_gpu_create_sh(name) int name(void)
def_gpu_create_sh(gpu_create_sh);

#define def_gpu_handle_win_resize(name) int name(void)
def_gpu_handle_win_resize(gpu_handle_win_resize);

#endif // ifdef LIB

#endif

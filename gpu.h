#ifndef GPU_H
#define GPU_H

#include <vulkan/vulkan.h>

#define SC_MAX_IMGS 4
#define SC_MIN_IMGS 2

enum dsl_indices {
    DSL_UB,
    DSL_SI,
    DSL_CNT,
};

struct gpu {
    VkInstance inst;
    VkSurfaceKHR surf;
    VkPhysicalDevice phys_dev;
    VkDevice dev;
    VkPhysicalDeviceProperties props;
    
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
    VkDescriptorSetLayout dsl[DSL_CNT];
    VkFramebuffer fb[SC_MAX_IMGS];
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

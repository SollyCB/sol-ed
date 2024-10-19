#ifndef GPU_H
#define GPU_H

#include <vulkan/vulkan.h>

#include "shader.h"
#include "chars.h"

#define SC_MAX_IMGS 4 /* Arbitrarily small size that I doubt will be exceeded */
#define SC_MIN_IMGS 2

#define GPU_ALLOC_CNT 3 /* image, vertex, transfer */
#define GPU_BUF_CNT 2 /* vertex, transfer */

struct gpu {
    VkInstance inst;
    VkSurfaceKHR surf;
    VkPhysicalDeviceProperties props;
    VkPhysicalDevice phys_dev;
    VkDevice dev;
    
    struct {
        struct {
            VkDeviceMemory mem;
            u64 size;
            u64 used;
        } allocs[GPU_ALLOC_CNT];
        
        struct gpu_img {
            VkImage img;
            VkImageView view;
        } imgs[CHT_SZ];
        
        VkBuffer bufs[GPU_BUF_CNT];
    } mem;
    
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

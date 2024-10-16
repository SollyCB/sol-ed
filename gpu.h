#ifndef GPU_H
#define GPU_H

#include <vulkan/vulkan.h>

#define MAX_SWAPCHAIN_IMAGES 4
#define MIN_SWAPCHAIN_IMAGES 2

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
        VkImage images[MAX_SWAPCHAIN_IMAGES];
        VkImageView views[MAX_SWAPCHAIN_IMAGES];
    } sc;
    
    struct {
        VkShaderModule vert;
        VkShaderModule frag;
    } sh;
    
    VkPipelineLayout pll;
    VkPipeline pl;
};

#ifdef LIB
extern struct gpu *gpu;

#define def_create_gpu(name) int name(void)
def_create_gpu(create_gpu);

#define def_gpu_compile_shaders(name) int name(void)
def_gpu_compile_shaders(gpu_compile_shaders);

#endif // ifdef LIB

#endif

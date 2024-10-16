#ifndef GPU_H
#define GPU_H

#include <vulkan/vulkan.h>

extern struct gpu {
    VkInstance inst;
    VkSurfaceKHR surf;
    VkPhysicalDevice phys_dev;
    VkDevice dev;
    VkPhysicalDeviceProperties props;
    
    struct {
        u32 transfer;
        u32 graphics;
        u32 present;
    } queues;
} gpu;

#define def_create_gpu(name) int name(void)
def_create_gpu(create_gpu);

#endif

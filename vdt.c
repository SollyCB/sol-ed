#include "vdt.h"

struct vdt_elem vdt[VDT_SIZE] = {
    [VDT_EnumeratePhysicalDevices] = {.name = "vkEnumeratePhysicalDevices"},
    [VDT_GetPhysicalDeviceProperties] = {.name = "vkGetPhysicalDeviceProperties"},
    [VDT_GetPhysicalDeviceQueueFamilyProperties] = {.name = "vkGetPhysicalDeviceQueueFamilyProperties"},
    [VDT_GetPhysicalDeviceSurfaceSupportKHR] = {.name = "vkGetPhysicalDeviceSurfaceSupportKHR"},
    [VDT_CreateDevice] = {.name = "vkCreateDevice"},
    [VDT_GetPhysicalDeviceSurfaceCapabilitiesKHR] = {.name = "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"},
    [VDT_GetPhysicalDeviceSurfaceFormatsKHR] = {.name = "vkGetPhysicalDeviceSurfaceFormatsKHR"},
    [VDT_GetPhysicalDeviceSurfacePresentModesKHR] = {.name = "vkGetPhysicalDeviceSurfacePresentModesKHR"},
    [VDT_GetDeviceQueue] = {.name = "vkGetDeviceQueue"},
    [VDT_CreateSwapchainKHR] = {.name = "vkCreateSwapchainKHR"},
};

def_create_vdt(create_vdt)
{
    for(u32 i = VDT_INST_START; gpu.inst && !gpu.dev && i < VDT_INST_END; ++i) {
        vdt[i].fn = vkGetInstanceProcAddr(gpu.inst, vdt[i].name);
        if (!vdt[i].fn) {
            log_error("Failed to get pfn for %s", vdt[i].name);
            return -1;
        }
    }
    for(u32 i = VDT_DEV_START; gpu.dev && i < VDT_DEV_END; ++i) {
        vdt[i].fn = vkGetDeviceProcAddr(gpu.dev, vdt[i].name);
        if (!vdt[i].fn) {
            log_error("Failed to get pfn for %s", vdt[i].name);
            return -1;
        }
    }
    return 0;
}
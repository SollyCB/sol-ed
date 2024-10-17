#include "vdt.h"

#ifdef EXE
struct vdt_elem exevdt[VDT_SIZE] = {
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
    [VDT_GetSwapchainImagesKHR] = {.name = "vkGetSwapchainImagesKHR"},
    [VDT_CreateImageView] = {.name = "vkCreateImageView"},
    [VDT_DestroyImageView] = {.name = "vkDestroyImageView"},
};
#else
struct vdt *vdt;
def_create_vdt(create_vdt)
{
    for(u32 i = VDT_INST_START; gpu->inst && !gpu->dev && i < VDT_INST_END; ++i) {
        vdt->table[i].fn = vkGetInstanceProcAddr(gpu->inst, vdt->table[i].name);
        if (!vdt->table[i].fn) {
            log_error("Failed to get pfn for %s", vdt->table[i].name);
            return -1;
        }
    }
    for(u32 i = VDT_DEV_START; gpu->dev && i < VDT_DEV_END; ++i) {
        vdt->table[i].fn = vkGetDeviceProcAddr(gpu->dev, vdt->table[i].name);
        if (!vdt->table[i].fn) {
            log_error("Failed to get pfn for %s", vdt->table[i].name);
            return -1;
        }
    }
    return 0;
}
#endif // ifdef EXE
#ifndef VDT_H
#define VDT_H

#include "gpu.h"

enum {
    // Instance API
    VDT_EnumeratePhysicalDevices,
    VDT_GetPhysicalDeviceProperties,
    VDT_GetPhysicalDeviceQueueFamilyProperties,
    VDT_GetPhysicalDeviceSurfaceSupportKHR,
    VDT_CreateDevice,
    VDT_GetPhysicalDeviceSurfaceCapabilitiesKHR,
    VDT_GetPhysicalDeviceSurfaceFormatsKHR,
    VDT_GetPhysicalDeviceSurfacePresentModesKHR,
    VDT_INST_END,
    
    // Device API
    VDT_GetDeviceQueue,
    VDT_CreateSwapchainKHR,
    VDT_GetSwapchainImagesKHR,
    VDT_CreateImageView,
    VDT_DestroyImageView,
    VDT_CreateShaderModule,
    VDT_DestroyShaderModule,
    VDT_DEV_END,
    
    // Other meta info
    VDT_INST_START = 0,
    VDT_DEV_START = VDT_INST_END + 1,
    VDT_SIZE = VDT_DEV_END,
};

struct vdt_elem {
    char *name;
    PFN_vkVoidFunction fn;
};

struct vdt {
    struct vdt_elem *table;
};

#ifdef EXE
extern struct vdt_elem exevdt[VDT_SIZE];
#else
extern struct vdt *vdt;

#define def_create_vdt(name) int name(void)
def_create_vdt(create_vdt);

/***************************************************************/

#define GAC NULL

#ifdef DEBUG
#define def_cvk(name) VkResult name(VkResult res, char *file, int line, char *fn)
def_cvk(cvk_fn);
#define cvk(res) cvk_fn(res, __FILE__, __LINE__, __FUNCTION__)
#else
#define cvk(res)
#endif

#define vdt_call(name) ((PFN_vk ## name)(vdt->table[VDT_ ## name].fn))

static inline VkResult vk_create_instance(VkInstanceCreateInfo *info) {
    return cvk(vkCreateInstance(info, GAC, &gpu->inst));
}

static inline VkResult vk_enumerate_physical_devices(u32 *cnt, VkPhysicalDevice *devs) {
    return cvk(vdt_call(EnumeratePhysicalDevices)(gpu->inst, cnt, devs));
}

static inline void vk_get_physical_device_properties(VkPhysicalDevice dev, VkPhysicalDeviceProperties *props) {
    vdt_call(GetPhysicalDeviceProperties)(dev, props);
}

static inline void vk_get_physical_device_queue_family_properties(u32 *cnt, VkQueueFamilyProperties *props) {
    vdt_call(GetPhysicalDeviceQueueFamilyProperties)(gpu->phys_dev, cnt, props);
}

static inline VkResult vk_get_physical_device_surface_support_khr(u32 qfi, b32 *support) {
    return cvk(vdt_call(GetPhysicalDeviceSurfaceSupportKHR)(gpu->phys_dev, qfi, gpu->surf, support));
}

static inline VkResult vk_create_device(VkDeviceCreateInfo *ci) {
    return cvk(vdt_call(CreateDevice)(gpu->phys_dev, ci, GAC, &gpu->dev));
}

static inline void vk_get_physical_device_surface_capabilities_khr(VkSurfaceCapabilitiesKHR *cap) {
    vdt_call(GetPhysicalDeviceSurfaceCapabilitiesKHR)(gpu->phys_dev, gpu->surf, cap);
}

static inline void vk_get_physical_device_surface_formats_khr(u32 *cnt, VkSurfaceFormatKHR *fmts) {
    vdt_call(GetPhysicalDeviceSurfaceFormatsKHR)(gpu->phys_dev, gpu->surf, cnt, fmts);
}

static inline void vk_get_device_queue(u32 qi, VkQueue *qh) {
    vdt_call(GetDeviceQueue)(gpu->dev, qi, 0, qh);
}

static inline VkResult vk_create_swapchain_khr(void) {
    return cvk(vdt_call(CreateSwapchainKHR)(gpu->dev, &gpu->sc.info, GAC, &gpu->sc.handle));
}

static inline VkResult vk_get_swapchain_images_khr(u32 *cnt, VkImage *imgs) {
    return cvk(vdt_call(GetSwapchainImagesKHR)(gpu->dev, gpu->sc.handle, cnt, imgs));
}

static inline VkResult vk_create_image_view(VkImageViewCreateInfo *ci, VkImageView *view) {
    return cvk(vdt_call(CreateImageView)(gpu->dev, ci, GAC, view));
}

static inline void vk_destroy_image_view(VkImageView view) {
    vdt_call(DestroyImageView)(gpu->dev, view, GAC);
}

static inline VkResult vk_create_shader_module(VkShaderModuleCreateInfo *ci, VkShaderModule *mod) {
    return cvk(vdt_call(CreateShaderModule)(gpu->dev, ci, GAC, mod));
}

static inline void vk_destroy_shader_module(VkShaderModule mod) {
    vdt_call(DestroyShaderModule)(gpu->dev, mod, GAC);
}

def_cvk(cvk_fn)
{
    char *err = "Unknown VkResult (may be an extension)";
    switch(res) {
        case VK_SUCCESS: return 0;
        case VK_NOT_READY: err = "VK_NOT_READY"; break;
        case VK_TIMEOUT: err = "VK_TIMEOUT"; break;
        case VK_EVENT_SET: err = "VK_EVENT_SET"; break;
        case VK_EVENT_RESET: err = "VK_EVENT_RESET"; break;
        case VK_INCOMPLETE: err = "VK_INCOMPLETE"; break;
        case VK_ERROR_OUT_OF_HOST_MEMORY: err = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: err = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
        case VK_ERROR_INITIALIZATION_FAILED: err = "VK_ERROR_INITIALIZATION_FAILED"; break;
        case VK_ERROR_DEVICE_LOST: err = "VK_ERROR_DEVICE_LOST"; break;
        case VK_ERROR_MEMORY_MAP_FAILED: err = "VK_ERROR_MEMORY_MAP_FAILED"; break;
        case VK_ERROR_LAYER_NOT_PRESENT: err = "VK_ERROR_LAYER_NOT_PRESENT"; break;
        case VK_ERROR_EXTENSION_NOT_PRESENT: err = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
        case VK_ERROR_FEATURE_NOT_PRESENT: err = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
        case VK_ERROR_INCOMPATIBLE_DRIVER: err = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
        case VK_ERROR_TOO_MANY_OBJECTS: err = "VK_ERROR_TOO_MANY_OBJECTS"; break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED: err = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
        case VK_ERROR_FRAGMENTED_POOL: err = "VK_ERROR_FRAGMENTED_POOL"; break;
        case VK_ERROR_UNKNOWN: err = "VK_ERROR_UNKNOWN"; break;
        case VK_ERROR_OUT_OF_POOL_MEMORY: err = "VK_ERROR_OUT_OF_POOL_MEMORY"; break;
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: err = "VK_ERROR_INVALID_EXTERNAL_HANDLE"; break;
        case VK_ERROR_FRAGMENTATION: err = "VK_ERROR_FRAGMENTATION"; break;
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: err = "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"; break;
        case VK_PIPELINE_COMPILE_REQUIRED: err = "VK_PIPELINE_COMPILE_REQUIRED"; break;
        default: break;
    }
    log_error("[%s, %u, %s] %s (%i)", file, line, fn, err, (s64)res);
    return res;
}
#endif // LIB

#endif // VDT_H

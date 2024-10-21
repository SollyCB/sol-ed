#ifndef VDT_H
#define VDT_H

#include "gpu.h"

enum {
    // Instance API
    VDT_EnumeratePhysicalDevices,
    VDT_GetPhysicalDeviceProperties,
    VDT_GetPhysicalDeviceMemoryProperties,
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
    VDT_DestroySwapchainKHR,
    VDT_GetSwapchainImagesKHR,
    VDT_CreateBuffer,
    VDT_DestroyBuffer,
    VDT_CreateImage,
    VDT_DestroyImage,
    VDT_GetBufferMemoryRequirements,
    VDT_GetImageMemoryRequirements,
    VDT_AllocateMemory,
    VDT_MapMemory,
    VDT_FreeMemory,
    VDT_BindBufferMemory,
    VDT_BindImageMemory,
    VDT_CreateImageView,
    VDT_DestroyImageView,
    VDT_CreateShaderModule,
    VDT_DestroyShaderModule,
    VDT_CreateDescriptorSetLayout,
    VDT_DestroyDescriptorSetLayout,
    VDT_CreatePipelineLayout,
    VDT_DestroyPipelineLayout,
    VDT_CreateDescriptorPool,
    VDT_DestroyDescriptorPool,
    VDT_AllocateDescriptorSets,
    VDT_CreateRenderPass,
    VDT_DestroyRenderPass,
    VDT_CreateFramebuffer,
    VDT_DestroyFramebuffer,
    VDT_CreateGraphicsPipelines,
    VDT_DestroyPipeline,
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

static inline VkResult vk_create_inst(VkInstanceCreateInfo *info) {
    return cvk(vkCreateInstance(info, GAC, &gpu->inst));
}

static inline VkResult vk_enum_phys_devs(u32 *cnt, VkPhysicalDevice *devs) {
    return cvk(vdt_call(EnumeratePhysicalDevices)(gpu->inst, cnt, devs));
}

static inline void vk_get_phys_dev_props(VkPhysicalDevice dev, VkPhysicalDeviceProperties *props) {
    vdt_call(GetPhysicalDeviceProperties)(dev, props);
}

static inline void vk_get_phys_dev_memprops(VkPhysicalDevice dev, VkPhysicalDeviceMemoryProperties *props) {
    vdt_call(GetPhysicalDeviceMemoryProperties)(dev, props);
}

static inline void vk_get_phys_devq_fam_props(u32 *cnt, VkQueueFamilyProperties *props) {
    vdt_call(GetPhysicalDeviceQueueFamilyProperties)(gpu->phys_dev, cnt, props);
}

static inline VkResult vk_get_phys_dev_surf_support_khr(u32 qfi, b32 *support) {
    return cvk(vdt_call(GetPhysicalDeviceSurfaceSupportKHR)(gpu->phys_dev, qfi, gpu->surf, support));
}

static inline VkResult vk_create_dev(VkDeviceCreateInfo *ci) {
    return cvk(vdt_call(CreateDevice)(gpu->phys_dev, ci, GAC, &gpu->dev));
}

static inline void vk_get_phys_dev_surf_cap_khr(VkSurfaceCapabilitiesKHR *cap) {
    vdt_call(GetPhysicalDeviceSurfaceCapabilitiesKHR)(gpu->phys_dev, gpu->surf, cap);
}

static inline void vk_get_phys_dev_surf_fmts_khr(u32 *cnt, VkSurfaceFormatKHR *fmts) {
    vdt_call(GetPhysicalDeviceSurfaceFormatsKHR)(gpu->phys_dev, gpu->surf, cnt, fmts);
}

static inline void vk_get_devq(u32 qi, VkQueue *qh) {
    vdt_call(GetDeviceQueue)(gpu->dev, qi, 0, qh);
}

static inline VkResult vk_create_sc_khr(VkSwapchainCreateInfoKHR *ci, VkSwapchainKHR *sc) {
    return cvk(vdt_call(CreateSwapchainKHR)(gpu->dev, ci, GAC, sc));
}

static inline void vk_destroy_sc_khr(VkSwapchainKHR sc) {
    return vdt_call(DestroySwapchainKHR)(gpu->dev, sc, GAC);
}

static inline VkResult vk_get_sc_imgs_khr(u32 *cnt, VkImage *imgs) {
    return cvk(vdt_call(GetSwapchainImagesKHR)(gpu->dev, gpu->sc.handle, cnt, imgs));
}

static inline VkResult vk_create_buf(VkBufferCreateInfo *ci, VkBuffer *buf) {
    return cvk(vdt_call(CreateBuffer)(gpu->dev, ci, GAC, buf));
}

static inline void vk_destroy_buf(VkBuffer buf) {
    vdt_call(DestroyBuffer)(gpu->dev, buf, GAC);
}

static inline VkResult vk_create_img(VkImageCreateInfo *ci, VkImage *img) {
    return cvk(vdt_call(CreateImage)(gpu->dev, ci, GAC, img));
}

static inline void vk_destroy_img(VkImage img) {
    vdt_call(DestroyImage)(gpu->dev, img, GAC);
}

static inline void vk_get_buf_memreq(VkBuffer buf, VkMemoryRequirements *mr) {
    vdt_call(GetBufferMemoryRequirements)(gpu->dev, buf, mr);
}

static inline void vk_get_img_memreq(VkImage img, VkMemoryRequirements *mr) {
    vdt_call(GetImageMemoryRequirements)(gpu->dev, img, mr);
}

static inline VkResult vk_alloc_mem(VkMemoryAllocateInfo *ci, VkDeviceMemory *mem) {
    return cvk(vdt_call(AllocateMemory)(gpu->dev, ci, GAC, mem));
}

static inline VkResult vk_map_mem(VkDeviceMemory mem, u64 ofs, u64 sz, void **p) {
    return cvk(vdt_call(MapMemory)(gpu->dev, mem, ofs, sz, 0x0, p));
}

static inline void vk_free_mem(VkDeviceMemory mem) {
    vdt_call(FreeMemory)(gpu->dev, mem, GAC);
}

static inline VkResult vk_bind_img_mem(VkImage img, VkDeviceMemory mem, u64 ofs) {
    return cvk(vdt_call(BindImageMemory)(gpu->dev, img, mem, ofs));
}

static inline VkResult vk_bind_buf_mem(VkBuffer buf, VkDeviceMemory mem, u64 ofs) {
    return cvk(vdt_call(BindBufferMemory)(gpu->dev, buf, mem, ofs));
}

static inline VkResult vk_create_imgv(VkImageViewCreateInfo *ci, VkImageView *view) {
    return cvk(vdt_call(CreateImageView)(gpu->dev, ci, GAC, view));
}

static inline void vk_destroy_imgv(VkImageView view) {
    vdt_call(DestroyImageView)(gpu->dev, view, GAC);
}

static inline VkResult vk_create_shmod(VkShaderModuleCreateInfo *ci, VkShaderModule *mod) {
    return cvk(vdt_call(CreateShaderModule)(gpu->dev, ci, GAC, mod));
}

static inline void vk_destroy_shmod(VkShaderModule mod) {
    vdt_call(DestroyShaderModule)(gpu->dev, mod, GAC);
}

static inline VkResult vk_create_dsl(VkDescriptorSetLayoutCreateInfo *ci, VkDescriptorSetLayout *dsl) {
    return cvk(vdt_call(CreateDescriptorSetLayout)(gpu->dev, ci, GAC, dsl));
}

static inline void vk_destroy_dsl(VkDescriptorSetLayout dsl) {
    vdt_call(DestroyDescriptorSetLayout)(gpu->dev, dsl, GAC);
}

static inline VkResult vk_create_pll(VkPipelineLayoutCreateInfo *ci, VkPipelineLayout *pll) {
    return cvk(vdt_call(CreatePipelineLayout)(gpu->dev, ci, GAC, pll));
}

static inline void vk_destroy_pll(VkPipelineLayout pll) {
    vdt_call(DestroyPipelineLayout)(gpu->dev, pll, GAC);
}

static inline VkResult vk_create_dp(VkDescriptorPoolCreateInfo *ci, VkDescriptorPool *dp) {
    return cvk(vdt_call(CreateDescriptorPool)(gpu->dev, ci, GAC, dp));
}

static inline void vk_destroy_dp(VkDescriptorPool dp) {
    vdt_call(DestroyDescriptorPool)(gpu->dev, dp, GAC);
}

static inline VkResult vk_alloc_ds(VkDescriptorSetAllocateInfo *ai, VkDescriptorSet *ds) {
    return cvk(vdt_call(AllocateDescriptorSets)(gpu->dev, ai, ds));
}

static inline VkResult vk_create_rp(VkRenderPassCreateInfo *ci, VkRenderPass *rp) {
    return cvk(vdt_call(CreateRenderPass)(gpu->dev, ci, GAC, rp));
}

static inline void vk_destroy_rp(VkRenderPass rp) {
    vdt_call(DestroyRenderPass)(gpu->dev, rp, GAC);
}

static inline VkResult vk_create_fb(VkFramebufferCreateInfo *ci, VkFramebuffer *fb) {
    return cvk(vdt_call(CreateFramebuffer)(gpu->dev, ci, GAC, fb));
}

static inline void vk_destroy_fb(VkFramebuffer fb) {
    vdt_call(DestroyFramebuffer)(gpu->dev, fb, GAC);
}

static inline VkResult vk_create_gpl(u32 cnt, VkGraphicsPipelineCreateInfo *ci, VkPipeline *pl) {
    return cvk(vdt_call(CreateGraphicsPipelines)(gpu->dev, NULL, cnt, ci, GAC, pl));
}

static inline void vk_destroy_pl(VkPipeline pl) {
    vdt_call(DestroyPipeline)(gpu->dev, pl, GAC);
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

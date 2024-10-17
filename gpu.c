#include "prg.h"
#include "win.h"
#include "vdt.h"
#include "gpu.h"

struct gpu *gpu;

internal int create_swapchain(void)
{
    gpu->sc.info.imageExtent = (VkExtent2D) {.width = win->dim.w, .height = win->dim.h};
    
    if (vk_create_swapchain_khr())
        return -1;
    gpu->sc.info.oldSwapchain = gpu->sc.handle;
    
    if (gpu->sc.views[0]) {
        for(u32 i=0; i < gpu->sc.img_cnt; ++i)
            vk_destroy_image_view(gpu->sc.views[i]);
        memset(gpu->sc.views, 0, sizeof(gpu->sc.views));
    }
    if (vk_get_swapchain_images_khr(&gpu->sc.img_cnt, gpu->sc.images))
        return -1;
    
    u32 i;
    for(i=0; i < gpu->sc.img_cnt; ++i) {
        VkImageViewCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image = gpu->sc.images[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = gpu->sc.info.imageFormat;
        ci.subresourceRange = (VkImageSubresourceRange) {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        if (vk_create_image_view(&ci, &gpu->sc.views[i]))
            goto view_fail;
    }
    
    return 0;
    
    view_fail:
    while(--i < Max_u32)
        vk_destroy_image_view(gpu->sc.views[i]);
    return -1;
}

def_create_gpu(create_gpu)
{
    {
        VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
        ai.pApplicationName = "name";
        ai.applicationVersion = 0;
        ai.pEngineName = "engine";
        ai.engineVersion = 0;
        ai.apiVersion = VK_API_VERSION_1_3;
        
#define MAX_EXTENSION_COUNT 8
        u32 ext_count = 0;
        char *exts[MAX_EXTENSION_COUNT];
        win_inst_exts(&ext_count, NULL); assert(ext_count <= MAX_EXTENSION_COUNT);
        win_inst_exts(&ext_count, exts);
        
        VkInstanceCreateInfo ci = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ci.pApplicationInfo = &ai;
        ci.enabledExtensionCount = ext_count;
        ci.ppEnabledExtensionNames = exts;
        
        if (vk_create_instance(&ci))
            return -1;
        
        if (win_create_surf())
            return -1;
#undef MAX_EXTENSION_COUNT
    }
    
    create_vdt(); // initialize instance api calls
    
    {
#define MAX_DEVICE_COUNT 8
        u32 cnt;
        VkPhysicalDevice pd[MAX_DEVICE_COUNT];
        vk_enumerate_physical_devices(&cnt, NULL); assert(cnt <= MAX_DEVICE_COUNT);
        vk_enumerate_physical_devices(&cnt, pd);
        
        VkPhysicalDeviceProperties props[MAX_DEVICE_COUNT];
        u32 discrete   = Max_u32;
        u32 integrated = Max_u32;
        
        for(u32 i=0; i < cnt; ++i) {
            vk_get_physical_device_properties(pd[i], props);
            if (props[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                discrete = i;
                break;
            } else if (props[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                integrated = i;
            }
        }
        
        if (discrete != Max_u32) {
            gpu->phys_dev = pd[discrete];
            gpu->props = props[discrete];
        } else if (integrated != Max_u32) {
            gpu->phys_dev = pd[integrated];
            gpu->props = props[integrated];
        } else {
            log_error("Failed to find device with appropriate type");
            return -1;
        }
#undef MAX_DEVICE_COUNT
    }
    
    {
#define MAX_QUEUE_COUNT 8
        u32 cnt;
        VkQueueFamilyProperties fp[MAX_QUEUE_COUNT];
        vk_get_physical_device_queue_family_properties(&cnt, NULL); assert(cnt <= MAX_QUEUE_COUNT);
        vk_get_physical_device_queue_family_properties(&cnt, fp);
        
        u32 graphics = Max_u32;
        u32 present  = Max_u32;
        u32 transfer = Max_u32;
        for(u32 i=0; i < cnt; ++i) {
            b32 surf;
            vk_get_physical_device_surface_support_khr(i, &surf);
            if (surf && present == Max_u32) {
                present = i;
            }
            if ((fp[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                (fp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                graphics == Max_u32)
            {
                graphics = i;
            }
            if ((fp[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                !(fp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                transfer == Max_u32)
            {
                transfer = i;
            }
        }
        
        if (graphics == Max_u32 || present == Max_u32) {
            log_error_if(graphics == Max_u32,
                         "physical device %s does not support graphics operations",
                         gpu->props.deviceName);
            log_error_if(present == Max_u32,
                         "physical device %s does not support present operations",
                         gpu->props.deviceName);
            return -1;
        }
        
        if (transfer == Max_u32) transfer = graphics;
        
        float priority = 1;
        VkDeviceQueueCreateInfo qci[3];
        u32 qc = 0;
        
        qci[qc] = (VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = graphics,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
        qc++;
        
        qci[qc] = (VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = present,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
        qc += (present != graphics);
        
        qci[qc] = (VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = transfer,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
        qc += (transfer != graphics);
        
        char *ext_names[] = {
            "VK_KHR_swapchain",
        };
        
        VkDeviceCreateInfo ci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        ci.queueCreateInfoCount = qc;
        ci.pQueueCreateInfos = qci;
        ci.enabledExtensionCount = cl_array_size(ext_names);
        ci.ppEnabledExtensionNames = ext_names;
        
        if (vk_create_device(&ci))
            return -1;
#undef MAX_QUEUE_COUNT
        
        gpu->qi.g = graphics;
        gpu->qi.p = present;
        gpu->qi.t = transfer;
    }
    
    create_vdt(); // initialize device api calls
    
    {
        vk_get_device_queue(gpu->qi.g, &gpu->qh.g);
        
        if (gpu->qi.p != gpu->qi.g)
            vk_get_device_queue(gpu->qi.p, &gpu->qh.p);
        else
            gpu->qi.p = gpu->qi.g;
        
        if (gpu->qi.t != gpu->qi.g)
            vk_get_device_queue(gpu->qi.t, &gpu->qh.t);
        else
            gpu->qi.t = gpu->qi.g;
    }
    
    {
        VkSurfaceCapabilitiesKHR cap;
        vk_get_physical_device_surface_capabilities_khr(&cap);
        
        log_error_if(cap.currentExtent.width != win->dim.w || cap.currentExtent.height != win->dim.h,
                     "Window dimensions do not match those reported by surface capabilities");
        
        if (cap.minImageCount > MAX_SWAPCHAIN_IMAGES) {
            log_error("MAX_SWAPCHAIN_IMAGES is smaller that minimum required swapchain images");
            return -1;
        }
        if (cap.maxImageCount < MIN_SWAPCHAIN_IMAGES && cap.maxImageCount != 0) {
            log_error("MIN_SWAPCHAIN_IMAGES is greater than maximum available swapchain images");
            return -1;
        }
        
        gpu->sc.img_cnt = cap.minImageCount < MIN_SWAPCHAIN_IMAGES ? MIN_SWAPCHAIN_IMAGES : cap.minImageCount;
        
        u32 fmt_cnt = 1;
        VkSurfaceFormatKHR fmt;
        vk_get_physical_device_surface_formats_khr(&fmt_cnt, &fmt);
        
        gpu->sc.info = (typeof(gpu->sc.info)) {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        gpu->sc.info.surface = gpu->surf;
        gpu->sc.info.preTransform = cap.currentTransform;
        gpu->sc.info.imageExtent = cap.currentExtent;
        gpu->sc.info.imageFormat = fmt.format;
        gpu->sc.info.imageColorSpace = fmt.colorSpace;
        gpu->sc.info.minImageCount = gpu->sc.img_cnt;
        gpu->sc.info.imageArrayLayers = 1;
        gpu->sc.info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        gpu->sc.info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        gpu->sc.info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        gpu->sc.info.clipped = VK_TRUE;
        gpu->sc.info.queueFamilyIndexCount = 1;
        gpu->sc.info.pQueueFamilyIndices = &gpu->qi.p;
        gpu->sc.info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        
        if (create_swapchain())
            return -1;
    }
    
    return 0;
}

#include "prg.h"
#include "win.h"
#include "vdt.h"
#include "gpu.h"

struct gpu gpu;

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
        
        vk_create_instance(&ci);
        
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
            gpu.phys_dev = pd[discrete];
            gpu.props = props[discrete];
        } else if (integrated != Max_u32) {
            gpu.phys_dev = pd[integrated];
            gpu.props = props[integrated];
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
                         gpu.props.deviceName);
            log_error_if(present == Max_u32,
                         "physical device %s does not support present operations",
                         gpu.props.deviceName);
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
        
        VkDeviceCreateInfo ci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        ci.queueCreateInfoCount = qc;
        ci.pQueueCreateInfos = qci;
        
        vk_create_device(&ci);
        
        gpu.queues.graphics = graphics;
        gpu.queues.present = present;
        gpu.queues.transfer = transfer;
#undef MAX_QUEUE_COUNT
    }
    
    create_vdt(); // initialize device api calls
    
    return 0;
}

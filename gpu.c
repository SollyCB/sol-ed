#include "prg.h"
#include "win.h"
#include "vdt.h"
#include "gpu.h"
#include "shader.h"

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

#if 0
// ** Fuck linking shader libs. And fuck their apis **
//
// OMFG I *hate* every library that is not stb! They all have such
// terrible interfaces which such cryptic header files! My only
// query is "how do I add a preprocessor definition?", no clue.
// So fuck it, I will just build the file outside this function
// with the defines embedded: no need to fight an api hiding a
// mountain of code when you can just handle a pointer to some
// text yourself in 20 lines (it is less than 20).
//
// glslc had a much for comprehensible header, but for some reason
// it does not want to link, and I am not about to waste my time
// searching for why that is the case.
internal struct string gpu_compile_spv(char *src, int stage)
{
    switch(stage) {
        case SHST_VRT: stage = GLSLANG_STAGE_VERTEX; break;
        case SHST_FRG: stage = GLSLANG_STAGE_FRAGMENT; break;
        default: invalid_default_case; break;
    }
    
    glslang_input_t in = {
        .language = GLSLANG_SOURCE_GLSL,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = GLSLANG_TARGET_VULKAN_1_3,
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = GLSLANG_TARGET_SPV_1_5,
        .code = src,
        .stage = stage,
        .default_version = 100,
        .default_profile = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = glslang_default_resource(),
    };
    
    struct string spv = {};
    char *cs = NULL;
    
    glslang_program_t* p = glslang_program_create();
    glslang_shader_t* s = glslang_shader_create(&in);
    
    if (!glslang_shader_preprocess(s, &in)) { cs = "preprocess"; goto out; }
    if (!glslang_shader_parse(s, &in)) { cs = "parse"; goto out; }
    
    glslang_program_add_shader(p, s);
    
    if (!glslang_program_link(p, GLSLANG_MSG_SPV_RULES_BIT|GLSLANG_MSG_VULKAN_RULES_BIT)) {
        cs = "link";
        goto out;
    }
    
    glslang_program_SPIRV_generate(p, stage);
    
    spv.size = glslang_program_SPIRV_get_size(p);
    spv.data = salloc(MAIN_THREAD, spv.size);
    
    glslang_program_SPIRV_get(p, (u32*)spv.data);
    
    out:
    log_error_if(cs, "Failed to %s shader - %s", cs, glslang_shader_get_info_log(s));
    
    char *m = (char*)glslang_program_SPIRV_get_messages(p);
    if (m) write_stdout(m, strlen(m));
    
    glslang_shader_delete(s);
    glslang_program_delete(p);
    
    return spv;
}
#endif

internal VkShaderModule gpu_create_shader(struct string spv)
{
    VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = spv.size;
    ci.pCode = (u32*)spv.data;
    
    VkShaderModule ret;
    if (vk_create_shader_module(&ci, &ret))
        return VK_NULL_HANDLE;
    
    return ret;
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
    
    gpu_compile_shaders();
    
    return 0;
}

def_gpu_compile_shaders(gpu_compile_shaders)
{
    struct string src;
    src.size = read_file(SH_SRC_URI, NULL, 0);
    src.data = salloc(MT, src.size);
    read_file(SH_SRC_URI, src.data, src.size);
    
    // You have to look at the structure of shader.h to understand what is happening here.
    // It is basically chopping the file up into vertex and fragment source code segments
    // based on the position of some marker defines in the file.
    u32 ofs = strfind(STR("GL_core_profile"), src);
    src.size -= ofs;
    src.data += ofs;
    
    u32 v_ofs = strfind(STR(SH_VERT_BEGIN_STR), src) + (u32)strlen(SH_VERT_BEGIN_STR) + 1; // +1 for newline
    u32 f_ofs = strfind(STR(SH_FRAG_BEGIN_STR), src) + (u32)strlen(SH_FRAG_BEGIN_STR) + 1;
    u32 v_sz = strfind(STR(SH_VERT_END_STR), src) - v_ofs;
    u32 f_sz = strfind(STR(SH_FRAG_END_STR), src) - f_ofs;
    
    src.data[v_ofs + v_sz] = 0;
    src.data[f_ofs + f_sz] = 0;
    
    write_file(SH_VERT_SRC_URI, src.data + v_ofs, v_sz);
    write_file(SH_FRAG_SRC_URI, src.data + f_ofs, f_sz);
    
    struct os_process vp = {.p = INVALID_HANDLE_VALUE};
    struct os_process fp = {.p = INVALID_HANDLE_VALUE};
    
    char *va[] = {SH_CL_URI, SH_VERT_SRC_URI, "-Werror -std=450 -o", SH_VERT_OUT_URI};
    char *fa[] = {SH_CL_URI, SH_FRAG_SRC_URI, "-Werror -std=450 -o", SH_FRAG_OUT_URI};
    
    char vcmd_buf[256];
    char fcmd_buf[256];
    struct string vcmd = flatten_pchar_array(va, (u32)cl_array_size(va), vcmd_buf, (u32)sizeof(vcmd_buf), ' ');
    struct string fcmd = flatten_pchar_array(fa, (u32)cl_array_size(fa), fcmd_buf, (u32)sizeof(vcmd_buf), ' ');
    
    int res = 0;
    
    if (os_create_process(vcmd.data, &vp)) {
        log_error("Failed to create shader compiler (vertex)");
        res = -1;
        goto out;
    }
    
    if (os_create_process(fcmd.data, &fp)) {
        log_error("Failed to create shader compiler (fragment)");
        res = -1;
        goto out;
    }
    
    int vr = os_await_process(&vp);
    int fr = os_await_process(&fp);
    
    if (vr || fr) {
        if (vr) {
            println("\nVertex shader source:");
            write_stdout(src.data + v_ofs, v_sz);
        }
        if (fr) {
            println("\nFragment shader source:");
            write_stdout(src.data + f_ofs, f_sz);
        }
        log_error_if(vr, "Vertex shader compiler return non-zero error code (%i)", (s64)vr);
        log_error_if(fr, "Fragment shader compiler return non-zero error code (%i)", (s64)fr);
        res = -1;
        goto out;
    }
    
    struct string vspv,fspv;
    
    vspv.size = read_file(SH_VERT_OUT_URI, NULL, 0);
    vspv.data = salloc(MT, vspv.size);
    read_file(SH_VERT_OUT_URI, vspv.data, vspv.size);
    
    fspv.size = read_file(SH_FRAG_OUT_URI, NULL, 0);
    fspv.data = salloc(MT, fspv.size);
    read_file(SH_FRAG_OUT_URI, fspv.data, fspv.size);
    
    VkShaderModule vmod = gpu_create_shader(vspv);
    VkShaderModule fmod = gpu_create_shader(fspv);
    
    if (!vmod || !fmod) {
        log_error_if(!vmod, "Failed to create vertex shader module");
        log_error_if(!fmod, "Failed to create fragment shader module");
        if (vmod) vk_destroy_shader_module(vmod);
        if (fmod) vk_destroy_shader_module(fmod);
        return -1;
    }
    
    gpu->sh.vert = vmod;
    gpu->sh.frag = fmod;
    
    out:
    if (vp.p != INVALID_HANDLE_VALUE) os_destroy_process(&vp);
    if (fp.p != INVALID_HANDLE_VALUE) os_destroy_process(&fp);
    return res;
}
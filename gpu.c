#include "external/stb_truetype.h"

#include "prg.h"
#include "win.h"
#include "vdt.h"
#include "gpu.h"
#include "shader.h"
#include "chars.h"

struct gpu *gpu;

struct cell {
    u16 pd[3]; // x and y are offsets from the top left corner of the screen in pixels, z is scale
    u8 fg[3];
    u8 bg[3];
};
#define CELL_PD_FMT VK_FORMAT_R16G16B16_UINT
#define CELL_FG_FMT VK_FORMAT_R8G8B8_UNORM
#define CELL_BG_FMT VK_FORMAT_R8G8B8_UNORM

// Character cells are defined by a 3 component u16 vector, where x and y are offsets in pixels
// of the top left corner of the cell from the top left corner of the screen, and z is a scale.
// The size of the screen is passed in as a push constant to allow conversion to screen coords.
#define CELL_BYTE_WIDTH (sizeof(u16) * 3)
#define CELL_COLOR

#define GPU_VB_SZ mb(64) /* Both arbitrary, need to be able to contain 2 frames of data */
#define GPU_TB_SZ mb(64)

internal u32 gpu_bi_to_mi[GPU_BUF_CNT] = {
    [GPU_BI_V] = GPU_MI_V,
    [GPU_BI_T] = GPU_MI_T,
};

internal char* gpu_mem_names[GPU_MEM_CNT] = {
    [GPU_MI_V] = "Vertex",
    [GPU_MI_T] = "Transfer",
    [GPU_MI_I] = "Image",
};

union gpu_memreq_info {
    VkBufferCreateInfo *buf;
    VkImageCreateInfo *img;
};

internal int gpu_memreq_helper(union gpu_memreq_info info, u32 i, VkMemoryRequirements *mr)
{
    switch(i) {
        case GPU_MI_I: {
            VkImage img;
            if (vk_create_img(info.img, &img))
                break;
            vk_get_img_memreq(img, mr);
            vk_destroy_img(img);
        } return 0;
        
        case GPU_MI_V:
        case GPU_MI_T: {
            VkBuffer buf;
            if (vk_create_buf(info.buf, &buf))
                break;
            vk_get_buf_memreq(buf, mr);
            vk_destroy_buf(buf);
        } return 0;
        
        default: invalid_default_case;
    }
    log_error("Failed to create dummy object for getting memory requirements (%s)", gpu_mem_names[i]);
    return -1;
}

internal u32 gpu_memtype_helper(u32 types, u32 req)
{
    // Ensure that a memory type requiring device features cannot be chosen.
    u32 mem_mask = Max_u32 << (ctz(VK_MEMORY_PROPERTY_PROTECTED_BIT) + 1);
    u64 max_heap = 0;
    
    u32 type = Max_u32;
    u32 cnt,tz;
    for_bits(tz, cnt, type_bits) {
        if(gpu->memprops.memoryTypes[tz].propertyFlags & mem_mask) {
            continue;
        } else if ((gpu->memprops.memoryTypes[tz].propertyFlags & req) == req &&
                   (gpu->memprops.memoryHeaps[gpu->memprops.memoryTypes[tz].heapIndex].size > max_heap))
        {
            type = tz;
            max_heap = gpu->memprops.memoryHeaps[gpu->memprops.memoryTypes[tz].heapIndex].size;
        }
    }
    
    return type;
}

internal int gpu_create_mem(void)
{
    local_persist VkImageCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8_UINT,
        .extent = {.width = 1,.height = 1,.depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
    };
    
    local_persist VkBufferCreateInfo bci[GPU_BUF_CNT] = {
        [GPU_BI_V] = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = 1,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        [GPU_BI_T] = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = 1,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        },
    };
    
    if (!(gpu->flags & GPU_MEM_INI)) {
        if (gpu->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            gpu->flags |= GPU_MEM_UNI;
        
        vk_get_phys_dev_memprops(gpu->phys_dev, &gpu->memprops);
        
        union gpu_memreq_info mr_infos[GPU_MEM_CNT] = {
            [GPU_MI_V] = {.buf = &bci[GPU_BI_V]},
            [GPU_MI_T] = {.buf = &bci[GPU_BI_T]},
            [GPU_MI_I] = {.img = &ici},
        };
        
        VkMemoryRequirements mr[GPU_MEM_CNT];
        for(u32 i=0; i < GPU_MEM_CNT; ++i) {
            if (gpu_memreq_helper(mr_infos[i], i, &mr[i])) {
                log_error("Failed to get memory requirements for obj %u (%s)", i, gpu_mem_names[i]);
                gpu->flags &= GPU_MEM_BITS;
                return -1;
            }
        }
        
        u32 req_type_bits[GPU_MEM_CNT] = {
            [GPU_MEM_I] = VK_MEMORY_PROPERTY_DEVICE_LOCAL,
            [GPU_MEM_V] = VK_MEMORY_PROPERTY_DEVICE_LOCAL,
            [GPU_MEM_T] = VK_MEMORY_PROPERTY_HOST_VISIBLE|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
        
        if (gpu->flags & GPU_MEM_UNI)
            req_type_bits[GPU_MEM_V] |= VK_MEMORY_PROPERTY_HOST_VISIBLE|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        // Only update state when nothing can fail
        for(u32 i=0; i < GPU_MEM_CNT; ++i)
            gpu->mem[i].type = gpu_memtype_helper(mr[i].memoryTypeBits, req_type_bits[i]);
        if (gpu->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            gpu->flags |= GPU_MEM_UNI;
        gpu->flags |= GPU_MEM_INI;
    }
    
    u32 img_sz = 0;
    u32 bm_tot = 0;
    u32 bm_sz[CHT_SZ];
    u8 *bm[CHT_SZ];
    { // Glyphs
        u64 sz = read_file(FONT_URI, NULL, 0);
        u8 *data = salloc(MT, sz);
        read_file(FONT_URI, data, sz);
        
        stbtt_fontinfo font;
        stbtt_InitFont(&font, data, stbtt_GetFontOffsetForIndex(data, 0));
        
        struct gpu_glyph g[CHT_SZ];
        VkMemoryRequirements mr[CHT_SZ];
        
        int max_w = 0;
        int max_h = 0;
        for(u32 i=0; i < cl_array_size(bm); ++i) {
            bm[i] = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, FONT_HEIGHT),
                                             cht[i], &g[i].w, &g[i].h, &g[i].x, &g[i].y);
            
            bm_sz[i] = g[i].w * g[i].h;
            bm_tot += align(bm_sz[i], gpu->props.limits.optimalBufferCopyOffsetAlignment);
            if (max_w < g[i].w) max_w = g[i].w;
            if (max_h < g[i].h) max_h = g[i].h;
            
            ici.extent = (VkExtent3D) {.width = w, .height = h, .depth = 1};
            
            if (vk_create_img(&ici, &g[i].img)) {
                if (bm[i])
                    stbtt_FreeBitmap(bm[i], NULL);
                while(--i < Max_u32) {
                    vk_destroy_img(g[i].img);
                    g[i].img = VK_NULL_HANDLE;
                    stbtt_FreeBitmap(bm[i], NULL);
                }
                return -1;
            }
            
            vk_get_img_memreq(g[i].img, &mr[i]);
            img_sz = (u32)(align(img_sz, mr[i].alignment) + mr[i].size);
        }
        
        VkDeviceMemory mem = VK_NULL_HANDLE;
        VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = img_sz;
        ai.memoryTypeIndex = gpu->mem[GPU_MI_I].type;
        
        if (vk_alloc_mem(&ai, &mem)) {
            log_error("Failed to allocate glyph memory, size %u", img_sz);
            ok = false;
            goto fail_dest_imgs;
        }
        
        u32 ofs = 0;
        for(u32 i=0; i < CHT_SZ; ++i) {
            ofs = (u32)align(ofs, mr[i].alignment);
            if (vk_bind_img_mem(g[i].img, mem, ofs)) {
                log_error("Failed to bind memory to image %u", i);
                goto fail_free_mem;
            }
            ofs += (u32)mr[i].size;
        }
        
        local_persist VkImageViewCreateInfo vci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8_UNORM,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,.levelCount = 1,.layerCount = 1,},
        };
        
        for(u32 i=0; i < CHT_SZ; ++i) {
            vci.image = g[i].img;
            if (vk_create_imgv(&vci, &g[i].view)) {
                log_error("Failed to create view for image %u", i);
                while(--i < Max_u32)
                    vk_destroy_imgv(g[i].view);
                goto fail_free_img_mem;
            }
        }
    }
    
    // @Todo Probably will need to add padding between characters, or maybe the font includes it?
    // @Todo There will be other things to draw besides characters (the window borders, cursor)
    u32 cell_cnt = (win->dim.w / max_w + 1) * (win->dim.h / max_h + 1);
    u32 vert_sz = sizeof(struct cell) * cell_cnt;
    
    if (vert_sz < bm_tot || (gpu->flags & GPU_MEM_UNI))
        bci[GPU_BI_T].size = bm_tot;
    else
        bci[GPU_BI_T].size = vert_sz;
    
    bci[GPU_BI_V].size = vert_sz;
    
    VkBuffer buf[GPU_BUF_CNT];
    for(u32 i=0; i < GPU_BUF_CNT; ++i) {
        if (vk_create_buf(&bci[i], &buf[i])) {
            log_error("Failed to create buffer %u (%s)", gpu_names[i]));
            while(--i < Max_u32)
                vk_destroy_buf(buf[i]);
            goto fail_dest_views;
        }
    }
    
    VkMemoryAllocateInfo ai[GPU_BUF_CNT] = {
        [GPU_BI_V] = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .memoryTypeIndex = gpu->mem[GPU_MEM_V].type,
        },
        [GPU_BI_T] = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .memoryTypeIndex = gpu->mem[GPU_MEM_T].type,
        },
    };
    
    VkDeviceMemory buf_mem[GPU_BUF_CNT];
    VkMemoryRequirements mr[GPU_BUF_CNT];
    for(u32 i=0; i < GPU_BUF_CNT; ++i) {
        vk_get_buf_memreq(buf[i], &mr[i]);
        ai[i].allocationSize = mr[i].size;
        
        if (vk_alloc_mem(&ai[i], &buf_mem[i])) {
            log_error("Failed to allocate memory for buffer %u (%s)", i, gpu_mem_names[gpu_bi_to_mi[i]]);
            while(--i < Max_u32)
                vk_free_mem(buf_mem[i]);
            goto fail_dest_bufs;
        }
    }
    
    void **buf_map[GPU_BUF_CNT];
    for(u32 i=0; i < GPU_BUF_CNT; ++i) {
        if (vk_map_mem(buf_mem[i], 0, mr[i].size, &buf_map[i])) {
            log_error("Failed to map buffer memory %u (%s)", i, gpu_mem_names[gpu_bi_to_mi[i]]);
            goto fail_free_buf_mem;
        }
        if (vk_bind_buf_mem(buf[i], buf_mem[i], 0)) {
            log_error("Failed to bind memory to buffer %u (%s)", gpu_mem_names[gpu_bi_to_mi[i]]);
            goto fail_free_buf_mem;
        }
    }
    
    // @TODO CurrentTask: At this stage in the program, all memory objects have been
    // successfully created, so now we need to:
    //     - destroy the old objects
    //     - upload the glyph data to the gpu
    memcpy(gpu->glyphs, g, sizeof(g));
    
    vk_free_mem(gpu->mem[GPU_BI_T].handle);
    vk_destroy_buf(gpu->buf[GPU_BI_T].handle);
    for(u32 i=0; i < CHT_SZ; ++i)
        stbtt_FreeBitmap(bm[i], NULL);
    
    return 0;
    
    fail_free_buf_mem:
    for(u32 i=0; i < GPU_BUF_CNT; ++i)
        vk_free_mem(buf_mems[i]);
    
    fail_dest_bufs:
    for(u32 i=0; i < GPU_BUF_CNT; ++i)
        vk_destroy_buf(buf[i]);
    
    fail_dest_views:
    for(u32 i=0; i < CHT_SZ; ++i)
        vk_destroy_imgv(g[i].view);
    
    fail_free_img_mem:
    vk_free_mem(img_mem);
    
    fail_dest_imgs:
    for(u32 i=0; i < CHT_SZ; ++i) {
        vk_destroy_img(g[i].img);
        stbtt_FreeBitmap(bm[i], NULL);
    }
    
    return -1;
}

internal int gpu_create_sc(void)
{
    char msg[128];
    
    VkSwapchainCreateInfoKHR sc_info = gpu->sc.info;
    sc_info.imageExtent = (VkExtent2D) {.width = win->dim.w, .height = win->dim.h};
    
    // I am not actually sure what the appropriate action to take is if this fails.
    // Spec says that oldSwapchain is retired regardless of whether creating a new
    // swapchain succeeds or not, so I do not know what the correct state to return is.
    if (vk_create_sc_khr(&sc_info, &gpu->sc.handle)) {
        strcpy(CLSTR(msg), STR("failed to create swapchain object (TODO I cannot tell if I am handling this correctly, the spec is odd when creation fails but oldSwapchain was a valid handle...)"));
        goto fail_sc;
    }
    
    VkImage imgs[SC_MAX_IMGS] = {};
    if (vk_get_sc_imgs_khr(&gpu->sc.img_cnt, imgs)) {
        strcpy(CLSTR(msg), STR("failed to get images"));
        goto fail_imgs;
    }
    
    local_persist VkImageViewCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    
    VkImageView views[SC_MAX_IMGS] = {};
    u32 i;
    for(i=0; i < gpu->sc.img_cnt; ++i) {
        ci.image = imgs[i];
        ci.format = sc_info.imageFormat;
        if (vk_create_imgv(&ci, &views[i])) {
            dbg_strfmt(CLSTR(msg), "failed to create image view %u", i);
            goto fail_views;
        }
    }
    
    gpu->sc.info.oldSwapchain = gpu->sc.handle;
    gpu->sc.info.imageExtent = sc_info.imageExtent;
    
    for(i=0; i < gpu->sc.img_cnt; ++i) {
        if (gpu->sc.views[i])
            vk_destroy_imgv(gpu->sc.views[i]);
        gpu->sc.views[i] = views[i];
        gpu->sc.imgs[i] = imgs[i];
    }
    
    return 0;
    
    fail_views:
    while(--i < Max_u32)
        vk_destroy_imgv(views[i]);
    
    fail_imgs:
    // As stated above, I am really not sure what the correct route to take is with regard
    // to swapchain creation failure. The best I can come up with for now is destroy everything
    // and trigger a complete swapchain rebuild.
    vk_destroy_sc_khr(gpu->sc.handle);
    gpu->sc.info.oldSwapchain = NULL;
    gpu->sc.handle = NULL;
    
    fail_sc:
    log_error("Swapchain creation failed - %s", msg);
    return -1;
}

internal VkShaderModule gpu_create_shader(struct string spv)
{
    VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = spv.size;
    ci.pCode = (u32*)spv.data;
    
    VkShaderModule ret;
    if (vk_create_shmod(&ci, &ret))
        return VK_NULL_HANDLE;
    
    return ret;
}

internal int gpu_create_dsl(void)
{
    local_persist VkDescriptorSetLayoutBinding b = {
        .binding = SH_SI_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = CHT_SZ,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    local_persist VkDescriptorSetLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &b,
    };
    
    if (vk_create_dsl(&ci, &gpu->dsl))
        return -1;
    
    return 0;
}

internal int gpu_create_pll(void)
{
    VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &gpu->dsl,
    };
    
    if (vk_create_pll(&ci, &gpu->pll))
        return -1;
    
    return 0;
}

internal int gpu_create_ds(void)
{
    if (gpu->dp == VK_NULL_HANDLE) {
        local_persist VkDescriptorPoolSize sz = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = CHT_SZ,
        };
        
        local_persist VkDescriptorPoolCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &sz,
        };
        if (vk_create_dp(&ci, &gpu->dp))
            return -1;
    }
    
    VkDescriptorSetAllocateInfo ai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = gpu->dp;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &gpu->dsl;
    
    if (vk_alloc_ds(&ai, gpu->ds))
        return -1;
    
    return 0;
}

internal int gpu_create_rp(void)
{
    local_persist VkAttachmentDescription a = {
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    
    local_persist VkAttachmentReference ca = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    
    local_persist VkSubpassDescription s = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &ca,
    };
    
    local_persist VkSubpassDependency d = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    };
    
    a.format = gpu->sc.info.imageFormat;
    
    local_persist VkRenderPassCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &a,
        .subpassCount = 1,
        .pSubpasses = &s,
        .dependencyCount = 1,
        .pDependencies = &d,
    };
    
    VkRenderPass rp = VK_NULL_HANDLE;
    if (vk_create_rp(&ci, &rp))
        return -1;
    
    if (gpu->rp)
        vk_destroy_rp(gpu->rp);
    gpu->rp = rp;
    
    return 0;
}

internal int gpu_create_fb(void)
{
    local_persist VkFramebufferCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .attachmentCount = 1,
        .layers = 1,
    };
    
    ci.renderPass = gpu->rp,
    ci.width = gpu->sc.info.imageExtent.width;
    ci.height = gpu->sc.info.imageExtent.height;
    
    VkFramebuffer fb[SC_MAX_IMGS];
    for(u32 i=0; i < gpu->sc.img_cnt; ++i) {
        ci.pAttachments = &gpu->sc.views[i];
        if (vk_create_fb(&ci, &fb[i])) {
            while(--i < Max_u32) vk_destroy_fb(fb[i]);
            return -1;
        }
    }
    
    for(u32 i=0; i < gpu->sc.img_cnt; ++i) {
        if (gpu->fb[i])
            vk_destroy_fb(gpu->fb[i]);
        gpu->fb[i] = fb[i];
    }
    
    return 0;
}

internal int gpu_create_pl(void)
{
    local_persist VkPipelineShaderStageCreateInfo sh[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .pName = SH_ENTRY_POINT,
        },{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pName = SH_ENTRY_POINT,
        }
    };
    
    local_persist VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };
    
    local_persist VkPipelineInputAssemblyStateCreateInfo ia = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    
    local_persist VkViewport view = {.x = 0, .y = 0, .minDepth = 0, .maxDepth = 1};
    local_persist VkRect2D scissor = {.offset = {.x = 0, .y = 0}};
    
    local_persist VkPipelineViewportStateCreateInfo vp = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
        .pViewports = &view,
        .pScissors = &scissor,
    };
    
    local_persist VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f,
    };
    
    local_persist VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    
    local_persist VkPipelineDepthStencilStateCreateInfo ds = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };
    
    local_persist VkPipelineColorBlendAttachmentState cba = {};
    
    local_persist VkPipelineColorBlendStateCreateInfo cb = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &cba,
    };
    
    local_persist VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    
    local_persist VkPipelineDynamicStateCreateInfo dy = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = cl_array_size(dyn_states),
        .pDynamicStates = dyn_states,
    };
    
    sh[0].module = gpu->sh.vert;
    sh[1].module = gpu->sh.frag;
    
    view.width = (f32)win->dim.w;
    view.height = (f32)win->dim.h;
    scissor.extent.width = win->dim.w;
    scissor.extent.height = win->dim.h;
    
    local_persist VkGraphicsPipelineCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = cl_array_size(sh),
        .pStages = sh,
        .pVertexInputState = &vi,
        .pInputAssemblyState = &ia,
        .pViewportState = &vp,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pDepthStencilState = &ds,
        .pColorBlendState = &cb,
        .pDynamicState = &dy,
    };
    
    ci.layout = gpu->pll;
    ci.renderPass = gpu->rp;
    
    VkPipeline pl = VK_NULL_HANDLE;
    if (vk_create_gpl(1, &ci, &pl))
        return -1;
    
    if (gpu->pl)
        vk_destroy_pl(gpu->pl);
    gpu->pl = pl;
    
    return 0;
}

/*******************************************************************/
// Header functions

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
        
        if (vk_create_inst(&ci))
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
        vk_enum_phys_devs(&cnt, NULL); assert(cnt <= MAX_DEVICE_COUNT);
        vk_enum_phys_devs(&cnt, pd);
        
        VkPhysicalDeviceProperties props[MAX_DEVICE_COUNT];
        u32 discrete   = Max_u32;
        u32 integrated = Max_u32;
        
        for(u32 i=0; i < cnt; ++i) {
            vk_get_phys_dev_props(pd[i], props);
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
        vk_get_phys_devq_fam_props(&cnt, NULL); assert(cnt <= MAX_QUEUE_COUNT);
        vk_get_phys_devq_fam_props(&cnt, fp);
        
        u32 graphics = Max_u32;
        u32 present  = Max_u32;
        u32 transfer = Max_u32;
        for(u32 i=0; i < cnt; ++i) {
            b32 surf;
            vk_get_phys_dev_surf_support_khr(i, &surf);
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
        
        if (vk_create_dev(&ci))
            return -1;
#undef MAX_QUEUE_COUNT
        
        gpu->qi.g = graphics;
        gpu->qi.p = present;
        gpu->qi.t = transfer;
    }
    
    create_vdt(); // initialize device api calls
    
    {
        vk_get_devq(gpu->qi.g, &gpu->qh.g);
        
        if (gpu->qi.p != gpu->qi.g)
            vk_get_devq(gpu->qi.p, &gpu->qh.p);
        else
            gpu->qi.p = gpu->qi.g;
        
        if (gpu->qi.t != gpu->qi.g)
            vk_get_devq(gpu->qi.t, &gpu->qh.t);
        else
            gpu->qi.t = gpu->qi.g;
    }
    
    {
        VkSurfaceCapabilitiesKHR cap;
        vk_get_phys_dev_surf_cap_khr(&cap);
        
        log_error_if(cap.currentExtent.width != win->dim.w || cap.currentExtent.height != win->dim.h,
                     "Window dimensions do not match those reported by surface capabilities");
        
        if (cap.minImageCount > SC_MAX_IMGS) {
            log_error("SC_MAX_IMGS is smaller that minimum required swapchain images");
            return -1;
        }
        if (cap.maxImageCount < SC_MIN_IMGS && cap.maxImageCount != 0) {
            log_error("SC_MIN_IMGS is greater than maximum available swapchain images");
            return -1;
        }
        
        gpu->sc.img_cnt = cap.minImageCount < SC_MIN_IMGS ? SC_MIN_IMGS : cap.minImageCount;
        
        u32 fmt_cnt = 1;
        VkSurfaceFormatKHR fmt;
        vk_get_phys_dev_surf_fmts_khr(&fmt_cnt, &fmt);
        
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
        
        if (gpu_create_sc())
            return -1;
    }
    
    gpu_create_mem();
    gpu_create_sh();
    gpu_create_dsl();
    gpu_create_pll();
    gpu_create_ds();
    gpu_create_rp();
    gpu_create_fb();
    gpu_create_pl();
    
    return 0;
}

def_gpu_create_sh(gpu_create_sh)
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
        if (vmod)
            vk_destroy_shmod(vmod);
        if (fmod)
            vk_destroy_shmod(fmod);
        return -1;
    }
    
    gpu->sh.vert = vmod;
    gpu->sh.frag = fmod;
    
    out:
    if (vp.p != INVALID_HANDLE_VALUE)
        os_destroy_process(&vp);
    if (fp.p != INVALID_HANDLE_VALUE)
        os_destroy_process(&fp);
    return res;
}

def_gpu_handle_win_resize(gpu_handle_win_resize)
{
    println("GPU handling resize");
    if (gpu_create_sc()) {
        log_error("Failed to retire old swapchain, retrying from scratch...");
        if (gpu_create_sc()) {
            log_error("Failed to create fresh swapchain");
            return -1;
        }
    }
    if (gpu_create_fb()) {
        log_error("Failed to create framebuffer");
        return -1;
    }
    return 0;
}
#include "external/stb_truetype.h"

#include "prg.h"
#include "win.h"
#include "gpu.h"
#include "shader.h"
#include "chars.h"

struct gpu *gpu;

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
        
        case GPU_MI_G:
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

internal u32 gpu_memtype_helper(u32 type_bits, u32 req)
{
    // Ensure that a memory type requiring device features cannot be chosen.
    u32 mem_mask = Max_u32 << (ctz(VK_MEMORY_PROPERTY_PROTECTED_BIT) + 1);
    u64 max_heap = 0;
    
    u32 ret = Max_u32;
    u32 cnt,tz;
    for_bits(tz, cnt, type_bits) {
        if(gpu->memprops.memoryTypes[tz].propertyFlags & mem_mask) {
            continue;
        } else if ((gpu->memprops.memoryTypes[tz].propertyFlags & req) == req &&
                   (gpu->memprops.memoryHeaps[gpu->memprops.memoryTypes[tz].heapIndex].size > max_heap))
        {
            ret = tz;
            max_heap = gpu->memprops.memoryHeaps[gpu->memprops.memoryTypes[tz].heapIndex].size;
        }
    }
    
    return ret;
}

internal u32 gpu_alloc_cmds(u32 ci, u32 cnt)
{
    if (cnt == 0) return Max_u32;
    
    if (GPU_MAX_CMDS < gpu_cmd(ci).buf_cnt + cnt) {
        log_error("Command buffer allocation overflow for queue %u (%s)", ci, gpu_cmd_name(ci));
        return Max_u32;
    }
    
    if (vk_alloc_cmds(ci, cnt))
        return Max_u32;
    
    gpu_cmd(ci).buf_cnt += cnt;
    return gpu_cmd(ci).buf_cnt - cnt;
}

internal void gpu_dealloc_cmds(u32 ci)
{
    if (gpu_cmd(ci).buf_cnt == 0)
        return;
    vk_free_cmds(ci);
    gpu_cmd(ci).buf_cnt = 0;
}

internal int gpu_create_mem(void)
{
    local_persist VkImageCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = CELL_GL_FMT,
        .extent = {.width = 1,.height = 1,.depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
    };
    
    local_persist VkBufferCreateInfo bci[GPU_BUF_CNT] = {
        [GPU_BI_G] = {
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
            [GPU_MI_G] = {.buf = &bci[GPU_BI_G]},
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
            [GPU_MI_I] = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            [GPU_MI_G] = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            [GPU_MI_T] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
        
        if (gpu->flags & GPU_MEM_UNI)
            req_type_bits[GPU_MI_G] |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        if (gpu->q[GPU_QI_G].i != gpu->q[GPU_QI_T].i) {
            VkCommandPoolCreateInfo ci[GPU_CMDQ_CNT] = {
                [GPU_QI_G] = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                    .queueFamilyIndex = gpu->q[GPU_QI_G].i,
                },
                [GPU_QI_T] = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                    .queueFamilyIndex = gpu->q[GPU_QI_T].i,
                },
            };
            for(u32 i=0; i < GPU_CMDQ_CNT; ++i) {
                if (vk_create_cmdpool(&ci[i], &gpu_cmd(i).pool))
                    return -1;
            }
        } else {
            VkCommandPoolCreateInfo ci = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                .queueFamilyIndex = gpu->q[GPU_QI_G].i,
            };
            if (vk_create_cmdpool(&ci, &gpu_cmd(GPU_CI_G).pool))
                return -1;
        }
        
        // Only update state when nothing can fail
        for(u32 i=0; i < GPU_MEM_CNT; ++i)
            gpu->mem[i].type = gpu_memtype_helper(mr[i].memoryTypeBits, req_type_bits[i]);
        if (gpu->props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            gpu->flags |= GPU_MEM_UNI;
        gpu->flags |= GPU_MEM_INI;
    }
    
    if (gpu->sampler == VK_NULL_HANDLE) {
        VkSamplerCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .minLod = 0,
            .maxLod = VK_LOD_CLAMP_NONE,
            .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        };
        if (vk_create_sampler(&ci, &gpu->sampler))
            return -1;
    }
    
    VkCommandBuffer cmd[GPU_CMDQ_CNT];
    if (gpu->q[GPU_QI_G].i != gpu->q[GPU_QI_T].i) {
        for(u32 i=0; i < GPU_CMDQ_CNT; ++i) {
            vk_reset_cmdpool(gpu_cmd(i).pool, GPU_CMD_RE);
            gpu_dealloc_cmds(i);
        }
        for(u32 i=0; i < GPU_CMDQ_CNT; ++i) {
            u32 ci = gpu_alloc_cmds(i, 1); 
            if (ci == Max_u32) {
                log_error("Failed to allocate %u command buffer for uploading glyph (%s)", i, gpu_cmd_name(i));
                return -1;
            }
            cmd[i] = gpu_cmd(i).bufs[ci];
        }
    } else {
        vk_reset_cmdpool(gpu_cmd(GPU_CI_G).pool, GPU_CMD_RE);
        gpu_dealloc_cmds(GPU_CI_G);
        u32 ci = gpu_alloc_cmds(GPU_QI_G, 1);
        if (ci == Max_u32) {
            log_error("Failed to allocate command buffer for uploading glyph (%s)", gpu_cmd_name(GPU_CI_G));
            return -1;
        }
        cmd[GPU_CI_G] = gpu_cmd(GPU_CI_G).bufs[ci];
    }
    
    u32 max_w = 0;
    u32 max_h = 0;
    u32 img_sz = 0;
    u32 bm_tot = 0;
    u32 bm_sz[CHT_SZ]; // probably don't need size and dim, basically trading multiplies for stores and loads
    struct rect_u32 bm_dim[CHT_SZ];
    u8 *bm[CHT_SZ];
    VkDeviceMemory mem[GPU_MEM_CNT];
    struct gpu_glyph g[CHT_SZ];
    { // Glyphs
        u64 sz = read_file(FONT_URI, NULL, 0);
        u8 *data = salloc(MT, sz);
        read_file(FONT_URI, data, sz);
        
        stbtt_fontinfo font;
        stbtt_InitFont(&font, data, stbtt_GetFontOffsetForIndex(data, 0));
        
        VkMemoryRequirements mr[CHT_SZ];
        
        for(u32 i=0; i < cl_array_size(bm); ++i) {
            bm[i] = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, FONT_HEIGHT),
                                             cht[i], (int*)&bm_dim[i].w, (int*)&bm_dim[i].h, &g[i].x, &g[i].y);
            
            
            bm_sz[i] = bm_dim[i].w * bm_dim[i].h;
            bm_tot += (u32)align(bm_sz[i], gpu->props.limits.optimalBufferCopyOffsetAlignment);
            if (max_w < bm_dim[i].w) max_w = bm_dim[i].w;
            if (max_h < bm_dim[i].h) max_h = bm_dim[i].h;
            
            ici.extent = (VkExtent3D) {.width = bm_dim[i].w, .height = bm_dim[i].h, .depth = 1};
            
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
        
        VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = img_sz;
        ai.memoryTypeIndex = gpu->mem[GPU_MI_I].type;
        
        if (vk_alloc_mem(&ai, &mem[GPU_MI_I])) {
            log_error("Failed to allocate glyph memory, size %u", img_sz);
            goto fail_dest_imgs;
        }
        
        u32 ofs = 0;
        for(u32 i=0; i < CHT_SZ; ++i) {
            ofs = (u32)align(ofs, mr[i].alignment);
            if (vk_bind_img_mem(g[i].img, mem[GPU_MI_I], ofs)) {
                log_error("Failed to bind memory to image %u", i);
                goto fail_free_img_mem;
            }
            ofs += (u32)mr[i].size;
        }
        
        local_persist VkImageViewCreateInfo vci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = CELL_GL_FMT,
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
    u32 vert_sz = sizeof(struct gpu_cell_data) * cell_cnt;
    
    if (vert_sz < bm_tot || (gpu->flags & GPU_MEM_UNI))
        bci[GPU_BI_T].size = bm_tot;
    else
        bci[GPU_BI_T].size = vert_sz;
    
    bci[GPU_BI_G].size = vert_sz;
    
    VkBuffer buf[GPU_BUF_CNT];
    for(u32 i=0; i < GPU_BUF_CNT; ++i) {
        if (vk_create_buf(&bci[i], &buf[i])) {
            log_error("Failed to create buffer %u (%s)", i, gpu_mem_names[i]);
            while(--i < Max_u32)
                vk_destroy_buf(buf[i]);
            goto fail_dest_views;
        }
    }
    
    VkMemoryAllocateInfo ai[GPU_BUF_CNT] = {
        [GPU_BI_G] = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .memoryTypeIndex = gpu->mem[GPU_MI_G].type,
        },
        [GPU_BI_T] = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .memoryTypeIndex = gpu->mem[GPU_MI_T].type,
        },
    };
    
    VkMemoryRequirements buf_mr[GPU_BUF_CNT];
    for(u32 i=0; i < GPU_BUF_CNT; ++i) {
        vk_get_buf_memreq(buf[i], &buf_mr[i]);
        ai[i].allocationSize = buf_mr[i].size;
        
        if (vk_alloc_mem(&ai[i], &mem[gpu_bi_to_mi[i]])) {
            log_error("Failed to allocate memory for buffer %u (%s)", i, gpu_mem_names[gpu_bi_to_mi[i]]);
            while(--i < Max_u32)
                vk_free_mem(mem[gpu_bi_to_mi[i]]);
            goto fail_dest_bufs;
        }
    }
    
    void *buf_map[GPU_BUF_CNT];
    if (vk_map_mem(mem[GPU_MI_T], 0, buf_mr[GPU_BI_T].size, &buf_map[GPU_BI_T])) {
        log_error("Failed to map buffer memory %u (%s)", GPU_BI_T, gpu_mem_names[GPU_MI_T]);
        goto fail_free_buf_mem;
    }
    if (gpu->flags & GPU_MEM_UNI) {
        if (vk_map_mem(mem[GPU_MI_G], 0, buf_mr[GPU_BI_G].size, &buf_map[GPU_BI_G])) {
            log_error("Failed to map buffer memory %u (%s)", GPU_BI_G, gpu_mem_names[GPU_BI_G]);
            goto fail_free_buf_mem;
        }
    }
    
    for(u32 i=0; i < GPU_BUF_CNT; ++i) {
        if (vk_bind_buf_mem(buf[i], mem[gpu_bi_to_mi[i]], 0)) {
            log_error("Failed to bind memory to buffer %u (%s)", i, gpu_mem_names[gpu_bi_to_mi[i]]);
            goto fail_free_buf_mem;
        }
    }
    
    void *di = palloc(MT, gpu_dba_sz(cell_cnt));
    if (!di) {
        log_error("Failed to allocate memory for draw info array (%u bytes)", gpu_dba_sz(cell_cnt));
        goto fail_free_buf_mem;
    }
    
    // @TODO CurrentTask: At this stage in the program, all memory objects have been
    // successfully created, so now we need to:
    //     - destroy the old objects
    //     - upload the glyph data to the gpu
    
    if (gpu->db.di)
        pfree(MT, gpu->db.di, gpu_dba_sz(gpu->cell.cnt));
    gpu->db.di = di;
    gpu->db.used = 0;
    
    for(u32 i=0; i < GPU_MEM_CNT; ++i) {
        if (gpu->mem[i].handle)
            vk_free_mem(gpu->mem[i].handle);
        gpu->mem[i].handle = mem[i];
    }
    
    for(u32 i=0; i < CHT_SZ; ++i) {
        log_error_if((gpu->glyph[i].img && !gpu->glyph[i].view) ||
                     (!gpu->glyph[i].img && gpu->glyph[i].view),
                     "Glyph %u's image and view have different states (one is null, one is valid)");
        if (gpu->glyph[i].img)
            vk_destroy_img(gpu->glyph[i].img);
        if (gpu->glyph[i].view)
            vk_destroy_imgv(gpu->glyph[i].view);
    }
    memcpy(gpu->glyph, g, sizeof(g));
    gpu->cell.dim.w = max_w;
    gpu->cell.dim.h = max_h;
    gpu->cell.cnt = cell_cnt;
    
    for(u32 i=0; i < GPU_BUF_CNT; ++i) {
        if (gpu->buf[i].handle)
            vk_destroy_buf(gpu->buf[i].handle);
        gpu->buf[i].handle = buf[i];
        gpu->buf[i].size = bci[i].size;
        gpu->buf[i].used = 0;
        gpu->buf[i].data = NULL;
    }
    gpu->buf[GPU_BI_T].data = buf_map[GPU_BI_T];
    if (gpu->flags & GPU_MEM_UNI)
        gpu->buf[GPU_BI_G].data = buf_map[GPU_BI_G];
    
    if (gpu->q[GPU_QI_G].i != gpu->q[GPU_QI_T].i) {
        enum {UT,TG,STAGE_CNT};
        VkImageMemoryBarrier2 b[STAGE_CNT][CHT_SZ] = {
            [UT][0] = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = gpu->glyph[0].img,
                .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            },
            [TG][0] = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = gpu->q[GPU_QI_T].i,
                .dstQueueFamilyIndex = gpu->q[GPU_QI_G].i,
                .image = gpu->glyph[0].img,
                .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            },
        };
        for(u32 i=0; i < STAGE_CNT; ++i) {
            for(u32 j=1; j < CHT_SZ; ++j) {
                b[i][j] = b[i][0];
                b[i][j].image = gpu->glyph[j].img;
            }
        }
        
        VkDependencyInfo d[STAGE_CNT] = {
            [UT] = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = CHT_SZ,
                .pImageMemoryBarriers = b[UT],
            },
            [TG] = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = CHT_SZ,
                .pImageMemoryBarriers = b[TG],
            },
        };
        
        vk_begin_cmd(cmd[GPU_CI_T], GPU_CMD_OT);
        vk_begin_cmd(cmd[GPU_CI_G], GPU_CMD_OT);
        
        vk_cmd_pl_barr(cmd[GPU_CI_T], &d[UT]);
        
        u32 ofs = 0;
        for(u32 i=0; i < CHT_SZ; ++i) {
            memcpy((u8*)gpu->buf[GPU_BI_T].data + ofs, bm[i], bm_sz[i]);
            vk_cmd_copy_buf_to_img(cmd[GPU_CI_T], gpu->buf[GPU_BI_T].handle,
                                   gpu->glyph[i].img, ofs, bm_dim[i].w, bm_dim[i].h);
            ofs += (u32)align(bm_sz[i], gpu->props.limits.optimalBufferCopyOffsetAlignment);
        }
        
        vk_cmd_pl_barr(cmd[GPU_CI_T], &d[TG]);
        vk_cmd_pl_barr(cmd[GPU_CI_G], &d[TG]);
        
        vk_end_cmd(cmd[GPU_CI_T]);
        vk_end_cmd(cmd[GPU_CI_G]);
    } else {
        VkImageMemoryBarrier2 b[CHT_SZ] = {
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = gpu->q[GPU_QI_T].i,
                .dstQueueFamilyIndex = gpu->q[GPU_QI_G].i,
                .image = gpu->glyph[0].img,
                .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            }
        };
        for(u32 i=1; i < CHT_SZ; ++i) {
            b[i] = b[0];
            b[i].image = gpu->glyph[i].img;
        }
        
        VkDependencyInfo d = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = CHT_SZ,
            .pImageMemoryBarriers = b,
        };
        
        vk_begin_cmd(cmd[GPU_CI_G], GPU_CMD_OT);
        
        vk_cmd_pl_barr(cmd[GPU_CI_G], &d);
        
        u32 ofs = 0;
        for(u32 i=0; i < CHT_SZ; ++i) {
            memcpy((u8*)gpu->buf[GPU_BI_G].data + ofs, bm[i], bm_sz[i]);
            vk_cmd_copy_buf_to_img(cmd[GPU_BI_G], gpu->buf[GPU_BI_G].handle,
                                   gpu->glyph[i].img, ofs, bm_dim[i].w, bm_dim[i].h);
            ofs += (u32)align(bm_sz[i], gpu->props.limits.optimalBufferCopyOffsetAlignment);
        }
        
        vk_cmd_pl_barr(cmd[GPU_CI_G], &d);
        
        vk_end_cmd(cmd[GPU_CI_G]);
    }
    
    if (gpu->flags & GPU_MEM_UNI) {
        vk_free_mem(gpu->mem[GPU_BI_T].handle);
        vk_destroy_buf(gpu->buf[GPU_BI_T].handle);
        gpu->mem[GPU_BI_T].handle = VK_NULL_HANDLE;
        gpu->buf[GPU_BI_T].handle = VK_NULL_HANDLE;
    }
    for(u32 i=0; i < CHT_SZ; ++i)
        stbtt_FreeBitmap(bm[i], NULL);
    
    return 0;
    
    fail_free_buf_mem:
    for(u32 i=0; i < GPU_BUF_CNT; ++i)
        vk_free_mem(mem[gpu_bi_to_mi[i]]);
    
    fail_dest_bufs:
    for(u32 i=0; i < GPU_BUF_CNT; ++i)
        vk_destroy_buf(buf[i]);
    
    fail_dest_views:
    for(u32 i=0; i < CHT_SZ; ++i)
        vk_destroy_imgv(g[i].view);
    
    fail_free_img_mem:
    vk_free_mem(mem[GPU_MI_I]);
    
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
    vk_destroy_sc_khr(gpu->sc.info.oldSwapchain);
    
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
    VkPushConstantRange pcr = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 16, // 1 * vec4
    };
    VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &gpu->dsl,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pcr,
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
    
    if (vk_alloc_ds(&ai, &gpu->ds) == 0)
        return -1;
    
    VkDescriptorImageInfo ii[CHT_SZ] = {
        {
            .sampler = gpu->sampler,
            .imageView = gpu->glyph[0].view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }
    };
    for(u32 i=1; i < CHT_SZ; ++i) {
        ii[i] = ii[0];
        ii[i].imageView = gpu->glyph[i].view;
    }
    
    VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = gpu->ds;
    w.dstBinding = 0;
    w.dstArrayElement = 0;
    w.descriptorCount = CHT_SZ;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = ii;
    
    vk_update_ds(1, &w);
    
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
    
    local_persist VkVertexInputBindingDescription vi_b = {
        .binding = 0,
        .stride = sizeof(struct gpu_cell_data),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
    };
    
    local_persist VkVertexInputAttributeDescription vi_a[] = {
        [SH_PD_LOC] = {
            .location = SH_PD_LOC,
            .format = CELL_PD_FMT,
            .offset = offsetof(struct gpu_cell_data, pd),
        },
        [SH_FG_LOC] = {
            .location = SH_FG_LOC,
            .format = CELL_FG_FMT,
            .offset = offsetof(struct gpu_cell_data, fg),
        },
        [SH_BG_LOC] = {
            .location = SH_BG_LOC,
            .format = CELL_BG_FMT,
            .offset = offsetof(struct gpu_cell_data, bg),
        },
    };
    
    local_persist VkPipelineVertexInputStateCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .vertexAttributeDescriptionCount = cl_array_size(vi_a),
        .pVertexBindingDescriptions = &vi_b,
        .pVertexAttributeDescriptions = vi_a
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
        u32 ver;
        if (vkEnumerateInstanceVersion(&ver) == VK_ERROR_OUT_OF_HOST_MEMORY) {
            log_error("Failed to enumerate vulkan instance version (note that this can only happen due to the loader or enabled layers).");
            return -1;
        }
        if (VK_API_VERSION_MAJOR(ver) != 1 || VK_API_VERSION_MINOR(ver) < 3) {
            log_error("Vulkan instance version is not 1.3 or higher");
            return -1;
        }
        
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
        
        u32 qi[GPU_Q_CNT];
        memset(qi, 0xff, sizeof(qi));
        for(u32 i=0; i < cnt; ++i) {
            b32 surf;
            vk_get_phys_dev_surf_support_khr(i, &surf);
            if (surf && qi[GPU_QI_P] == Max_u32) {
                qi[GPU_QI_P] = i;
            }
            if ((fp[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                (fp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                qi[GPU_QI_G] == Max_u32)
            {
                qi[GPU_QI_G] = i;
            }
            if ((fp[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                !(fp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                qi[GPU_QI_T] == Max_u32)
            {
                qi[GPU_QI_T] = i;
            }
        }
        
        if (qi[GPU_QI_T] == Max_u32)
            qi[GPU_QI_T] = qi[GPU_QI_G];
        
        if (qi[GPU_QI_G] == Max_u32 || qi[GPU_QI_P] == Max_u32) {
            log_error_if(qi[GPU_QI_G] == Max_u32,
                         "physical device %s does not support graphics operations",
                         gpu->props.deviceName);
            log_error_if(qi[GPU_QI_P] == Max_u32,
                         "physical device %s does not support present operations",
                         gpu->props.deviceName);
            return -1;
        }
        
        float priority = 1;
        VkDeviceQueueCreateInfo qci[GPU_Q_CNT] = {
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = qi[0],
                .queueCount = 1,
                .pQueuePriorities = &priority,
            }
        };
        u32 qc = 1;
        {
            u32 set = 1;
            for(u32 i=1; i < GPU_Q_CNT; ++i) {
                if (!set_test(set, qi[i])) {
                    qci[qc] = qci[0];
                    qci[qc].queueFamilyIndex = qi[i];
                    qc += 1;
                    set = (u32)set_add(set, i);
                }
            }
        }
        
        char *ext_names[] = {
            "VK_KHR_swapchain",
        };
        
        VkPhysicalDeviceVulkan13Features feat13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = VK_TRUE,
        };
        
        VkDeviceCreateInfo ci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        ci.pNext = &feat13;
        ci.queueCreateInfoCount = qc;
        ci.pQueueCreateInfos = qci;
        ci.enabledExtensionCount = cl_array_size(ext_names);
        ci.ppEnabledExtensionNames = ext_names;
        
        if (vk_create_dev(&ci))
            return -1;
        
        gpu->q_cnt = qc;
        for(u32 i=0; i < GPU_Q_CNT; ++i)
            gpu->q[i].i = qi[i];
        
        create_vdt(); // initialize device api calls
        
        {
            vk_get_devq(gpu->q[0].i, &gpu->q[0].handle);
            u32 set = 1;
            u8 map[MAX_QUEUE_COUNT];
            for(u32 i=1; i < GPU_Q_CNT; ++i) {
                if (!set_test(set, gpu->q[i].i)) {
                    vk_get_devq(gpu->q[i].i, &gpu->q[i].handle);
                    map[gpu->q[i].i] = (u8)i;
                    set = (u32)set_add(set, gpu->q[i].i);
                } else {
                    gpu->q[i].handle = gpu->q[map[gpu->q[i].i]].handle;
                }
            }
        }
#undef MAX_QUEUE_COUNT
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
        gpu->sc.info.pQueueFamilyIndices = &gpu->q[GPU_QI_P].i;
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
    u32 ofs = strfind(STR("SH_BEGIN"), src) + (u32)strlen("SH_BEGIN") + 1;
    u32 sz = strfind(STR("SH_END"), src) + (u32)strlen("SH_END");
    src.data += ofs;
    src.size = sz - ofs;
    
    write_file(SH_SRC_OUT_URI, src.data, src.size);
    
    struct os_process vp = {.p = INVALID_HANDLE_VALUE};
    struct os_process fp = {.p = INVALID_HANDLE_VALUE};
    
    char *va[] = {SH_CL_URI, "-fshader-stage=vert", SH_SRC_OUT_URI, "-Werror -std=450 -o", SH_VERT_OUT_URI, "-DVERT"};
    char *fa[] = {SH_CL_URI, "-fshader-stage=frag", SH_SRC_OUT_URI, "-Werror -std=450 -o", SH_FRAG_OUT_URI};
    
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
        println("\nshader source dump:");
        write_stdout(src.data, src.size);
        println("\nend of shader source dump");
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
        log_error("Failed to create framebuffer on window resize");
        return -1;
    }
    if (gpu_create_mem()) {
        log_error("Failed to create memory objects on window resize");
        return -1;
    }
    return 0;
}

def_gpu_check_leaks(gpu_check_leaks)
{
    // @NOTE I am not necessarily trying to destroy everything,
    // just enough that the validation messages are parseable.
    
    for(u32 i=0; i < GPU_CMDQ_CNT; ++i) {
        gpu_dealloc_cmds(i);
        vk_destroy_cmdpool(gpu_cmd(i).pool);
    }
    
    for(u32 i=0; i < CHT_SZ; ++i) {
        vk_destroy_img(gpu->glyph[i].img);
        vk_destroy_imgv(gpu->glyph[i].view);
    }
    for(u32 i=0; i < GPU_BUF_CNT; ++i) {
        if (gpu->buf[i].handle)
            vk_destroy_buf(gpu->buf[i].handle);
    }
    for(u32 i=0; i < GPU_MEM_CNT; ++i)
        vk_free_mem(gpu->mem[i].handle);
    
    vk_destroy_shmod(gpu->sh.vert);
    vk_destroy_shmod(gpu->sh.frag);
    vk_destroy_pll(gpu->pll);
    vk_destroy_pl(gpu->pl);
    vk_destroy_rp(gpu->rp);
    for(u32 i=0; i < gpu->sc.img_cnt; ++i)
        vk_destroy_fb(gpu->fb[i]);
    
    vk_destroy_dsl(gpu->dsl);
    vk_destroy_dp(gpu->dp);
    
    for(u32 i=0; i < gpu->sc.img_cnt; ++i)
        vk_destroy_imgv(gpu->sc.views[i]);
    vk_destroy_sc_khr(gpu->sc.info.oldSwapchain);
    
    vkDestroyDevice(gpu->dev, NULL);
}
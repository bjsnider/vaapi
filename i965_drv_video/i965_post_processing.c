/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Xiang Haihao <haihao.xiang@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <va/va_backend.h>

#include "intel_batchbuffer.h"
#include "intel_driver.h"
#include "i965_defines.h"
#include "i965_structs.h"
#include "i965_drv_video.h"
#include "i965_post_processing.h"
#include "i965_render.h"

#define HAS_PP(ctx) (IS_IRONLAKE((ctx)->intel.device_id) ||     \
                     IS_GEN6((ctx)->intel.device_id) ||         \
                     IS_GEN7((ctx)->intel.device_id))

static const uint32_t pp_null_gen5[][4] = {
#include "shaders/post_processing/null.g4b.gen5"
};

static const uint32_t pp_nv12_load_save_gen5[][4] = {
#include "shaders/post_processing/nv12_load_save_nv12.g4b.gen5"
};

static const uint32_t pp_nv12_scaling_gen5[][4] = {
#include "shaders/post_processing/nv12_scaling_nv12.g4b.gen5"
};

static const uint32_t pp_nv12_avs_gen5[][4] = {
#include "shaders/post_processing/nv12_avs_nv12.g4b.gen5"
};

static const uint32_t pp_nv12_dndi_gen5[][4] = {
#include "shaders/post_processing/nv12_dndi_nv12.g4b.gen5"
};

static void pp_null_initialize(VADriverContextP ctx,
                               VASurfaceID in_surface_id, VASurfaceID out_surface_id,
                               const VARectangle *src_rect, const VARectangle *dst_rect);
static void pp_nv12_avs_initialize(VADriverContextP ctx,
                                   VASurfaceID in_surface_id, VASurfaceID out_surface_id,
                                   const VARectangle *src_rect, const VARectangle *dst_rect);
static void pp_nv12_scaling_initialize(VADriverContextP ctx,
                                       VASurfaceID in_surface_id, VASurfaceID out_surface_id,
                                       const VARectangle *src_rect, const VARectangle *dst_rect);
static void pp_nv12_load_save_initialize(VADriverContextP ctx,
                                         VASurfaceID in_surface_id, VASurfaceID out_surface_id,
                                         const VARectangle *src_rect, const VARectangle *dst_rect);
static void pp_nv12_dndi_initialize(VADriverContextP ctx,
                                    VASurfaceID in_surface_id, VASurfaceID out_surface_id,
                                    const VARectangle *src_rect, const VARectangle *dst_rect);

static struct pp_module pp_modules_gen5[] = {
    {
        {
            "NULL module (for testing)",
            PP_NULL,
            pp_null_gen5,
            sizeof(pp_null_gen5),
            NULL,
        },

        pp_null_initialize,
    },

    {
        {
            "NV12 Load & Save module",
            PP_NV12_LOAD_SAVE,
            pp_nv12_load_save_gen5,
            sizeof(pp_nv12_load_save_gen5),
            NULL,
        },

        pp_nv12_load_save_initialize,
    },

    {
        {
            "NV12 Scaling module",
            PP_NV12_SCALING,
            pp_nv12_scaling_gen5,
            sizeof(pp_nv12_scaling_gen5),
            NULL,
        },

        pp_nv12_scaling_initialize,
    },

    {
        {
            "NV12 AVS module",
            PP_NV12_AVS,
            pp_nv12_avs_gen5,
            sizeof(pp_nv12_avs_gen5),
            NULL,
        },

        pp_nv12_avs_initialize,
    },

    {
        {
            "NV12 DNDI module",
            PP_NV12_DNDI,
            pp_nv12_dndi_gen5,
            sizeof(pp_nv12_dndi_gen5),
            NULL,
        },

        pp_nv12_dndi_initialize,
    },
};

static const uint32_t pp_null_gen6[][4] = {
#include "shaders/post_processing/null.g6b"
};

static const uint32_t pp_nv12_load_save_gen6[][4] = {
#include "shaders/post_processing/nv12_load_save_nv12.g6b"
};

static const uint32_t pp_nv12_scaling_gen6[][4] = {
#include "shaders/post_processing/nv12_scaling_nv12.g6b"
};

static const uint32_t pp_nv12_avs_gen6[][4] = {
#include "shaders/post_processing/nv12_avs_nv12.g6b"
};

static const uint32_t pp_nv12_dndi_gen6[][4] = {
#include "shaders/post_processing/nv12_dndi_nv12.g6b"
};

static struct pp_module pp_modules_gen6[] = {
    {
        {
            "NULL module (for testing)",
            PP_NULL,
            pp_null_gen6,
            sizeof(pp_null_gen6),
            NULL,
        },

        pp_null_initialize,
    },

    {
        {
            "NV12 Load & Save module",
            PP_NV12_LOAD_SAVE,
            pp_nv12_load_save_gen6,
            sizeof(pp_nv12_load_save_gen6),
            NULL,
        },

        pp_nv12_load_save_initialize,
    },

    {
        {
            "NV12 Scaling module",
            PP_NV12_SCALING,
            pp_nv12_scaling_gen6,
            sizeof(pp_nv12_scaling_gen6),
            NULL,
        },

        pp_nv12_scaling_initialize,
    },

    {
        {
            "NV12 AVS module",
            PP_NV12_AVS,
            pp_nv12_avs_gen6,
            sizeof(pp_nv12_avs_gen6),
            NULL,
        },

        pp_nv12_avs_initialize,
    },

    {
        {
            "NV12 DNDI module",
            PP_NV12_DNDI,
            pp_nv12_dndi_gen6,
            sizeof(pp_nv12_dndi_gen6),
            NULL,
        },

        pp_nv12_dndi_initialize,
    },
};

#define pp_static_parameter     pp_context->pp_static_parameter
#define pp_inline_parameter     pp_context->pp_inline_parameter

static void
pp_set_surface_tiling(struct i965_surface_state *ss, unsigned int tiling)
{
    switch (tiling) {
    case I915_TILING_NONE:
        ss->ss3.tiled_surface = 0;
        ss->ss3.tile_walk = 0;
        break;
    case I915_TILING_X:
        ss->ss3.tiled_surface = 1;
        ss->ss3.tile_walk = I965_TILEWALK_XMAJOR;
        break;
    case I915_TILING_Y:
        ss->ss3.tiled_surface = 1;
        ss->ss3.tile_walk = I965_TILEWALK_YMAJOR;
        break;
    }
}

static void
pp_set_surface2_tiling(struct i965_surface_state2 *ss, unsigned int tiling)
{
    switch (tiling) {
    case I915_TILING_NONE:
        ss->ss2.tiled_surface = 0;
        ss->ss2.tile_walk = 0;
        break;
    case I915_TILING_X:
        ss->ss2.tiled_surface = 1;
        ss->ss2.tile_walk = I965_TILEWALK_XMAJOR;
        break;
    case I915_TILING_Y:
        ss->ss2.tiled_surface = 1;
        ss->ss2.tile_walk = I965_TILEWALK_YMAJOR;
        break;
    }
}

static void
ironlake_pp_surface_state(struct i965_post_processing_context *pp_context)
{

}

static void
ironlake_pp_interface_descriptor_table(struct i965_post_processing_context *pp_context)
{
    struct i965_interface_descriptor *desc;
    dri_bo *bo;
    int pp_index = pp_context->current_pp;

    bo = pp_context->idrt.bo;
    dri_bo_map(bo, 1);
    assert(bo->virtual);
    desc = bo->virtual;
    memset(desc, 0, sizeof(*desc));
    desc->desc0.grf_reg_blocks = 10;
    desc->desc0.kernel_start_pointer = pp_context->pp_modules[pp_index].kernel.bo->offset >> 6; /* reloc */
    desc->desc1.const_urb_entry_read_offset = 0;
    desc->desc1.const_urb_entry_read_len = 4; /* grf 1-4 */
    desc->desc2.sampler_state_pointer = pp_context->sampler_state_table.bo->offset >> 5;
    desc->desc2.sampler_count = 0;
    desc->desc3.binding_table_entry_count = 0;
    desc->desc3.binding_table_pointer = 
        pp_context->binding_table.bo->offset >> 5; /*reloc */

    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_INSTRUCTION, 0,
                      desc->desc0.grf_reg_blocks,
                      offsetof(struct i965_interface_descriptor, desc0),
                      pp_context->pp_modules[pp_index].kernel.bo);

    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_INSTRUCTION, 0,
                      desc->desc2.sampler_count << 2,
                      offsetof(struct i965_interface_descriptor, desc2),
                      pp_context->sampler_state_table.bo);

    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_INSTRUCTION, 0,
                      desc->desc3.binding_table_entry_count,
                      offsetof(struct i965_interface_descriptor, desc3),
                      pp_context->binding_table.bo);

    dri_bo_unmap(bo);
    pp_context->idrt.num_interface_descriptors++;
}

static void
ironlake_pp_binding_table(struct i965_post_processing_context *pp_context)
{
    unsigned int *binding_table;
    dri_bo *bo = pp_context->binding_table.bo;
    int i;

    dri_bo_map(bo, 1);
    assert(bo->virtual);
    binding_table = bo->virtual;
    memset(binding_table, 0, bo->size);

    for (i = 0; i < MAX_PP_SURFACES; i++) {
        if (pp_context->surfaces[i].ss_bo) {
            assert(pp_context->surfaces[i].s_bo);

            binding_table[i] = pp_context->surfaces[i].ss_bo->offset;
            dri_bo_emit_reloc(bo,
                              I915_GEM_DOMAIN_INSTRUCTION, 0,
                              0,
                              i * sizeof(*binding_table),
                              pp_context->surfaces[i].ss_bo);
        }
    
    }

    dri_bo_unmap(bo);
}

static void
ironlake_pp_vfe_state(struct i965_post_processing_context *pp_context)
{
    struct i965_vfe_state *vfe_state;
    dri_bo *bo;

    bo = pp_context->vfe_state.bo;
    dri_bo_map(bo, 1);
    assert(bo->virtual);
    vfe_state = bo->virtual;
    memset(vfe_state, 0, sizeof(*vfe_state));
    vfe_state->vfe1.max_threads = pp_context->urb.num_vfe_entries - 1;
    vfe_state->vfe1.urb_entry_alloc_size = pp_context->urb.size_vfe_entry - 1;
    vfe_state->vfe1.num_urb_entries = pp_context->urb.num_vfe_entries;
    vfe_state->vfe1.vfe_mode = VFE_GENERIC_MODE;
    vfe_state->vfe1.children_present = 0;
    vfe_state->vfe2.interface_descriptor_base = 
        pp_context->idrt.bo->offset >> 4; /* reloc */
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_INSTRUCTION, 0,
                      0,
                      offsetof(struct i965_vfe_state, vfe2),
                      pp_context->idrt.bo);
    dri_bo_unmap(bo);
}

static void
ironlake_pp_upload_constants(struct i965_post_processing_context *pp_context)
{
    unsigned char *constant_buffer;

    assert(sizeof(pp_static_parameter) == 128);
    dri_bo_map(pp_context->curbe.bo, 1);
    assert(pp_context->curbe.bo->virtual);
    constant_buffer = pp_context->curbe.bo->virtual;
    memcpy(constant_buffer, &pp_static_parameter, sizeof(pp_static_parameter));
    dri_bo_unmap(pp_context->curbe.bo);
}

static void
ironlake_pp_states_setup(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;

    ironlake_pp_surface_state(pp_context);
    ironlake_pp_binding_table(pp_context);
    ironlake_pp_interface_descriptor_table(pp_context);
    ironlake_pp_vfe_state(pp_context);
    ironlake_pp_upload_constants(pp_context);
}

static void
ironlake_pp_pipeline_select(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;

    BEGIN_BATCH(batch, 1);
    OUT_BATCH(batch, CMD_PIPELINE_SELECT | PIPELINE_SELECT_MEDIA);
    ADVANCE_BATCH(batch);
}

static void
ironlake_pp_urb_layout(VADriverContextP ctx, struct i965_post_processing_context *pp_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;
    unsigned int vfe_fence, cs_fence;

    vfe_fence = pp_context->urb.cs_start;
    cs_fence = pp_context->urb.size;

    BEGIN_BATCH(batch, 3);
    OUT_BATCH(batch, CMD_URB_FENCE | UF0_VFE_REALLOC | UF0_CS_REALLOC | 1);
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, 
              (vfe_fence << UF2_VFE_FENCE_SHIFT) |      /* VFE_SIZE */
              (cs_fence << UF2_CS_FENCE_SHIFT));        /* CS_SIZE */
    ADVANCE_BATCH(batch);
}

static void
ironlake_pp_state_base_address(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;

    BEGIN_BATCH(batch, 8);
    OUT_BATCH(batch, CMD_STATE_BASE_ADDRESS | 6);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    ADVANCE_BATCH(batch);
}

static void
ironlake_pp_state_pointers(VADriverContextP ctx, struct i965_post_processing_context *pp_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;

    BEGIN_BATCH(batch, 3);
    OUT_BATCH(batch, CMD_MEDIA_STATE_POINTERS | 1);
    OUT_BATCH(batch, 0);
    OUT_RELOC(batch, pp_context->vfe_state.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, 0);
    ADVANCE_BATCH(batch);
}

static void 
ironlake_pp_cs_urb_layout(VADriverContextP ctx, struct i965_post_processing_context *pp_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;

    BEGIN_BATCH(batch, 2);
    OUT_BATCH(batch, CMD_CS_URB_STATE | 0);
    OUT_BATCH(batch,
              ((pp_context->urb.size_cs_entry - 1) << 4) |     /* URB Entry Allocation Size */
              (pp_context->urb.num_cs_entries << 0));          /* Number of URB Entries */
    ADVANCE_BATCH(batch);
}

static void
ironlake_pp_constant_buffer(VADriverContextP ctx, struct i965_post_processing_context *pp_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;

    BEGIN_BATCH(batch, 2);
    OUT_BATCH(batch, CMD_CONSTANT_BUFFER | (1 << 8) | (2 - 2));
    OUT_RELOC(batch, pp_context->curbe.bo,
              I915_GEM_DOMAIN_INSTRUCTION, 0,
              pp_context->urb.size_cs_entry - 1);
    ADVANCE_BATCH(batch);    
}

static void
ironlake_pp_object_walker(VADriverContextP ctx, struct i965_post_processing_context *pp_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;
    int x, x_steps, y, y_steps;

    x_steps = pp_context->pp_x_steps(&pp_context->private_context);
    y_steps = pp_context->pp_y_steps(&pp_context->private_context);

    for (y = 0; y < y_steps; y++) {
        for (x = 0; x < x_steps; x++) {
            if (!pp_context->pp_set_block_parameter(pp_context, x, y)) {
                BEGIN_BATCH(batch, 20);
                OUT_BATCH(batch, CMD_MEDIA_OBJECT | 18);
                OUT_BATCH(batch, 0);
                OUT_BATCH(batch, 0); /* no indirect data */
                OUT_BATCH(batch, 0);

                /* inline data grf 5-6 */
                assert(sizeof(pp_inline_parameter) == 64);
                intel_batchbuffer_data(batch, &pp_inline_parameter, sizeof(pp_inline_parameter));

                ADVANCE_BATCH(batch);
            }
        }
    }
}

static void
ironlake_pp_pipeline_setup(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;
    struct i965_post_processing_context *pp_context = i965->pp_context;

    intel_batchbuffer_start_atomic(batch, 0x1000);
    intel_batchbuffer_emit_mi_flush(batch);
    ironlake_pp_pipeline_select(ctx);
    ironlake_pp_state_base_address(ctx);
    ironlake_pp_state_pointers(ctx, pp_context);
    ironlake_pp_urb_layout(ctx, pp_context);
    ironlake_pp_cs_urb_layout(ctx, pp_context);
    ironlake_pp_constant_buffer(ctx, pp_context);
    ironlake_pp_object_walker(ctx, pp_context);
    intel_batchbuffer_end_atomic(batch);
}

static int
pp_null_x_steps(void *private_context)
{
    return 1;
}

static int
pp_null_y_steps(void *private_context)
{
    return 1;
}

static int
pp_null_set_block_parameter(struct i965_post_processing_context *pp_context, int x, int y)
{
    return 0;
}

static void
pp_null_initialize(VADriverContextP ctx, 
                   VASurfaceID in_surface_id, VASurfaceID out_surface_id,
                   const VARectangle *src_rect, const VARectangle *dst_rect)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;

    /* private function & data */
    pp_context->pp_x_steps = pp_null_x_steps;
    pp_context->pp_y_steps = pp_null_y_steps;
    pp_context->pp_set_block_parameter = pp_null_set_block_parameter;
}

static int
pp_load_save_x_steps(void *private_context)
{
    return 1;
}

static int
pp_load_save_y_steps(void *private_context)
{
    struct pp_load_save_context *pp_load_save_context = private_context;

    return pp_load_save_context->dest_h / 8;
}

static int
pp_load_save_set_block_parameter(struct i965_post_processing_context *pp_context, int x, int y)
{
    pp_inline_parameter.grf5.block_vertical_mask = 0xff;
    pp_inline_parameter.grf5.block_horizontal_mask = 0xffff;
    pp_inline_parameter.grf5.destination_block_horizontal_origin = x * 16;
    pp_inline_parameter.grf5.destination_block_vertical_origin = y * 8;

    return 0;
}

static void
pp_nv12_load_save_initialize(VADriverContextP ctx,
                             VASurfaceID in_surface_id, VASurfaceID out_surface_id,
                             const VARectangle *src_rect, const VARectangle *dst_rect)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;
    struct pp_load_save_context *pp_load_save_context = (struct pp_load_save_context *)&pp_context->private_context;
    struct object_surface *obj_surface;
    struct i965_surface_state *ss;
    dri_bo *bo;
    int index, w, h;
    int orig_w, orig_h;
    unsigned int tiling, swizzle;

    /* source surface */
    obj_surface = SURFACE(in_surface_id);
    orig_w = obj_surface->orig_width;
    orig_h = obj_surface->orig_height;
    w = obj_surface->width;
    h = obj_surface->height;
    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

    /* source Y surface index 1 */
    index = 1;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset;
    ss->ss2.width = orig_w / 4 - 1;
    ss->ss2.height = orig_h - 1;
    ss->ss3.pitch = w - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      0,
                      0,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* source UV surface index 2 */
    index = 2;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8G8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset + w * h;
    ss->ss2.width = orig_w / 4 - 1;
    ss->ss2.height = orig_h / 2 - 1;
    ss->ss3.pitch = w - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      0,
                      w * h,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* destination surface */
    obj_surface = SURFACE(out_surface_id);
    orig_w = obj_surface->orig_width;
    orig_h = obj_surface->orig_height;
    w = obj_surface->width;
    h = obj_surface->height;
    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

    /* destination Y surface index 7 */
    index = 7;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset;
    ss->ss2.width = orig_w / 4 - 1;
    ss->ss2.height = orig_h - 1;
    ss->ss3.pitch = w - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      I915_GEM_DOMAIN_RENDER,
                      0,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* destination UV surface index 8 */
    index = 8;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8G8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset + w * h;
    ss->ss2.width = orig_w / 4 - 1;
    ss->ss2.height = orig_h / 2 - 1;
    ss->ss3.pitch = w - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      I915_GEM_DOMAIN_RENDER,
                      w * h,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* private function & data */
    pp_context->pp_x_steps = pp_load_save_x_steps;
    pp_context->pp_y_steps = pp_load_save_y_steps;
    pp_context->pp_set_block_parameter = pp_load_save_set_block_parameter;
    pp_load_save_context->dest_h = h;
    pp_load_save_context->dest_w = w;

    pp_inline_parameter.grf5.block_count_x = w / 16;   /* 1 x N */
    pp_inline_parameter.grf5.number_blocks = w / 16;
}

static int
pp_scaling_x_steps(void *private_context)
{
    return 1;
}

static int
pp_scaling_y_steps(void *private_context)
{
    struct pp_scaling_context *pp_scaling_context = private_context;

    return pp_scaling_context->dest_h / 8;
}

static int
pp_scaling_set_block_parameter(struct i965_post_processing_context *pp_context, int x, int y)
{
    struct pp_scaling_context *pp_scaling_context = (struct pp_scaling_context *)&pp_context->private_context;
    float src_x_steping = pp_inline_parameter.grf5.normalized_video_x_scaling_step;
    float src_y_steping = pp_static_parameter.grf1.r1_6.normalized_video_y_scaling_step;

    pp_inline_parameter.grf5.r5_1.source_surface_block_normalized_horizontal_origin = src_x_steping * x * 16 + pp_scaling_context->src_normalized_x;
    pp_inline_parameter.grf5.source_surface_block_normalized_vertical_origin = src_y_steping * y * 8 + pp_scaling_context->src_normalized_y;
    pp_inline_parameter.grf5.destination_block_horizontal_origin = x * 16 + pp_scaling_context->dest_x;
    pp_inline_parameter.grf5.destination_block_vertical_origin = y * 8 + pp_scaling_context->dest_y;
    
    return 0;
}

static void
pp_nv12_scaling_initialize(VADriverContextP ctx,
                           VASurfaceID in_surface_id, VASurfaceID out_surface_id,
                           const VARectangle *src_rect, const VARectangle *dst_rect)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;
    struct pp_scaling_context *pp_scaling_context = (struct pp_scaling_context *)&pp_context->private_context;
    struct object_surface *obj_surface;
    struct i965_sampler_state *sampler_state;
    struct i965_surface_state *ss;
    dri_bo *bo;
    int index;
    int in_w, in_h, in_wpitch, in_hpitch;
    int out_w, out_h, out_wpitch, out_hpitch;
    unsigned int tiling, swizzle;

    /* source surface */
    obj_surface = SURFACE(in_surface_id);
    in_w = obj_surface->orig_width;
    in_h = obj_surface->orig_height;
    in_wpitch = obj_surface->width;
    in_hpitch = obj_surface->height;
    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

    /* source Y surface index 1 */
    index = 1;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset;
    ss->ss2.width = in_w - 1;
    ss->ss2.height = in_h - 1;
    ss->ss3.pitch = in_wpitch - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      0,
                      0,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* source UV surface index 2 */
    index = 2;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8G8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset + in_wpitch * in_hpitch;
    ss->ss2.width = in_w / 2 - 1;
    ss->ss2.height = in_h / 2 - 1;
    ss->ss3.pitch = in_wpitch - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      0,
                      in_wpitch * in_hpitch,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* destination surface */
    obj_surface = SURFACE(out_surface_id);
    out_w = obj_surface->orig_width;
    out_h = obj_surface->orig_height;
    out_wpitch = obj_surface->width;
    out_hpitch = obj_surface->height;
    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

    /* destination Y surface index 7 */
    index = 7;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset;
    ss->ss2.width = out_w / 4 - 1;
    ss->ss2.height = out_h - 1;
    ss->ss3.pitch = out_wpitch - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      I915_GEM_DOMAIN_RENDER,
                      0,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* destination UV surface index 8 */
    index = 8;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8G8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset + out_wpitch * out_hpitch;
    ss->ss2.width = out_w / 4 - 1;
    ss->ss2.height = out_h / 2 - 1;
    ss->ss3.pitch = out_wpitch - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      I915_GEM_DOMAIN_RENDER,
                      out_wpitch * out_hpitch,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* sampler state */
    dri_bo_map(pp_context->sampler_state_table.bo, True);
    assert(pp_context->sampler_state_table.bo->virtual);
    sampler_state = pp_context->sampler_state_table.bo->virtual;

    /* SIMD16 Y index 1 */
    sampler_state[1].ss0.min_filter = I965_MAPFILTER_LINEAR;
    sampler_state[1].ss0.mag_filter = I965_MAPFILTER_LINEAR;
    sampler_state[1].ss1.r_wrap_mode = I965_TEXCOORDMODE_CLAMP;
    sampler_state[1].ss1.s_wrap_mode = I965_TEXCOORDMODE_CLAMP;
    sampler_state[1].ss1.t_wrap_mode = I965_TEXCOORDMODE_CLAMP;

    /* SIMD16 UV index 2 */
    sampler_state[2].ss0.min_filter = I965_MAPFILTER_LINEAR;
    sampler_state[2].ss0.mag_filter = I965_MAPFILTER_LINEAR;
    sampler_state[2].ss1.r_wrap_mode = I965_TEXCOORDMODE_CLAMP;
    sampler_state[2].ss1.s_wrap_mode = I965_TEXCOORDMODE_CLAMP;
    sampler_state[2].ss1.t_wrap_mode = I965_TEXCOORDMODE_CLAMP;

    dri_bo_unmap(pp_context->sampler_state_table.bo);

    /* private function & data */
    pp_context->pp_x_steps = pp_scaling_x_steps;
    pp_context->pp_y_steps = pp_scaling_y_steps;
    pp_context->pp_set_block_parameter = pp_scaling_set_block_parameter;

    pp_scaling_context->dest_x = dst_rect->x;
    pp_scaling_context->dest_y = dst_rect->y;
    pp_scaling_context->dest_w = ALIGN(dst_rect->width, 16);
    pp_scaling_context->dest_h = ALIGN(dst_rect->height, 16);
    pp_scaling_context->src_normalized_x = (float)src_rect->x / in_w / out_w;
    pp_scaling_context->src_normalized_y = (float)src_rect->y / in_h / out_h;

    pp_static_parameter.grf1.r1_6.normalized_video_y_scaling_step = (float) src_rect->height / in_h / out_h;

    pp_inline_parameter.grf5.normalized_video_x_scaling_step = (float) src_rect->width / in_w / out_w;
    pp_inline_parameter.grf5.block_count_x = pp_scaling_context->dest_w / 16;   /* 1 x N */
    pp_inline_parameter.grf5.number_blocks = pp_scaling_context->dest_w / 16;
    pp_inline_parameter.grf5.block_vertical_mask = 0xff;
    pp_inline_parameter.grf5.block_horizontal_mask = 0xffff;
}

static int
pp_avs_x_steps(void *private_context)
{
    struct pp_avs_context *pp_avs_context = private_context;

    return pp_avs_context->dest_w / 16;
}

static int
pp_avs_y_steps(void *private_context)
{
    return 1;
}

static int
pp_avs_set_block_parameter(struct i965_post_processing_context *pp_context, int x, int y)
{
    struct pp_avs_context *pp_avs_context = (struct pp_avs_context *)&pp_context->private_context;
    float src_x_steping, src_y_steping, video_step_delta;
    int tmp_w = ALIGN(pp_avs_context->dest_h * pp_avs_context->src_w / pp_avs_context->src_h, 16);

    if (tmp_w >= pp_avs_context->dest_w) {
        pp_inline_parameter.grf5.normalized_video_x_scaling_step = 1.0 / tmp_w;
        pp_inline_parameter.grf6.video_step_delta = 0;
        
        if (x == 0) {
            pp_inline_parameter.grf5.r5_1.source_surface_block_normalized_horizontal_origin = (float)(tmp_w - pp_avs_context->dest_w) / tmp_w / 2 +
                pp_avs_context->src_normalized_x;
        } else {
            src_x_steping = pp_inline_parameter.grf5.normalized_video_x_scaling_step;
            video_step_delta = pp_inline_parameter.grf6.video_step_delta;
            pp_inline_parameter.grf5.r5_1.source_surface_block_normalized_horizontal_origin += src_x_steping * 16 +
                16 * 15 * video_step_delta / 2;
        }
    } else {
        int n0, n1, n2, nls_left, nls_right;
        int factor_a = 5, factor_b = 4;
        float f;

        n0 = (pp_avs_context->dest_w - tmp_w) / (16 * 2);
        n1 = (pp_avs_context->dest_w - tmp_w) / 16 - n0;
        n2 = tmp_w / (16 * factor_a);
        nls_left = n0 + n2;
        nls_right = n1 + n2;
        f = (float) n2 * 16 / tmp_w;
        
        if (n0 < 5) {
            pp_inline_parameter.grf6.video_step_delta = 0.0;

            if (x == 0) {
                pp_inline_parameter.grf5.normalized_video_x_scaling_step = 1.0 / pp_avs_context->dest_w;
                pp_inline_parameter.grf5.r5_1.source_surface_block_normalized_horizontal_origin = pp_avs_context->src_normalized_x;
            } else {
                src_x_steping = pp_inline_parameter.grf5.normalized_video_x_scaling_step;
                video_step_delta = pp_inline_parameter.grf6.video_step_delta;
                pp_inline_parameter.grf5.r5_1.source_surface_block_normalized_horizontal_origin += src_x_steping * 16 +
                    16 * 15 * video_step_delta / 2;
            }
        } else {
            if (x < nls_left) {
                /* f = a * nls_left * 16 + b * nls_left * 16 * (nls_left * 16 - 1) / 2 */
                float a = f / (nls_left * 16 * factor_b);
                float b = (f - nls_left * 16 * a) * 2 / (nls_left * 16 * (nls_left * 16 - 1));
                
                pp_inline_parameter.grf6.video_step_delta = b;

                if (x == 0) {
                    pp_inline_parameter.grf5.r5_1.source_surface_block_normalized_horizontal_origin = pp_avs_context->src_normalized_x;
                    pp_inline_parameter.grf5.normalized_video_x_scaling_step = a;
                } else {
                    src_x_steping = pp_inline_parameter.grf5.normalized_video_x_scaling_step;
                    video_step_delta = pp_inline_parameter.grf6.video_step_delta;
                    pp_inline_parameter.grf5.r5_1.source_surface_block_normalized_horizontal_origin += src_x_steping * 16 +
                        16 * 15 * video_step_delta / 2;
                    pp_inline_parameter.grf5.normalized_video_x_scaling_step += 16 * b;
                }
            } else if (x < (pp_avs_context->dest_w / 16 - nls_right)) {
                /* scale the center linearly */
                src_x_steping = pp_inline_parameter.grf5.normalized_video_x_scaling_step;
                video_step_delta = pp_inline_parameter.grf6.video_step_delta;
                pp_inline_parameter.grf5.r5_1.source_surface_block_normalized_horizontal_origin += src_x_steping * 16 +
                    16 * 15 * video_step_delta / 2;
                pp_inline_parameter.grf6.video_step_delta = 0.0;
                pp_inline_parameter.grf5.normalized_video_x_scaling_step = 1.0 / tmp_w;
            } else {
                float a = f / (nls_right * 16 * factor_b);
                float b = (f - nls_right * 16 * a) * 2 / (nls_right * 16 * (nls_right * 16 - 1));

                src_x_steping = pp_inline_parameter.grf5.normalized_video_x_scaling_step;
                video_step_delta = pp_inline_parameter.grf6.video_step_delta;
                pp_inline_parameter.grf5.r5_1.source_surface_block_normalized_horizontal_origin += src_x_steping * 16 +
                    16 * 15 * video_step_delta / 2;
                pp_inline_parameter.grf6.video_step_delta = -b;

                if (x == (pp_avs_context->dest_w / 16 - nls_right))
                    pp_inline_parameter.grf5.normalized_video_x_scaling_step = a + (nls_right * 16  - 1) * b;
                else
                    pp_inline_parameter.grf5.normalized_video_x_scaling_step -= b * 16;
            }
        }
    }

    src_y_steping = pp_static_parameter.grf1.r1_6.normalized_video_y_scaling_step;
    pp_inline_parameter.grf5.source_surface_block_normalized_vertical_origin = src_y_steping * y * 8 + pp_avs_context->src_normalized_y;
    pp_inline_parameter.grf5.destination_block_horizontal_origin = x * 16 + pp_avs_context->dest_x;
    pp_inline_parameter.grf5.destination_block_vertical_origin = y * 8 + pp_avs_context->dest_y;

    return 0;
}

static void
pp_nv12_avs_initialize(VADriverContextP ctx,
                       VASurfaceID in_surface_id, VASurfaceID out_surface_id,
                       const VARectangle *src_rect, const VARectangle *dst_rect)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;
    struct pp_avs_context *pp_avs_context = (struct pp_avs_context *)&pp_context->private_context;
    struct object_surface *obj_surface;
    struct i965_surface_state *ss;
    struct i965_sampler_8x8 *sampler_8x8;
    struct i965_sampler_8x8_state *sampler_8x8_state;
    struct i965_surface_state2 *ss_8x8;
    dri_bo *bo;
    int index;
    int in_w, in_h, in_wpitch, in_hpitch;
    int out_w, out_h, out_wpitch, out_hpitch;
    unsigned int tiling, swizzle;

    /* surface */
    obj_surface = SURFACE(in_surface_id);
    in_w = obj_surface->orig_width;
    in_h = obj_surface->orig_height;
    in_wpitch = obj_surface->width;
    in_hpitch = obj_surface->height;
    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

    /* source Y surface index 1 */
    index = 1;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "Y surface state for sample_8x8", 
                      sizeof(struct i965_surface_state2), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss_8x8 = bo->virtual;
    memset(ss_8x8, 0, sizeof(*ss_8x8));
    ss_8x8->ss0.surface_base_address = pp_context->surfaces[index].s_bo->offset;
    ss_8x8->ss1.cbcr_pixel_offset_v_direction = 0;
    ss_8x8->ss1.width = in_w - 1;
    ss_8x8->ss1.height = in_h - 1;
    ss_8x8->ss2.half_pitch_for_chroma = 0;
    ss_8x8->ss2.pitch = in_wpitch - 1;
    ss_8x8->ss2.interleave_chroma = 0;
    ss_8x8->ss2.surface_format = SURFACE_FORMAT_Y8_UNORM;
    ss_8x8->ss3.x_offset_for_cb = 0;
    ss_8x8->ss3.y_offset_for_cb = 0;
    pp_set_surface2_tiling(ss_8x8, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      0,
                      0,
                      offsetof(struct i965_surface_state2, ss0),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* source UV surface index 2 */
    index = 2;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "UV surface state for sample_8x8", 
                      sizeof(struct i965_surface_state2), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss_8x8 = bo->virtual;
    memset(ss_8x8, 0, sizeof(*ss_8x8));
    ss_8x8->ss0.surface_base_address = pp_context->surfaces[index].s_bo->offset + in_wpitch * in_hpitch;
    ss_8x8->ss1.cbcr_pixel_offset_v_direction = 0;
    ss_8x8->ss1.width = in_w - 1;
    ss_8x8->ss1.height = in_h - 1;
    ss_8x8->ss2.half_pitch_for_chroma = 0;
    ss_8x8->ss2.pitch = in_wpitch - 1;
    ss_8x8->ss2.interleave_chroma = 1;
    ss_8x8->ss2.surface_format = SURFACE_FORMAT_PLANAR_420_8;
    ss_8x8->ss3.x_offset_for_cb = 0;
    ss_8x8->ss3.y_offset_for_cb = 0;
    pp_set_surface2_tiling(ss_8x8, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      0,
                      in_wpitch * in_hpitch,
                      offsetof(struct i965_surface_state2, ss0),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* destination surface */
    obj_surface = SURFACE(out_surface_id);
    out_w = obj_surface->orig_width;
    out_h = obj_surface->orig_height;
    out_wpitch = obj_surface->width;
    out_hpitch = obj_surface->height;
    assert(out_w <= out_wpitch && out_h <= out_hpitch);
    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

    /* destination Y surface index 7 */
    index = 7;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset;
    ss->ss2.width = out_w / 4 - 1;
    ss->ss2.height = out_h - 1;
    ss->ss3.pitch = out_wpitch - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      I915_GEM_DOMAIN_RENDER,
                      0,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* destination UV surface index 8 */
    index = 8;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8G8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset + out_wpitch * out_hpitch;
    ss->ss2.width = out_w / 4 - 1;
    ss->ss2.height = out_h / 2 - 1;
    ss->ss3.pitch = out_wpitch - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      I915_GEM_DOMAIN_RENDER,
                      out_wpitch * out_hpitch,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);
    
    /* sampler 8x8 state */
    dri_bo_map(pp_context->sampler_state_table.bo_8x8, True);
    assert(pp_context->sampler_state_table.bo_8x8->virtual);
    assert(sizeof(*sampler_8x8_state) == sizeof(int) * 138);
    sampler_8x8_state = pp_context->sampler_state_table.bo_8x8->virtual;
    memset(sampler_8x8_state, 0, sizeof(*sampler_8x8_state));
    sampler_8x8_state->dw136.default_sharpness_level = 0;
    sampler_8x8_state->dw137.adaptive_filter_for_all_channel = 1;
    sampler_8x8_state->dw137.bypass_y_adaptive_filtering = 1;
    sampler_8x8_state->dw137.bypass_x_adaptive_filtering = 1;
    dri_bo_unmap(pp_context->sampler_state_table.bo_8x8);

    /* sampler 8x8 */
    dri_bo_map(pp_context->sampler_state_table.bo, True);
    assert(pp_context->sampler_state_table.bo->virtual);
    assert(sizeof(*sampler_8x8) == sizeof(int) * 16);
    sampler_8x8 = pp_context->sampler_state_table.bo->virtual;

    /* sample_8x8 Y index 1 */
    index = 1;
    memset(&sampler_8x8[index], 0, sizeof(*sampler_8x8));
    sampler_8x8[index].dw0.avs_filter_type = AVS_FILTER_ADAPTIVE_8_TAP;
    sampler_8x8[index].dw0.ief_bypass = 0;
    sampler_8x8[index].dw0.ief_filter_type = IEF_FILTER_DETAIL;
    sampler_8x8[index].dw0.ief_filter_size = IEF_FILTER_SIZE_5X5;
    sampler_8x8[index].dw1.sampler_8x8_state_pointer = pp_context->sampler_state_table.bo_8x8->offset >> 5;
    sampler_8x8[index].dw2.global_noise_estimation = 22;
    sampler_8x8[index].dw2.strong_edge_threshold = 8;
    sampler_8x8[index].dw2.weak_edge_threshold = 1;
    sampler_8x8[index].dw3.strong_edge_weight = 7;
    sampler_8x8[index].dw3.regular_weight = 2;
    sampler_8x8[index].dw3.non_edge_weight = 0;
    sampler_8x8[index].dw3.gain_factor = 40;
    sampler_8x8[index].dw4.steepness_boost = 0;
    sampler_8x8[index].dw4.steepness_threshold = 0;
    sampler_8x8[index].dw4.mr_boost = 0;
    sampler_8x8[index].dw4.mr_threshold = 5;
    sampler_8x8[index].dw5.pwl1_point_1 = 4;
    sampler_8x8[index].dw5.pwl1_point_2 = 12;
    sampler_8x8[index].dw5.pwl1_point_3 = 16;
    sampler_8x8[index].dw5.pwl1_point_4 = 26;
    sampler_8x8[index].dw6.pwl1_point_5 = 40;
    sampler_8x8[index].dw6.pwl1_point_6 = 160;
    sampler_8x8[index].dw6.pwl1_r3_bias_0 = 127;
    sampler_8x8[index].dw6.pwl1_r3_bias_1 = 98;
    sampler_8x8[index].dw7.pwl1_r3_bias_2 = 88;
    sampler_8x8[index].dw7.pwl1_r3_bias_3 = 64;
    sampler_8x8[index].dw7.pwl1_r3_bias_4 = 44;
    sampler_8x8[index].dw7.pwl1_r3_bias_5 = 0;
    sampler_8x8[index].dw8.pwl1_r3_bias_6 = 0;
    sampler_8x8[index].dw8.pwl1_r5_bias_0 = 3;
    sampler_8x8[index].dw8.pwl1_r5_bias_1 = 32;
    sampler_8x8[index].dw8.pwl1_r5_bias_2 = 32;
    sampler_8x8[index].dw9.pwl1_r5_bias_3 = 58;
    sampler_8x8[index].dw9.pwl1_r5_bias_4 = 100;
    sampler_8x8[index].dw9.pwl1_r5_bias_5 = 108;
    sampler_8x8[index].dw9.pwl1_r5_bias_6 = 88;
    sampler_8x8[index].dw10.pwl1_r3_slope_0 = -116;
    sampler_8x8[index].dw10.pwl1_r3_slope_1 = -20;
    sampler_8x8[index].dw10.pwl1_r3_slope_2 = -96;
    sampler_8x8[index].dw10.pwl1_r3_slope_3 = -32;
    sampler_8x8[index].dw11.pwl1_r3_slope_4 = -50;
    sampler_8x8[index].dw11.pwl1_r3_slope_5 = 0;
    sampler_8x8[index].dw11.pwl1_r3_slope_6 = 0;
    sampler_8x8[index].dw11.pwl1_r5_slope_0 = 116;
    sampler_8x8[index].dw12.pwl1_r5_slope_1 = 0;
    sampler_8x8[index].dw12.pwl1_r5_slope_2 = 114;
    sampler_8x8[index].dw12.pwl1_r5_slope_3 = 67;
    sampler_8x8[index].dw12.pwl1_r5_slope_4 = 9;
    sampler_8x8[index].dw13.pwl1_r5_slope_5 = -3;
    sampler_8x8[index].dw13.pwl1_r5_slope_6 = -15;
    sampler_8x8[index].dw13.limiter_boost = 0;
    sampler_8x8[index].dw13.minimum_limiter = 10;
    sampler_8x8[index].dw13.maximum_limiter = 11;
    sampler_8x8[index].dw14.clip_limiter = 130;
    dri_bo_emit_reloc(pp_context->sampler_state_table.bo,
                      I915_GEM_DOMAIN_RENDER, 
                      0,
                      0,
                      sizeof(*sampler_8x8) * index + offsetof(struct i965_sampler_8x8, dw1),
                      pp_context->sampler_state_table.bo_8x8);

    dri_bo_map(pp_context->sampler_state_table.bo_8x8_uv, True);
    assert(pp_context->sampler_state_table.bo_8x8_uv->virtual);
    assert(sizeof(*sampler_8x8_state) == sizeof(int) * 138);
    sampler_8x8_state = pp_context->sampler_state_table.bo_8x8_uv->virtual;
    memset(sampler_8x8_state, 0, sizeof(*sampler_8x8_state));
    sampler_8x8_state->dw136.default_sharpness_level = 0;
    sampler_8x8_state->dw137.adaptive_filter_for_all_channel = 0;
    sampler_8x8_state->dw137.bypass_y_adaptive_filtering = 1;
    sampler_8x8_state->dw137.bypass_x_adaptive_filtering = 1;
    dri_bo_unmap(pp_context->sampler_state_table.bo_8x8_uv);

    /* sample_8x8 UV index 2 */
    index = 2;
    memset(&sampler_8x8[index], 0, sizeof(*sampler_8x8));
    sampler_8x8[index].dw0.avs_filter_type = AVS_FILTER_NEAREST;
    sampler_8x8[index].dw0.ief_bypass = 0;
    sampler_8x8[index].dw0.ief_filter_type = IEF_FILTER_DETAIL;
    sampler_8x8[index].dw0.ief_filter_size = IEF_FILTER_SIZE_5X5;
    sampler_8x8[index].dw1.sampler_8x8_state_pointer = pp_context->sampler_state_table.bo_8x8_uv->offset >> 5;
    sampler_8x8[index].dw2.global_noise_estimation = 22;
    sampler_8x8[index].dw2.strong_edge_threshold = 8;
    sampler_8x8[index].dw2.weak_edge_threshold = 1;
    sampler_8x8[index].dw3.strong_edge_weight = 7;
    sampler_8x8[index].dw3.regular_weight = 2;
    sampler_8x8[index].dw3.non_edge_weight = 0;
    sampler_8x8[index].dw3.gain_factor = 40;
    sampler_8x8[index].dw4.steepness_boost = 0;
    sampler_8x8[index].dw4.steepness_threshold = 0;
    sampler_8x8[index].dw4.mr_boost = 0;
    sampler_8x8[index].dw4.mr_threshold = 5;
    sampler_8x8[index].dw5.pwl1_point_1 = 4;
    sampler_8x8[index].dw5.pwl1_point_2 = 12;
    sampler_8x8[index].dw5.pwl1_point_3 = 16;
    sampler_8x8[index].dw5.pwl1_point_4 = 26;
    sampler_8x8[index].dw6.pwl1_point_5 = 40;
    sampler_8x8[index].dw6.pwl1_point_6 = 160;
    sampler_8x8[index].dw6.pwl1_r3_bias_0 = 127;
    sampler_8x8[index].dw6.pwl1_r3_bias_1 = 98;
    sampler_8x8[index].dw7.pwl1_r3_bias_2 = 88;
    sampler_8x8[index].dw7.pwl1_r3_bias_3 = 64;
    sampler_8x8[index].dw7.pwl1_r3_bias_4 = 44;
    sampler_8x8[index].dw7.pwl1_r3_bias_5 = 0;
    sampler_8x8[index].dw8.pwl1_r3_bias_6 = 0;
    sampler_8x8[index].dw8.pwl1_r5_bias_0 = 3;
    sampler_8x8[index].dw8.pwl1_r5_bias_1 = 32;
    sampler_8x8[index].dw8.pwl1_r5_bias_2 = 32;
    sampler_8x8[index].dw9.pwl1_r5_bias_3 = 58;
    sampler_8x8[index].dw9.pwl1_r5_bias_4 = 100;
    sampler_8x8[index].dw9.pwl1_r5_bias_5 = 108;
    sampler_8x8[index].dw9.pwl1_r5_bias_6 = 88;
    sampler_8x8[index].dw10.pwl1_r3_slope_0 = -116;
    sampler_8x8[index].dw10.pwl1_r3_slope_1 = -20;
    sampler_8x8[index].dw10.pwl1_r3_slope_2 = -96;
    sampler_8x8[index].dw10.pwl1_r3_slope_3 = -32;
    sampler_8x8[index].dw11.pwl1_r3_slope_4 = -50;
    sampler_8x8[index].dw11.pwl1_r3_slope_5 = 0;
    sampler_8x8[index].dw11.pwl1_r3_slope_6 = 0;
    sampler_8x8[index].dw11.pwl1_r5_slope_0 = 116;
    sampler_8x8[index].dw12.pwl1_r5_slope_1 = 0;
    sampler_8x8[index].dw12.pwl1_r5_slope_2 = 114;
    sampler_8x8[index].dw12.pwl1_r5_slope_3 = 67;
    sampler_8x8[index].dw12.pwl1_r5_slope_4 = 9;
    sampler_8x8[index].dw13.pwl1_r5_slope_5 = -3;
    sampler_8x8[index].dw13.pwl1_r5_slope_6 = -15;
    sampler_8x8[index].dw13.limiter_boost = 0;
    sampler_8x8[index].dw13.minimum_limiter = 10;
    sampler_8x8[index].dw13.maximum_limiter = 11;
    sampler_8x8[index].dw14.clip_limiter = 130;
    dri_bo_emit_reloc(pp_context->sampler_state_table.bo,
                      I915_GEM_DOMAIN_RENDER, 
                      0,
                      0,
                      sizeof(*sampler_8x8) * index + offsetof(struct i965_sampler_8x8, dw1),
                      pp_context->sampler_state_table.bo_8x8_uv);

    dri_bo_unmap(pp_context->sampler_state_table.bo);

    /* private function & data */
    pp_context->pp_x_steps = pp_avs_x_steps;
    pp_context->pp_y_steps = pp_avs_y_steps;
    pp_context->pp_set_block_parameter = pp_avs_set_block_parameter;

    pp_avs_context->dest_x = dst_rect->x;
    pp_avs_context->dest_y = dst_rect->y;
    pp_avs_context->dest_w = ALIGN(dst_rect->width, 16);
    pp_avs_context->dest_h = ALIGN(dst_rect->height, 16);
    pp_avs_context->src_normalized_x = (float)src_rect->x / in_w / out_w;
    pp_avs_context->src_normalized_y = (float)src_rect->y / in_h / out_h;
    pp_avs_context->src_w = src_rect->width;
    pp_avs_context->src_h = src_rect->height;

    pp_static_parameter.grf4.r4_2.avs.nlas = 1;
    pp_static_parameter.grf1.r1_6.normalized_video_y_scaling_step = (float) src_rect->height / in_h / out_h;

    pp_inline_parameter.grf5.normalized_video_x_scaling_step = (float) src_rect->width / in_w / out_w;
    pp_inline_parameter.grf5.block_count_x = 1;        /* M x 1 */
    pp_inline_parameter.grf5.number_blocks = pp_avs_context->dest_h / 8;
    pp_inline_parameter.grf5.block_vertical_mask = 0xff;
    pp_inline_parameter.grf5.block_horizontal_mask = 0xffff;
    pp_inline_parameter.grf6.video_step_delta = 0.0;
}

static int
pp_dndi_x_steps(void *private_context)
{
    return 1;
}

static int
pp_dndi_y_steps(void *private_context)
{
    struct pp_dndi_context *pp_dndi_context = private_context;

    return pp_dndi_context->dest_h / 4;
}

static int
pp_dndi_set_block_parameter(struct i965_post_processing_context *pp_context, int x, int y)
{
    pp_inline_parameter.grf5.destination_block_horizontal_origin = x * 16;
    pp_inline_parameter.grf5.destination_block_vertical_origin = y * 4;

    return 0;
}

static 
void pp_nv12_dndi_initialize(VADriverContextP ctx,
                             VASurfaceID in_surface_id, VASurfaceID out_surface_id,
                             const VARectangle *src_rect, const VARectangle *dst_rect)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;
    struct pp_dndi_context *pp_dndi_context = (struct pp_dndi_context *)&pp_context->private_context;
    struct object_surface *obj_surface;
    struct i965_surface_state *ss;
    struct i965_surface_state2 *ss_dndi;
    struct i965_sampler_dndi *sampler_dndi;
    dri_bo *bo;
    int index;
    int w, h;
    int orig_w, orig_h;
    unsigned int tiling, swizzle;

    /* surface */
    obj_surface = SURFACE(in_surface_id);
    orig_w = obj_surface->orig_width;
    orig_h = obj_surface->orig_height;
    w = obj_surface->width;
    h = obj_surface->height;
    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

    if (pp_context->stmm.bo == NULL) {
        pp_context->stmm.bo = dri_bo_alloc(i965->intel.bufmgr,
                                           "STMM surface",
                                           w * h,
                                           4096);
        assert(pp_context->stmm.bo);
    }

    /* source UV surface index 2 */
    index = 2;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8G8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset + w * h;
    ss->ss2.width = orig_w / 4 - 1;
    ss->ss2.height = orig_h / 2 - 1;
    ss->ss3.pitch = w - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      0,
                      w * h,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* source YUV surface index 4 */
    index = 4;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "YUV surface state for deinterlace ", 
                      sizeof(struct i965_surface_state2), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss_dndi = bo->virtual;
    memset(ss_dndi, 0, sizeof(*ss_dndi));
    ss_dndi->ss0.surface_base_address = pp_context->surfaces[index].s_bo->offset;
    ss_dndi->ss1.cbcr_pixel_offset_v_direction = 0;
    ss_dndi->ss1.width = w - 1;
    ss_dndi->ss1.height = h - 1;
    ss_dndi->ss1.cbcr_pixel_offset_v_direction = 1;
    ss_dndi->ss2.half_pitch_for_chroma = 0;
    ss_dndi->ss2.pitch = w - 1;
    ss_dndi->ss2.interleave_chroma = 1;
    ss_dndi->ss2.surface_format = SURFACE_FORMAT_PLANAR_420_8;
    ss_dndi->ss2.half_pitch_for_chroma = 0;
    ss_dndi->ss2.tiled_surface = 0;
    ss_dndi->ss3.x_offset_for_cb = 0;
    ss_dndi->ss3.y_offset_for_cb = h;
    pp_set_surface2_tiling(ss_dndi, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      0,
                      0,
                      offsetof(struct i965_surface_state2, ss0),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* source STMM surface index 20 */
    index = 20;
    pp_context->surfaces[index].s_bo = pp_context->stmm.bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "STMM surface state for deinterlace ", 
                      sizeof(struct i965_surface_state2), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset;
    ss->ss2.width = w - 1;
    ss->ss2.height = h - 1;
    ss->ss3.pitch = w - 1;
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      I915_GEM_DOMAIN_RENDER,
                      0,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* destination surface */
    obj_surface = SURFACE(out_surface_id);
    orig_w = obj_surface->orig_width;
    orig_h = obj_surface->orig_height;
    w = obj_surface->width;
    h = obj_surface->height;
    dri_bo_get_tiling(obj_surface->bo, &tiling, &swizzle);

    /* destination Y surface index 7 */
    index = 7;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset;
    ss->ss2.width = orig_w / 4 - 1;
    ss->ss2.height = orig_h - 1;
    ss->ss3.pitch = w - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      I915_GEM_DOMAIN_RENDER,
                      0,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* destination UV surface index 8 */
    index = 8;
    pp_context->surfaces[index].s_bo = obj_surface->bo;
    dri_bo_reference(pp_context->surfaces[index].s_bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "surface state", 
                      sizeof(struct i965_surface_state), 
                      4096);
    assert(bo);
    pp_context->surfaces[index].ss_bo = bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    ss = bo->virtual;
    memset(ss, 0, sizeof(*ss));
    ss->ss0.surface_type = I965_SURFACE_2D;
    ss->ss0.surface_format = I965_SURFACEFORMAT_R8G8_UNORM;
    ss->ss1.base_addr = pp_context->surfaces[index].s_bo->offset + w * h;
    ss->ss2.width = orig_w / 4 - 1;
    ss->ss2.height = orig_h / 2 - 1;
    ss->ss3.pitch = w - 1;
    pp_set_surface_tiling(ss, tiling);
    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_RENDER, 
                      I915_GEM_DOMAIN_RENDER,
                      w * h,
                      offsetof(struct i965_surface_state, ss1),
                      pp_context->surfaces[index].s_bo);
    dri_bo_unmap(bo);

    /* sampler dndi */
    dri_bo_map(pp_context->sampler_state_table.bo, True);
    assert(pp_context->sampler_state_table.bo->virtual);
    assert(sizeof(*sampler_dndi) == sizeof(int) * 8);
    sampler_dndi = pp_context->sampler_state_table.bo->virtual;

    /* sample dndi index 1 */
    index = 0;
    sampler_dndi[index].dw0.denoise_asd_threshold = 0;
    sampler_dndi[index].dw0.denoise_history_delta = 8;          // 0-15, default is 8
    sampler_dndi[index].dw0.denoise_maximum_history = 128;      // 128-240
    sampler_dndi[index].dw0.denoise_stad_threshold = 0;

    sampler_dndi[index].dw1.denoise_threshold_for_sum_of_complexity_measure = 64;
    sampler_dndi[index].dw1.denoise_moving_pixel_threshold = 0;
    sampler_dndi[index].dw1.stmm_c2 = 0;
    sampler_dndi[index].dw1.low_temporal_difference_threshold = 8;
    sampler_dndi[index].dw1.temporal_difference_threshold = 16;

    sampler_dndi[index].dw2.block_noise_estimate_noise_threshold = 15;   // 0-31
    sampler_dndi[index].dw2.block_noise_estimate_edge_threshold = 7;    // 0-15
    sampler_dndi[index].dw2.denoise_edge_threshold = 7;                 // 0-15
    sampler_dndi[index].dw2.good_neighbor_threshold = 7;                // 0-63

    sampler_dndi[index].dw3.maximum_stmm = 128;
    sampler_dndi[index].dw3.multipler_for_vecm = 2;
    sampler_dndi[index].dw3.blending_constant_across_time_for_small_values_of_stmm = 0;
    sampler_dndi[index].dw3.blending_constant_across_time_for_large_values_of_stmm = 64;
    sampler_dndi[index].dw3.stmm_blending_constant_select = 0;

    sampler_dndi[index].dw4.sdi_delta = 8;
    sampler_dndi[index].dw4.sdi_threshold = 128;
    sampler_dndi[index].dw4.stmm_output_shift = 7;                      // stmm_max - stmm_min = 2 ^ stmm_output_shift
    sampler_dndi[index].dw4.stmm_shift_up = 0;
    sampler_dndi[index].dw4.stmm_shift_down = 0;
    sampler_dndi[index].dw4.minimum_stmm = 0;

    sampler_dndi[index].dw5.fmd_temporal_difference_threshold = 0;
    sampler_dndi[index].dw5.sdi_fallback_mode_2_constant = 0;
    sampler_dndi[index].dw5.sdi_fallback_mode_1_t2_constant = 0;
    sampler_dndi[index].dw5.sdi_fallback_mode_1_t1_constant = 0;

    sampler_dndi[index].dw6.dn_enable = 1;
    sampler_dndi[index].dw6.di_enable = 1;
    sampler_dndi[index].dw6.di_partial = 0;
    sampler_dndi[index].dw6.dndi_top_first = 1;
    sampler_dndi[index].dw6.dndi_stream_id = 1;
    sampler_dndi[index].dw6.dndi_first_frame = 1;
    sampler_dndi[index].dw6.progressive_dn = 0;
    sampler_dndi[index].dw6.fmd_tear_threshold = 32;
    sampler_dndi[index].dw6.fmd2_vertical_difference_threshold = 32;
    sampler_dndi[index].dw6.fmd1_vertical_difference_threshold = 32;

    sampler_dndi[index].dw7.fmd_for_1st_field_of_current_frame = 2;
    sampler_dndi[index].dw7.fmd_for_2nd_field_of_previous_frame = 1;
    sampler_dndi[index].dw7.vdi_walker_enable = 0;
    sampler_dndi[index].dw7.column_width_minus1 = w / 16;

    dri_bo_unmap(pp_context->sampler_state_table.bo);

    /* private function & data */
    pp_context->pp_x_steps = pp_dndi_x_steps;
    pp_context->pp_y_steps = pp_dndi_y_steps;
    pp_context->pp_set_block_parameter = pp_dndi_set_block_parameter;

    pp_static_parameter.grf1.statistics_surface_picth = w / 2;
    pp_static_parameter.grf1.r1_6.di.top_field_first = 0;
    pp_static_parameter.grf4.r4_2.di.motion_history_coefficient_m2 = 64;
    pp_static_parameter.grf4.r4_2.di.motion_history_coefficient_m1 = 192;

    pp_inline_parameter.grf5.block_count_x = w / 16;   /* 1 x N */
    pp_inline_parameter.grf5.number_blocks = w / 16;
    pp_inline_parameter.grf5.block_vertical_mask = 0xff;
    pp_inline_parameter.grf5.block_horizontal_mask = 0xffff;

    pp_dndi_context->dest_w = w;
    pp_dndi_context->dest_h = h;
}

static void
ironlake_pp_initialize(
    VADriverContextP   ctx,
    VASurfaceID        in_surface_id,
    VASurfaceID        out_surface_id,
    const VARectangle *src_rect,
    const VARectangle *dst_rect,
    int                pp_index
)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;
    struct pp_module *pp_module;
    dri_bo *bo;
    int i;

    dri_bo_unreference(pp_context->curbe.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "constant buffer",
                      4096, 
                      4096);
    assert(bo);
    pp_context->curbe.bo = bo;

    dri_bo_unreference(pp_context->binding_table.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "binding table",
                      sizeof(unsigned int), 
                      4096);
    assert(bo);
    pp_context->binding_table.bo = bo;

    dri_bo_unreference(pp_context->idrt.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "interface discriptor", 
                      sizeof(struct i965_interface_descriptor), 
                      4096);
    assert(bo);
    pp_context->idrt.bo = bo;
    pp_context->idrt.num_interface_descriptors = 0;

    dri_bo_unreference(pp_context->sampler_state_table.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "sampler state table", 
                      4096,
                      4096);
    assert(bo);
    dri_bo_map(bo, True);
    memset(bo->virtual, 0, bo->size);
    dri_bo_unmap(bo);
    pp_context->sampler_state_table.bo = bo;

    dri_bo_unreference(pp_context->sampler_state_table.bo_8x8);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "sampler 8x8 state ",
                      4096,
                      4096);
    assert(bo);
    pp_context->sampler_state_table.bo_8x8 = bo;

    dri_bo_unreference(pp_context->sampler_state_table.bo_8x8_uv);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "sampler 8x8 state ",
                      4096,
                      4096);
    assert(bo);
    pp_context->sampler_state_table.bo_8x8_uv = bo;

    dri_bo_unreference(pp_context->vfe_state.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "vfe state", 
                      sizeof(struct i965_vfe_state), 
                      4096);
    assert(bo);
    pp_context->vfe_state.bo = bo;
    
    for (i = 0; i < MAX_PP_SURFACES; i++) {
        dri_bo_unreference(pp_context->surfaces[i].ss_bo);
        pp_context->surfaces[i].ss_bo = NULL;

        dri_bo_unreference(pp_context->surfaces[i].s_bo);
        pp_context->surfaces[i].s_bo = NULL;
    }

    memset(&pp_static_parameter, 0, sizeof(pp_static_parameter));
    memset(&pp_inline_parameter, 0, sizeof(pp_inline_parameter));
    assert(pp_index >= PP_NULL && pp_index < NUM_PP_MODULES);
    pp_context->current_pp = pp_index;
    pp_module = &pp_context->pp_modules[pp_index];
    
    if (pp_module->initialize)
        pp_module->initialize(ctx, in_surface_id, out_surface_id,
                              src_rect, dst_rect);
}

static void
ironlake_post_processing(
    VADriverContextP   ctx,
    VASurfaceID        in_surface_id,
    VASurfaceID        out_surface_id,
    const VARectangle *src_rect,
    const VARectangle *dst_rect,
    int                pp_index
)
{
    ironlake_pp_initialize(ctx, in_surface_id, out_surface_id, src_rect, dst_rect, pp_index);
    ironlake_pp_states_setup(ctx);
    ironlake_pp_pipeline_setup(ctx);
}

static void
gen6_pp_initialize(
    VADriverContextP   ctx,
    VASurfaceID        in_surface_id,
    VASurfaceID        out_surface_id,
    const VARectangle *src_rect,
    const VARectangle *dst_rect,
    int                pp_index
)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;
    struct pp_module *pp_module;
    dri_bo *bo;
    int i;

    dri_bo_unreference(pp_context->curbe.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr,
                      "constant buffer",
                      4096, 
                      4096);
    assert(bo);
    pp_context->curbe.bo = bo;

    dri_bo_unreference(pp_context->binding_table.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "binding table",
                      sizeof(unsigned int), 
                      4096);
    assert(bo);
    pp_context->binding_table.bo = bo;

    dri_bo_unreference(pp_context->idrt.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "interface discriptor", 
                      sizeof(struct gen6_interface_descriptor_data), 
                      4096);
    assert(bo);
    pp_context->idrt.bo = bo;
    pp_context->idrt.num_interface_descriptors = 0;

    dri_bo_unreference(pp_context->sampler_state_table.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "sampler state table", 
                      4096,
                      4096);
    assert(bo);
    dri_bo_map(bo, True);
    memset(bo->virtual, 0, bo->size);
    dri_bo_unmap(bo);
    pp_context->sampler_state_table.bo = bo;

    dri_bo_unreference(pp_context->sampler_state_table.bo_8x8);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "sampler 8x8 state ",
                      4096,
                      4096);
    assert(bo);
    pp_context->sampler_state_table.bo_8x8 = bo;

    dri_bo_unreference(pp_context->sampler_state_table.bo_8x8_uv);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "sampler 8x8 state ",
                      4096,
                      4096);
    assert(bo);
    pp_context->sampler_state_table.bo_8x8_uv = bo;

    dri_bo_unreference(pp_context->vfe_state.bo);
    bo = dri_bo_alloc(i965->intel.bufmgr, 
                      "vfe state", 
                      sizeof(struct i965_vfe_state), 
                      4096);
    assert(bo);
    pp_context->vfe_state.bo = bo;
    
    for (i = 0; i < MAX_PP_SURFACES; i++) {
        dri_bo_unreference(pp_context->surfaces[i].ss_bo);
        pp_context->surfaces[i].ss_bo = NULL;

        dri_bo_unreference(pp_context->surfaces[i].s_bo);
        pp_context->surfaces[i].s_bo = NULL;
    }

    memset(&pp_static_parameter, 0, sizeof(pp_static_parameter));
    memset(&pp_inline_parameter, 0, sizeof(pp_inline_parameter));
    assert(pp_index >= PP_NULL && pp_index < NUM_PP_MODULES);
    pp_context->current_pp = pp_index;
    pp_module = &pp_context->pp_modules[pp_index];
    
    if (pp_module->initialize)
        pp_module->initialize(ctx, in_surface_id, out_surface_id,
                              src_rect, dst_rect);
}

static void
gen6_pp_binding_table(struct i965_post_processing_context *pp_context)
{
    unsigned int *binding_table;
    dri_bo *bo = pp_context->binding_table.bo;
    int i;

    dri_bo_map(bo, 1);
    assert(bo->virtual);
    binding_table = bo->virtual;
    memset(binding_table, 0, bo->size);

    for (i = 0; i < MAX_PP_SURFACES; i++) {
        if (pp_context->surfaces[i].ss_bo) {
            assert(pp_context->surfaces[i].s_bo);

            binding_table[i] = pp_context->surfaces[i].ss_bo->offset;
            dri_bo_emit_reloc(bo,
                              I915_GEM_DOMAIN_INSTRUCTION, 0,
                              0,
                              i * sizeof(*binding_table),
                              pp_context->surfaces[i].ss_bo);
        }
    
    }

    dri_bo_unmap(bo);
}

static void
gen6_pp_interface_descriptor_table(struct i965_post_processing_context *pp_context)
{
    struct gen6_interface_descriptor_data *desc;
    dri_bo *bo;
    int pp_index = pp_context->current_pp;

    bo = pp_context->idrt.bo;
    dri_bo_map(bo, True);
    assert(bo->virtual);
    desc = bo->virtual;
    memset(desc, 0, sizeof(*desc));
    desc->desc0.kernel_start_pointer = 
        pp_context->pp_modules[pp_index].kernel.bo->offset >> 6; /* reloc */
    desc->desc1.single_program_flow = 1;
    desc->desc1.floating_point_mode = FLOATING_POINT_IEEE_754;
    desc->desc2.sampler_count = 1;      /* 1 - 4 samplers used */
    desc->desc2.sampler_state_pointer = 
        pp_context->sampler_state_table.bo->offset >> 5;
    desc->desc3.binding_table_entry_count = 0;
    desc->desc3.binding_table_pointer = 
        pp_context->binding_table.bo->offset >> 5; /*reloc */
    desc->desc4.constant_urb_entry_read_offset = 0;
    desc->desc4.constant_urb_entry_read_length = 4; /* grf 1-4 */

    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_INSTRUCTION, 0,
                      0,
                      offsetof(struct gen6_interface_descriptor_data, desc0),
                      pp_context->pp_modules[pp_index].kernel.bo);

    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_INSTRUCTION, 0,
                      desc->desc2.sampler_count << 2,
                      offsetof(struct gen6_interface_descriptor_data, desc2),
                      pp_context->sampler_state_table.bo);

    dri_bo_emit_reloc(bo,
                      I915_GEM_DOMAIN_INSTRUCTION, 0,
                      desc->desc3.binding_table_entry_count,
                      offsetof(struct gen6_interface_descriptor_data, desc3),
                      pp_context->binding_table.bo);

    dri_bo_unmap(bo);
    pp_context->idrt.num_interface_descriptors++;
}

static void
gen6_pp_upload_constants(struct i965_post_processing_context *pp_context)
{
    unsigned char *constant_buffer;

    assert(sizeof(pp_static_parameter) == 128);
    dri_bo_map(pp_context->curbe.bo, 1);
    assert(pp_context->curbe.bo->virtual);
    constant_buffer = pp_context->curbe.bo->virtual;
    memcpy(constant_buffer, &pp_static_parameter, sizeof(pp_static_parameter));
    dri_bo_unmap(pp_context->curbe.bo);
}

static void
gen6_pp_states_setup(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;

    gen6_pp_binding_table(pp_context);
    gen6_pp_interface_descriptor_table(pp_context);
    gen6_pp_upload_constants(pp_context);
}

static void
gen6_pp_pipeline_select(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;

    BEGIN_BATCH(batch, 1);
    OUT_BATCH(batch, CMD_PIPELINE_SELECT | PIPELINE_SELECT_MEDIA);
    ADVANCE_BATCH(batch);
}

static void
gen6_pp_state_base_address(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;

    BEGIN_BATCH(batch, 10);
    OUT_BATCH(batch, CMD_STATE_BASE_ADDRESS | (10 - 2));
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(batch, 0 | BASE_ADDRESS_MODIFY);
    ADVANCE_BATCH(batch);
}

static void
gen6_pp_vfe_state(VADriverContextP ctx, struct i965_post_processing_context *pp_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;

    BEGIN_BATCH(batch, 8);
    OUT_BATCH(batch, CMD_MEDIA_VFE_STATE | (8 - 2));
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch,
              (pp_context->urb.num_vfe_entries - 1) << 16 |
              pp_context->urb.num_vfe_entries << 8);
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch,
              (pp_context->urb.size_vfe_entry * 2) << 16 |  /* in 256 bits unit */
              (pp_context->urb.size_cs_entry * pp_context->urb.num_cs_entries * 2 - 1));            /* in 256 bits unit */
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch, 0);
    ADVANCE_BATCH(batch);
}

static void
gen6_pp_curbe_load(VADriverContextP ctx, struct i965_post_processing_context *pp_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;

    assert(pp_context->urb.size_cs_entry * pp_context->urb.num_cs_entries * 512 <= pp_context->curbe.bo->size);

    BEGIN_BATCH(batch, 4);
    OUT_BATCH(batch, CMD_MEDIA_CURBE_LOAD | (4 - 2));
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch,
              pp_context->urb.size_cs_entry * pp_context->urb.num_cs_entries * 512);
    OUT_RELOC(batch, 
              pp_context->curbe.bo,
              I915_GEM_DOMAIN_INSTRUCTION, 0,
              0);
    ADVANCE_BATCH(batch);
}

static void
gen6_interface_descriptor_load(VADriverContextP ctx, struct i965_post_processing_context *pp_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;

    BEGIN_BATCH(batch, 4);
    OUT_BATCH(batch, CMD_MEDIA_INTERFACE_DESCRIPTOR_LOAD | (4 - 2));
    OUT_BATCH(batch, 0);
    OUT_BATCH(batch,
              pp_context->idrt.num_interface_descriptors * sizeof(struct gen6_interface_descriptor_data));
    OUT_RELOC(batch, 
              pp_context->idrt.bo,
              I915_GEM_DOMAIN_INSTRUCTION, 0,
              0);
    ADVANCE_BATCH(batch);
}

static void
gen6_pp_object_walker(VADriverContextP ctx, struct i965_post_processing_context *pp_context)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;
    int x, x_steps, y, y_steps;

    x_steps = pp_context->pp_x_steps(&pp_context->private_context);
    y_steps = pp_context->pp_y_steps(&pp_context->private_context);

    for (y = 0; y < y_steps; y++) {
        for (x = 0; x < x_steps; x++) {
            if (!pp_context->pp_set_block_parameter(pp_context, x, y)) {
                BEGIN_BATCH(batch, 22);
                OUT_BATCH(batch, CMD_MEDIA_OBJECT | 20);
                OUT_BATCH(batch, 0);
                OUT_BATCH(batch, 0); /* no indirect data */
                OUT_BATCH(batch, 0);
                OUT_BATCH(batch, 0); /* scoreboard */
                OUT_BATCH(batch, 0);

                /* inline data grf 5-6 */
                assert(sizeof(pp_inline_parameter) == 64);
                intel_batchbuffer_data(batch, &pp_inline_parameter, sizeof(pp_inline_parameter));

                ADVANCE_BATCH(batch);
            }
        }
    }
}

static void
gen6_pp_pipeline_setup(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct intel_batchbuffer *batch = i965->batch;
    struct i965_post_processing_context *pp_context = i965->pp_context;

    intel_batchbuffer_start_atomic(batch, 0x1000);
    intel_batchbuffer_emit_mi_flush(batch);
    gen6_pp_pipeline_select(ctx);
    gen6_pp_curbe_load(ctx, pp_context);
    gen6_interface_descriptor_load(ctx, pp_context);
    gen6_pp_state_base_address(ctx);
    gen6_pp_vfe_state(ctx, pp_context);
    gen6_pp_object_walker(ctx, pp_context);
    intel_batchbuffer_end_atomic(batch);
}

static void
gen6_post_processing(
    VADriverContextP   ctx,
    VASurfaceID        in_surface_id,
    VASurfaceID        out_surface_id,
    const VARectangle *src_rect,
    const VARectangle *dst_rect,
    int                pp_index
)
{
    gen6_pp_initialize(ctx, in_surface_id, out_surface_id, src_rect, dst_rect, pp_index);
    gen6_pp_states_setup(ctx);
    gen6_pp_pipeline_setup(ctx);
}

static void
i965_post_processing_internal(
    VADriverContextP   ctx,
    VASurfaceID        in_surface_id,
    VASurfaceID        out_surface_id,
    const VARectangle *src_rect,
    const VARectangle *dst_rect,
    int                pp_index
)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);

    if (IS_GEN6(i965->intel.device_id) ||
        IS_GEN7(i965->intel.device_id))
        gen6_post_processing(ctx, in_surface_id, out_surface_id, src_rect, dst_rect, pp_index);
    else
        ironlake_post_processing(ctx, in_surface_id, out_surface_id, src_rect, dst_rect, pp_index);
}

VAStatus 
i965_DestroySurfaces(VADriverContextP ctx,
                     VASurfaceID *surface_list,
                     int num_surfaces);
VAStatus 
i965_CreateSurfaces(VADriverContextP ctx,
                    int width,
                    int height,
                    int format,
                    int num_surfaces,
                    VASurfaceID *surfaces);
VASurfaceID
i965_post_processing(
    VADriverContextP   ctx,
    VASurfaceID        surface,
    const VARectangle *src_rect,
    const VARectangle *dst_rect,
    unsigned int       flags,
    int               *has_done_scaling  
)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    VASurfaceID in_surface_id = surface;
    VASurfaceID out_surface_id = VA_INVALID_ID;

    if (HAS_PP(i965)) {
        /* Currently only support post processing for NV12 surface */
        if (i965->render_state.interleaved_uv) {
            struct object_surface *obj_surface;
            VAStatus status;

            if (flags & I965_PP_FLAG_DEINTERLACING) {
                obj_surface = SURFACE(in_surface_id);
                status = i965_CreateSurfaces(ctx,
                                             obj_surface->orig_width,
                                             obj_surface->orig_height,
                                             VA_RT_FORMAT_YUV420,
                                             1,
                                             &out_surface_id);
                assert(status == VA_STATUS_SUCCESS);
                obj_surface = SURFACE(out_surface_id);
                i965_check_alloc_surface_bo(ctx, obj_surface, 0, VA_FOURCC('N','V','1','2'));
                i965_post_processing_internal(ctx,
                                              in_surface_id, out_surface_id,
                                              src_rect, dst_rect,
                                              PP_NV12_DNDI);
            }

            if (flags & I965_PP_FLAG_AVS) {
                struct i965_render_state *render_state = &i965->render_state;
                struct intel_region *dest_region = render_state->draw_region;

                if (out_surface_id != VA_INVALID_ID)
                    in_surface_id = out_surface_id;

                status = i965_CreateSurfaces(ctx,
                                             dest_region->width,
                                             dest_region->height,
                                             VA_RT_FORMAT_YUV420,
                                             1,
                                             &out_surface_id);
                assert(status == VA_STATUS_SUCCESS);
                obj_surface = SURFACE(out_surface_id);
                i965_check_alloc_surface_bo(ctx, obj_surface, 0, VA_FOURCC('N','V','1','2'));
                i965_post_processing_internal(ctx,
                                              in_surface_id, out_surface_id,
                                              src_rect, dst_rect,
                                              PP_NV12_AVS);

                if (in_surface_id != surface)
                    i965_DestroySurfaces(ctx, &in_surface_id, 1);
                
                *has_done_scaling = 1;
            }
        }
    }

    return out_surface_id;
}       

Bool
i965_post_processing_terminate(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;
    int i;

    if (HAS_PP(i965)) {
        if (pp_context) {
            dri_bo_unreference(pp_context->curbe.bo);
            pp_context->curbe.bo = NULL;

            for (i = 0; i < MAX_PP_SURFACES; i++) {
                dri_bo_unreference(pp_context->surfaces[i].ss_bo);
                pp_context->surfaces[i].ss_bo = NULL;

                dri_bo_unreference(pp_context->surfaces[i].s_bo);
                pp_context->surfaces[i].s_bo = NULL;
            }

            dri_bo_unreference(pp_context->sampler_state_table.bo);
            pp_context->sampler_state_table.bo = NULL;

            dri_bo_unreference(pp_context->sampler_state_table.bo_8x8);
            pp_context->sampler_state_table.bo_8x8 = NULL;

            dri_bo_unreference(pp_context->sampler_state_table.bo_8x8_uv);
            pp_context->sampler_state_table.bo_8x8_uv = NULL;

            dri_bo_unreference(pp_context->binding_table.bo);
            pp_context->binding_table.bo = NULL;

            dri_bo_unreference(pp_context->idrt.bo);
            pp_context->idrt.bo = NULL;
            pp_context->idrt.num_interface_descriptors = 0;

            dri_bo_unreference(pp_context->vfe_state.bo);
            pp_context->vfe_state.bo = NULL;

            dri_bo_unreference(pp_context->stmm.bo);
            pp_context->stmm.bo = NULL;

            for (i = 0; i < NUM_PP_MODULES; i++) {
                struct pp_module *pp_module = &pp_context->pp_modules[i];

                dri_bo_unreference(pp_module->kernel.bo);
                pp_module->kernel.bo = NULL;
            }

            free(pp_context);
        }

        i965->pp_context = NULL;
    }

    return True;
}

Bool
i965_post_processing_init(VADriverContextP ctx)
{
    struct i965_driver_data *i965 = i965_driver_data(ctx);
    struct i965_post_processing_context *pp_context = i965->pp_context;
    int i;

    if (HAS_PP(i965)) {
        if (pp_context == NULL) {
            pp_context = calloc(1, sizeof(*pp_context));
            i965->pp_context = pp_context;

            pp_context->urb.size = URB_SIZE((&i965->intel));
            pp_context->urb.num_vfe_entries = 32;
            pp_context->urb.size_vfe_entry = 1;     /* in 512 bits unit */
            pp_context->urb.num_cs_entries = 1;
            pp_context->urb.size_cs_entry = 2;      /* in 512 bits unit */
            pp_context->urb.vfe_start = 0;
            pp_context->urb.cs_start = pp_context->urb.vfe_start + 
                pp_context->urb.num_vfe_entries * pp_context->urb.size_vfe_entry;
            assert(pp_context->urb.cs_start + 
                   pp_context->urb.num_cs_entries * pp_context->urb.size_cs_entry <= URB_SIZE((&i965->intel)));

            assert(NUM_PP_MODULES == ARRAY_ELEMS(pp_modules_gen5));
            assert(NUM_PP_MODULES == ARRAY_ELEMS(pp_modules_gen6));

            if (IS_GEN6(i965->intel.device_id) ||
                IS_GEN7(i965->intel.device_id))
                memcpy(pp_context->pp_modules, pp_modules_gen6, sizeof(pp_context->pp_modules));
            else if (IS_IRONLAKE(i965->intel.device_id))
                memcpy(pp_context->pp_modules, pp_modules_gen5, sizeof(pp_context->pp_modules));

            for (i = 0; i < NUM_PP_MODULES; i++) {
                struct pp_module *pp_module = &pp_context->pp_modules[i];
                dri_bo_unreference(pp_module->kernel.bo);
                pp_module->kernel.bo = dri_bo_alloc(i965->intel.bufmgr,
                                                    pp_module->kernel.name,
                                                    pp_module->kernel.size,
                                                    4096);
                assert(pp_module->kernel.bo);
                dri_bo_subdata(pp_module->kernel.bo, 0, pp_module->kernel.size, pp_module->kernel.bin);
            }
        }
    }

    return True;
}

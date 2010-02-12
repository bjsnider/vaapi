/*
 * Copyright � 2009 Intel Corporation
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
 *    Zou Nan hai <nanhai.zou@intel.com>
 *
 */

#ifndef _I965_MEDIA_H_
#define _I965_MEDIA_H_

#include <xf86drm.h>
#include <drm.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>

#include "i965_structs.h"

#define MAX_INTERFACE_DESC      16
#define MAX_MEDIA_SURFACES      32

#define MPEG_TOP_FIELD		1
#define MPEG_BOTTOM_FIELD	2
#define MPEG_FRAME		3

struct decode_state;

struct media_kernel 
{
    char *name;
    int interface;
    unsigned int (*bin)[4];
    int size;
    dri_bo *bo;
};

struct i965_media_state 
{
    struct {
        dri_bo *bo;
    } surface_state[MAX_MEDIA_SURFACES];

    struct {
        dri_bo *bo;
    } binding_table;

    struct {
        dri_bo *bo;
    } idrt;  /* interface descriptor remap table */

    struct {
        dri_bo *bo;
        int enabled;
    } extended_state;

    struct {
        dri_bo *bo;
    } vfe_state;

    struct {
        dri_bo *bo;
    } curbe;

    struct {
        unsigned int vfe_start;
        unsigned int cs_start;

        unsigned int num_vfe_entries;
        unsigned int num_cs_entries;

        unsigned int size_vfe_entry;
        unsigned int size_cs_entry;
    } urb;

    void (*states_setup)(VADriverContextP ctx, struct decode_state *decode_state);
    void (*media_objects)(VADriverContextP ctx, struct decode_state *decode_state);
};

Bool i965_media_init(VADriverContextP ctx);
Bool i965_media_terminate(VADriverContextP ctx);
void i965_media_decode_picture(VADriverContextP ctx, 
                               VAProfile profile, 
                               struct decode_state *decode_state);
#endif /* _I965_MEDIA_H_ */

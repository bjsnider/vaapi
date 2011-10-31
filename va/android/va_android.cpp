/*
 * Copyright (c) 2007 Intel Corporation. All Rights Reserved.
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
 */

#define _GNU_SOURCE 1
#include "va.h"
#include "va_backend.h"
#include "va_android.h"
#include "va_dricommon.h" /* needs some helper functions from this file */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>
#ifndef ANDROID
#include <libudev.h>
#include "drmtest.h"
#endif

#define CHECK_SYMBOL(func) { if (!func) printf("func %s not found\n", #func); return VA_STATUS_ERROR_UNKNOWN; }
#define DEVICE_NAME "/dev/card0"

static int open_device (char *dev_name)
{
    struct stat st;
    int fd;

    if (-1 == stat (dev_name, &st))
    {
        printf ("Cannot identify '%s': %d, %s\n",
                dev_name, errno, strerror (errno));
        return -1;
    }

    if (!S_ISCHR (st.st_mode))
    {
        printf ("%s is no device\n", dev_name);
        return -1;
    }

    fd = open (dev_name, O_RDWR);

    if (-1 == fd)
    {
        fprintf (stderr, "Cannot open '%s': %d, %s\n",
                 dev_name, errno, strerror (errno));
        return -1;
    }

    return fd;
}

static int va_DisplayContextIsValid (
    VADisplayContextP pDisplayContext
                                  )
{
    return (pDisplayContext != NULL &&
            pDisplayContext->pDriverContext != NULL);
}

static void va_DisplayContextDestroy (
    VADisplayContextP pDisplayContext
)
{
    struct dri_state *dri_state;

    if (pDisplayContext == NULL)
        return;

    /* close the open-ed DRM fd */
    dri_state = (struct dri_state *)pDisplayContext->pDriverContext->dri_state;
    close(dri_state->fd);

    free(pDisplayContext->pDriverContext->dri_state);
    free(pDisplayContext->pDriverContext);
    free(pDisplayContext);
}

#ifdef ANDROID
static VAStatus va_DisplayContextGetDriverName (
    VADisplayContextP pDisplayContext,
    char **driver_name
)
{
    VADriverContextP ctx = pDisplayContext->pDriverContext;
    struct dri_state *dri_state = (struct dri_state *)ctx->dri_state;
    char *driver_name_env;
    int vendor_id, device_id;
    
    struct {
        int vendor_id;
        int device_id;
        char driver_name[64];
    } devices[] = {
        { 0x8086, 0x4100, "pvr" },
        { 0x8086, 0x0130, "pvr" },
        { 0x0,    0x0,    "\0" },
    };

    memset(dri_state, 0, sizeof(*dri_state));
    dri_state->fd = open_device(DEVICE_NAME);
    
    if (dri_state->fd < 0) {
        fprintf(stderr,"can't open DRM devices\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    /* TBD: other vendor driver names */
    vendor_id = devices[0].vendor_id;
    device_id = devices[0].device_id;
    *driver_name = strdup(devices[0].driver_name);
        
    dri_state->driConnectedFlag = VA_DUMMY;

    return VA_STATUS_SUCCESS;
}
#else
static VAStatus va_DisplayContextGetDriverName (
    VADisplayContextP pDisplayContext,
    char **driver_name
)
{
    VADriverContextP ctx = pDisplayContext->pDriverContext;
    struct dri_state *dri_state = (struct dri_state *)ctx->dri_state;
    char *driver_name_env;
    int vendor_id, device_id;
    int i = 0;
    
    struct {
        int vendor_id;
        int device_id;
        char driver_name[64];
    } devices[] = {
        { 0x8086, 0x4100, "pvr" },
        { 0x8086, 0x0130, "pvr" },
        { 0x0,    0x0,    "\0" },
    };

    memset(dri_state, 0, sizeof(*dri_state));
    dri_state->fd = drm_open_any(&vendor_id, &device_id);
    
    if (dri_state->fd < 0) {
        fprintf(stderr,"can't open DRM devices\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }
    
    /* TBD: other vendor driver names */

    while (devices[i].device_id != 0) {
        if ((devices[i].vendor_id == vendor_id) &&
            (devices[i].device_id == device_id))
            break;
        i++;
    }

    if (devices[i].device_id != 0)
        *driver_name = strdup(devices[i].driver_name);
    else {
        fprintf(stderr,"device (0x%04x:0x%04x) is not supported\n",
                vendor_id, device_id);
        
        return VA_STATUS_ERROR_UNKNOWN;
    }            

    printf("DRM device is opened, loading driver %s for device 0x%04x:0x%04x\n",
           driver_name, vendor_id, device_id);
    
    dri_state->driConnectedFlag = VA_DUMMY;

    return VA_STATUS_SUCCESS;
}
#endif

VADisplay vaGetDisplay (
    void *native_dpy /* implementation specific */
)
{
    VADisplay dpy = NULL;
    VADisplayContextP pDisplayContext;

    if (!native_dpy)
        return NULL;

    if (!dpy)
    {
        /* create new entry */
        VADriverContextP pDriverContext;
        struct dri_state *dri_state;
        pDisplayContext = (VADisplayContextP)calloc(1, sizeof(*pDisplayContext));
        pDriverContext  = (VADriverContextP)calloc(1, sizeof(*pDriverContext));
        dri_state       = (struct dri_state*)calloc(1, sizeof(*dri_state));
        if (pDisplayContext && pDriverContext && dri_state)
        {
            pDisplayContext->vadpy_magic = VA_DISPLAY_MAGIC;          

            pDriverContext->native_dpy       = (void *)native_dpy;
            pDisplayContext->pDriverContext  = pDriverContext;
            pDisplayContext->vaIsValid       = va_DisplayContextIsValid;
            pDisplayContext->vaDestroy       = va_DisplayContextDestroy;
            pDisplayContext->vaGetDriverName = va_DisplayContextGetDriverName;
            pDriverContext->dri_state 	     = dri_state;
            dpy                              = (VADisplay)pDisplayContext;
        }
        else
        {
            if (pDisplayContext)
                free(pDisplayContext);
            if (pDriverContext)
                free(pDriverContext);
            if (dri_state)
                free(dri_state);
        }
    }
  
    return dpy;
}

#define CTX(dpy) (((VADisplayContextP)dpy)->pDriverContext)
#define CHECK_DISPLAY(dpy) if( !vaDisplayIsValid(dpy) ) { return VA_STATUS_ERROR_INVALID_DISPLAY; }


#ifdef ANDROID
extern "C"  {
    extern int fool_postp; /* do nothing for vaPutSurface if set */
    extern int trace_flag; /* trace vaPutSurface parameters */

    void va_TracePutSurface (
        VADisplay dpy,
        VASurfaceID surface,
        void *draw, /* the target Drawable */
        short srcx,
        short srcy,
        unsigned short srcw,
        unsigned short srch,
        short destx,
        short desty,
        unsigned short destw,
        unsigned short desth,
        VARectangle *cliprects, /* client supplied clip list */
        unsigned int number_cliprects, /* number of clip rects in the clip list */
        unsigned int flags /* de-interlacing flags */
        );
}

#define VA_TRACE(trace_func,...)                \
    if (trace_flag) {                           \
        trace_func(__VA_ARGS__);                \
    }

VAStatus vaPutSurface (
    VADisplay dpy,
    VASurfaceID surface,
    sp<ISurface> draw, /* Android Surface/Window */
    short srcx,
    short srcy,
    unsigned short srcw,
    unsigned short srch,
    short destx,
    short desty,
    unsigned short destw,
    unsigned short desth,
    VARectangle *cliprects, /* client supplied clip list */
    unsigned int number_cliprects, /* number of clip rects in the clip list */
    unsigned int flags /* de-interlacing flags */
)
{
    VADriverContextP ctx;

    if (fool_postp)
        return VA_STATUS_SUCCESS;

    if (draw == NULL)
        return VA_STATUS_ERROR_UNKNOWN;

    CHECK_DISPLAY(dpy);
    ctx = CTX(dpy);

    VA_TRACE(va_TracePutSurface, dpy, surface, static_cast<void*>(&draw), srcx, srcy, srcw, srch,
             destx, desty, destw, desth,
             cliprects, number_cliprects, flags );
    
    return ctx->vtable->vaPutSurface( ctx, surface, static_cast<void*>(&draw), srcx, srcy, srcw, srch, 
                                     destx, desty, destw, desth,
                                     cliprects, number_cliprects, flags );
}
#endif

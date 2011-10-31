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
#include "sysdeps.h"
#include "va.h"
#include "va_backend.h"
#include "va_trace.h"
#include "va_fool.h"
#include "config.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>

#define DRIVER_EXTENSION	"_drv_video.so"

#define CTX(dpy) (((VADisplayContextP)dpy)->pDriverContext)
#define CHECK_DISPLAY(dpy) if( !vaDisplayIsValid(dpy) ) { return VA_STATUS_ERROR_INVALID_DISPLAY; }

#define ASSERT		assert
#define CHECK_VTABLE(s, ctx, func) if (!va_checkVtable(ctx->vtable->va##func, #func)) s = VA_STATUS_ERROR_UNKNOWN;
#define CHECK_MAXIMUM(s, ctx, var) if (!va_checkMaximum(ctx->max_##var, #var)) s = VA_STATUS_ERROR_UNKNOWN;
#define CHECK_STRING(s, ctx, var) if (!va_checkString(ctx->str_##var, #var)) s = VA_STATUS_ERROR_UNKNOWN;

extern int trace_flag;
#define VA_TRACE(trace_func,...)                \
    if (trace_flag) {                           \
        trace_func(__VA_ARGS__);                \
    }

extern int fool_decode;
extern int fool_encode;
#define VA_FOOL(fool_func,...)                 \
    if (fool_decode || fool_encode) {          \
        ret = fool_func(__VA_ARGS__);          \
    }

/*
 * read a config "env" for libva.conf or from environment setting
 * liva.conf has higher priority
 * return 0: the "env" is set, and the value is copied into env_value
 *        1: the env is not set
 */
int va_parseConfig(char *env, char *env_value)
{
    char *token, *value, *saveptr;
    char oneline[1024];
    FILE *fp=NULL;


    if (env == NULL)
        return 1;
    
    fp = fopen("/etc/libva.conf", "r");
    while (fp && (fgets(oneline, 1024, fp) != NULL)) {
	if (strlen(oneline) == 1)
	    continue;
        token = strtok_r(oneline, "=\n", &saveptr);
	value = strtok_r(NULL, "=\n", &saveptr);

	if (NULL == token || NULL == value)
	    continue;

        if (strcmp(token, env) == 0) {
            if (env_value)
                strncpy(env_value,value, 1024);

            fclose(fp);

            return 0;
        }
    }
    if (fp)
        fclose(fp);

    /* no setting in config file, use env setting */
    if (getenv(env)) {
        if (env_value)
            strncpy(env_value, getenv(env), 1024);

        return 0;
    }
    
    return 1;
}

int vaDisplayIsValid(VADisplay dpy)
{
    VADisplayContextP pDisplayContext = (VADisplayContextP)dpy;
    return pDisplayContext && (pDisplayContext->vadpy_magic == VA_DISPLAY_MAGIC) && pDisplayContext->vaIsValid(pDisplayContext);
}

void va_errorMessage(const char *msg, ...)
{
    va_list args;

    fprintf(stderr, "libva error: ");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
}

void va_infoMessage(const char *msg, ...)
{
    va_list args;

    fprintf(stderr, "libva: ");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
}

static Bool va_checkVtable(void *ptr, char *function)
{
    if (!ptr)
    {
        va_errorMessage("No valid vtable entry for va%s\n", function);
        return False;
    }
    return True;
}

static Bool va_checkMaximum(int value, char *variable)
{
    if (!value)
    {
        va_errorMessage("Failed to define max_%s in init\n", variable);
        return False;
    }
    return True;
}

static Bool va_checkString(const char* value, char *variable)
{
    if (!value)
    {
        va_errorMessage("Failed to define str_%s in init\n", variable);
        return False;
    }
    return True;
}

static VAStatus va_getDriverName(VADisplay dpy, char **driver_name)
{
    VADisplayContextP pDisplayContext = (VADisplayContextP)dpy;

    return pDisplayContext->vaGetDriverName(pDisplayContext, driver_name);
}

static VAStatus va_openDriver(VADisplay dpy, char *driver_name)
{
    VADriverContextP ctx = CTX(dpy);
    VAStatus vaStatus = VA_STATUS_ERROR_UNKNOWN;
    char *search_path = NULL;
    char *saveptr;
    char *driver_dir;
    
    if (geteuid() == getuid())
    {
        /* don't allow setuid apps to use LIBVA_DRIVERS_PATH */
        search_path = getenv("LIBVA_DRIVERS_PATH");
    }
    if (!search_path)
    {
        search_path = VA_DRIVERS_PATH;
    }

    search_path = strdup((const char *)search_path);
    driver_dir = strtok_r((const char *)search_path, ":", &saveptr);
    while(driver_dir)
    {
        void *handle = NULL;
        char *driver_path = (char *) malloc( strlen(driver_dir) +
                                             strlen(driver_name) +
                                             strlen(DRIVER_EXTENSION) + 2 );
        strncpy( driver_path, driver_dir, strlen(driver_dir) + 1);
        strncat( driver_path, "/", strlen("/") );
        strncat( driver_path, driver_name, strlen(driver_name) );
        strncat( driver_path, DRIVER_EXTENSION, strlen(DRIVER_EXTENSION) );
        
        va_infoMessage("Trying to open %s\n", driver_path);
#ifndef ANDROID
        handle = dlopen( driver_path, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE );
#else
        handle = dlopen( driver_path, RTLD_NOW| RTLD_GLOBAL);
#endif
        if (!handle)
        {
            /* Don't give errors for non-existing files */
            if (0 == access( driver_path, F_OK))
            {	
                va_errorMessage("dlopen of %s failed: %s\n", driver_path, dlerror());
            }
        }
        else
        {
            VADriverInit init_func;
            init_func = (VADriverInit) dlsym(handle, VA_DRIVER_INIT_FUNC_S);
            if (!init_func)
            {
                va_errorMessage("%s has no function %s\n", driver_path, VA_DRIVER_INIT_FUNC_S);
                dlclose(handle);
            }
            else
            {
                struct VADriverVTable *vtable = ctx->vtable;

                vaStatus = VA_STATUS_SUCCESS;
                if (!vtable) {
                    vtable = calloc(1, sizeof(*vtable));
                    if (!vtable)
                        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
                }
                ctx->vtable = vtable;

                if (VA_STATUS_SUCCESS == vaStatus)
                    vaStatus = (*init_func)(ctx);

                if (VA_STATUS_SUCCESS == vaStatus)
                {
                    CHECK_MAXIMUM(vaStatus, ctx, profiles);
                    CHECK_MAXIMUM(vaStatus, ctx, entrypoints);
                    CHECK_MAXIMUM(vaStatus, ctx, attributes);
                    CHECK_MAXIMUM(vaStatus, ctx, image_formats);
                    CHECK_MAXIMUM(vaStatus, ctx, subpic_formats);
                    CHECK_MAXIMUM(vaStatus, ctx, display_attributes);
                    CHECK_STRING(vaStatus, ctx, vendor);
                    CHECK_VTABLE(vaStatus, ctx, Terminate);
                    CHECK_VTABLE(vaStatus, ctx, QueryConfigProfiles);
                    CHECK_VTABLE(vaStatus, ctx, QueryConfigEntrypoints);
                    CHECK_VTABLE(vaStatus, ctx, QueryConfigAttributes);
                    CHECK_VTABLE(vaStatus, ctx, CreateConfig);
                    CHECK_VTABLE(vaStatus, ctx, DestroyConfig);
                    CHECK_VTABLE(vaStatus, ctx, GetConfigAttributes);
                    CHECK_VTABLE(vaStatus, ctx, CreateSurfaces);
                    CHECK_VTABLE(vaStatus, ctx, DestroySurfaces);
                    CHECK_VTABLE(vaStatus, ctx, CreateContext);
                    CHECK_VTABLE(vaStatus, ctx, DestroyContext);
                    CHECK_VTABLE(vaStatus, ctx, CreateBuffer);
                    CHECK_VTABLE(vaStatus, ctx, BufferSetNumElements);
                    CHECK_VTABLE(vaStatus, ctx, MapBuffer);
                    CHECK_VTABLE(vaStatus, ctx, UnmapBuffer);
                    CHECK_VTABLE(vaStatus, ctx, DestroyBuffer);
                    CHECK_VTABLE(vaStatus, ctx, BeginPicture);
                    CHECK_VTABLE(vaStatus, ctx, RenderPicture);
                    CHECK_VTABLE(vaStatus, ctx, EndPicture);
                    CHECK_VTABLE(vaStatus, ctx, SyncSurface);
                    CHECK_VTABLE(vaStatus, ctx, QuerySurfaceStatus);
                    CHECK_VTABLE(vaStatus, ctx, PutSurface);
                    CHECK_VTABLE(vaStatus, ctx, QueryImageFormats);
                    CHECK_VTABLE(vaStatus, ctx, CreateImage);
                    CHECK_VTABLE(vaStatus, ctx, DeriveImage);
                    CHECK_VTABLE(vaStatus, ctx, DestroyImage);
                    CHECK_VTABLE(vaStatus, ctx, SetImagePalette);
                    CHECK_VTABLE(vaStatus, ctx, GetImage);
                    CHECK_VTABLE(vaStatus, ctx, PutImage);
                    CHECK_VTABLE(vaStatus, ctx, QuerySubpictureFormats);
                    CHECK_VTABLE(vaStatus, ctx, CreateSubpicture);
                    CHECK_VTABLE(vaStatus, ctx, DestroySubpicture);
                    CHECK_VTABLE(vaStatus, ctx, SetSubpictureImage);
                    CHECK_VTABLE(vaStatus, ctx, SetSubpictureChromakey);
                    CHECK_VTABLE(vaStatus, ctx, SetSubpictureGlobalAlpha);
                    CHECK_VTABLE(vaStatus, ctx, AssociateSubpicture);
                    CHECK_VTABLE(vaStatus, ctx, DeassociateSubpicture);
                    CHECK_VTABLE(vaStatus, ctx, QueryDisplayAttributes);
                    CHECK_VTABLE(vaStatus, ctx, GetDisplayAttributes);
                    CHECK_VTABLE(vaStatus, ctx, SetDisplayAttributes);
                }
                if (VA_STATUS_SUCCESS != vaStatus)
                {
                    va_errorMessage("%s init failed\n", driver_path);
                    dlclose(handle);
                }
                if (VA_STATUS_SUCCESS == vaStatus)
                {
                    ctx->handle = handle;
                }
                free(driver_path);
                break;
            }
        }
        free(driver_path);
        
        driver_dir = strtok_r(NULL, ":", &saveptr);
    }
    
    free(search_path);    
    
    return vaStatus;
}

VAPrivFunc vaGetLibFunc(VADisplay dpy, const char *func)
{
    VADriverContextP ctx;
    if( !vaDisplayIsValid(dpy) )
        return NULL;
    ctx = CTX(dpy);

    if (NULL == ctx->handle)
        return NULL;
        
    return (VAPrivFunc) dlsym(ctx->handle, func);
}


/*
 * Returns a short english description of error_status
 */
const char *vaErrorStr(VAStatus error_status)
{
    switch(error_status)
    {
        case VA_STATUS_SUCCESS:
            return "success (no error)";
        case VA_STATUS_ERROR_OPERATION_FAILED:
            return "operation failed";
        case VA_STATUS_ERROR_ALLOCATION_FAILED:
            return "resource allocation failed";
        case VA_STATUS_ERROR_INVALID_DISPLAY:
            return "invalid VADisplay";
        case VA_STATUS_ERROR_INVALID_CONFIG:
            return "invalid VAConfigID";
        case VA_STATUS_ERROR_INVALID_CONTEXT:
            return "invalid VAContextID";
        case VA_STATUS_ERROR_INVALID_SURFACE:
            return "invalid VASurfaceID";
        case VA_STATUS_ERROR_INVALID_BUFFER:
            return "invalid VABufferID";
        case VA_STATUS_ERROR_INVALID_IMAGE:
            return "invalid VAImageID";
        case VA_STATUS_ERROR_INVALID_SUBPICTURE:
            return "invalid VASubpictureID";
        case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
            return "attribute not supported";
        case VA_STATUS_ERROR_MAX_NUM_EXCEEDED:
            return "list argument exceeds maximum number";
        case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
            return "the requested VAProfile is not supported";
        case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
            return "the requested VAEntryPoint is not supported";
        case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
            return "the requested RT Format is not supported";
        case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
            return "the requested VABufferType is not supported";
        case VA_STATUS_ERROR_SURFACE_BUSY:
            return "surface is in use";
        case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
            return "flag not supported";
        case VA_STATUS_ERROR_INVALID_PARAMETER:
            return "invalid parameter";
        case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
            return "resolution not supported";
        case VA_STATUS_ERROR_UNIMPLEMENTED:
            return "the requested function is not implemented";
        case VA_STATUS_ERROR_SURFACE_IN_DISPLAYING:
            return "surface is in displaying (may by overlay)" ;
        case VA_STATUS_ERROR_INVALID_IMAGE_FORMAT:
            return "invalid VAImageFormat";
        case VA_STATUS_ERROR_UNKNOWN:
            return "unknown libva error";
    }
    return "unknown libva error / description missing";
}
      
VAStatus vaInitialize (
    VADisplay dpy,
    int *major_version,	 /* out */
    int *minor_version 	 /* out */
)
{
    const char *driver_name_env = NULL;
    char *driver_name = NULL;
    VAStatus vaStatus;

    CHECK_DISPLAY(dpy);

    va_TraceInit(dpy);

    va_FoolInit(dpy);

    va_infoMessage("libva version %s\n", VA_VERSION_S);

    driver_name_env = getenv("LIBVA_DRIVER_NAME");
    if (driver_name_env && geteuid() == getuid())
    {
        /* Don't allow setuid apps to use LIBVA_DRIVER_NAME */
        driver_name = strdup(driver_name_env);
        vaStatus = VA_STATUS_SUCCESS;
        va_infoMessage("User requested driver '%s'\n", driver_name);
    }
    else
    {
        vaStatus = va_getDriverName(dpy, &driver_name);
        va_infoMessage("va_getDriverName() returns %d\n", vaStatus);
    }

    if (VA_STATUS_SUCCESS == vaStatus)
    {
        vaStatus = va_openDriver(dpy, driver_name);
        va_infoMessage("va_openDriver() returns %d\n", vaStatus);

        *major_version = VA_MAJOR_VERSION;
        *minor_version = VA_MINOR_VERSION;
    }

    if (driver_name)
        free(driver_name);
    
    VA_TRACE(va_TraceInitialize, dpy, major_version, minor_version);

    return vaStatus;
}


/*
 * After this call, all library internal resources will be cleaned up
 */ 
VAStatus vaTerminate (
    VADisplay dpy
)
{
  VAStatus vaStatus = VA_STATUS_SUCCESS;
  VADisplayContextP pDisplayContext = (VADisplayContextP)dpy;
  VADriverContextP old_ctx;

  CHECK_DISPLAY(dpy);
  old_ctx = CTX(dpy);

  if (old_ctx->handle) {
      vaStatus = old_ctx->vtable->vaTerminate(old_ctx);
      dlclose(old_ctx->handle);
      old_ctx->handle = NULL;
  }
  free(old_ctx->vtable);
  old_ctx->vtable = NULL;

  if (VA_STATUS_SUCCESS == vaStatus)
      pDisplayContext->vaDestroy(pDisplayContext);

  VA_TRACE(va_TraceTerminate, dpy);

  va_TraceEnd(dpy);

  va_FoolEnd(dpy);

  return vaStatus;
}

/*
 * vaQueryVendorString returns a pointer to a zero-terminated string
 * describing some aspects of the VA implemenation on a specific
 * hardware accelerator. The format of the returned string is:
 * <vendorname>-<major_version>-<minor_version>-<addtional_info>
 * e.g. for the Intel GMA500 implementation, an example would be:
 * "IntelGMA500-1.0-0.2-patch3
 */
const char *vaQueryVendorString (
    VADisplay dpy
)
{
  if( !vaDisplayIsValid(dpy) )
      return NULL;
  
  return CTX(dpy)->str_vendor;
}


/* Get maximum number of profiles supported by the implementation */
int vaMaxNumProfiles (
    VADisplay dpy
)
{
  if( !vaDisplayIsValid(dpy) )
      return 0;
  
  return CTX(dpy)->max_profiles;
}

/* Get maximum number of entrypoints supported by the implementation */
int vaMaxNumEntrypoints (
    VADisplay dpy
)
{
  if( !vaDisplayIsValid(dpy) )
      return 0;
  
  return CTX(dpy)->max_entrypoints;
}


/* Get maximum number of attributs supported by the implementation */
int vaMaxNumConfigAttributes (
    VADisplay dpy
)
{
  if( !vaDisplayIsValid(dpy) )
      return 0;
  
  return CTX(dpy)->max_attributes;
}

VAStatus vaQueryConfigEntrypoints (
    VADisplay dpy,
    VAProfile profile,
    VAEntrypoint *entrypoints,	/* out */
    int *num_entrypoints	/* out */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaQueryConfigEntrypoints ( ctx, profile, entrypoints, num_entrypoints);
}

VAStatus vaGetConfigAttributes (
    VADisplay dpy,
    VAProfile profile,
    VAEntrypoint entrypoint,
    VAConfigAttrib *attrib_list, /* in/out */
    int num_attribs
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaGetConfigAttributes ( ctx, profile, entrypoint, attrib_list, num_attribs );
}

VAStatus vaQueryConfigProfiles (
    VADisplay dpy,
    VAProfile *profile_list,	/* out */
    int *num_profiles		/* out */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaQueryConfigProfiles ( ctx, profile_list, num_profiles );
}

VAStatus vaCreateConfig (
    VADisplay dpy,
    VAProfile profile, 
    VAEntrypoint entrypoint, 
    VAConfigAttrib *attrib_list,
    int num_attribs,
    VAConfigID *config_id /* out */
)
{
  VADriverContextP ctx;
  VAStatus vaStatus = VA_STATUS_SUCCESS;
  int ret = 0;
  
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  VA_FOOL(va_FoolCreateConfig, dpy, profile, entrypoint, attrib_list, num_attribs, config_id);

  vaStatus =  ctx->vtable->vaCreateConfig ( ctx, profile, entrypoint, attrib_list, num_attribs, config_id );

  VA_TRACE(va_TraceCreateConfig, dpy, profile, entrypoint, attrib_list, num_attribs, config_id);
  
  return vaStatus;
}

VAStatus vaDestroyConfig (
    VADisplay dpy,
    VAConfigID config_id
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaDestroyConfig ( ctx, config_id );
}

VAStatus vaQueryConfigAttributes (
    VADisplay dpy,
    VAConfigID config_id, 
    VAProfile *profile, 	/* out */
    VAEntrypoint *entrypoint, 	/* out */
    VAConfigAttrib *attrib_list,/* out */
    int *num_attribs		/* out */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaQueryConfigAttributes( ctx, config_id, profile, entrypoint, attrib_list, num_attribs);
}

VAStatus vaCreateSurfaces (
    VADisplay dpy,
    int width,
    int height,
    int format,
    int num_surfaces,
    VASurfaceID *surfaces	/* out */
)
{
  VADriverContextP ctx;
  VAStatus vaStatus;
  int ret = 0;

  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  vaStatus = ctx->vtable->vaCreateSurfaces( ctx, width, height, format, num_surfaces, surfaces );

  VA_TRACE(va_TraceCreateSurface, dpy, width, height, format, num_surfaces, surfaces);

  VA_FOOL(va_FoolCreateSurfaces, dpy, width, height, format, num_surfaces, surfaces);
  
  return vaStatus;
}


VAStatus vaDestroySurfaces (
    VADisplay dpy,
    VASurfaceID *surface_list,
    int num_surfaces
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaDestroySurfaces( ctx, surface_list, num_surfaces );
}

VAStatus vaCreateContext (
    VADisplay dpy,
    VAConfigID config_id,
    int picture_width,
    int picture_height,
    int flag,
    VASurfaceID *render_targets,
    int num_render_targets,
    VAContextID *context		/* out */
)
{
  VADriverContextP ctx;
  VAStatus vaStatus;
  
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  vaStatus = ctx->vtable->vaCreateContext( ctx, config_id, picture_width, picture_height,
                                      flag, render_targets, num_render_targets, context );

  VA_TRACE(va_TraceCreateContext, dpy, config_id, picture_width, picture_height, flag, render_targets, num_render_targets, context);

  return vaStatus;
}

VAStatus vaDestroyContext (
    VADisplay dpy,
    VAContextID context
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaDestroyContext( ctx, context );
}

VAStatus vaCreateBuffer (
    VADisplay dpy,
    VAContextID context,	/* in */
    VABufferType type,		/* in */
    unsigned int size,		/* in */
    unsigned int num_elements,	/* in */
    void *data,			/* in */
    VABufferID *buf_id		/* out */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);
  int ret = 0;

  VA_FOOL(va_FoolCreateBuffer, dpy, context, type, size, num_elements, data, buf_id);
  if (ret)
      return VA_STATUS_SUCCESS;

  return ctx->vtable->vaCreateBuffer( ctx, context, type, size, num_elements, data, buf_id);
}

VAStatus vaBufferSetNumElements (
    VADisplay dpy,
    VABufferID buf_id,	/* in */
    unsigned int num_elements /* in */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaBufferSetNumElements( ctx, buf_id, num_elements );
}


VAStatus vaMapBuffer (
    VADisplay dpy,
    VABufferID buf_id,	/* in */
    void **pbuf 	/* out */
)
{
  VADriverContextP ctx;
  VAStatus va_status;
  int ret = 0;
  
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);
  
  VA_FOOL(va_FoolMapBuffer, dpy, buf_id, pbuf);
  if (ret)
      return VA_STATUS_SUCCESS;

  va_status = ctx->vtable->vaMapBuffer( ctx, buf_id, pbuf );

  if (va_status == VA_STATUS_SUCCESS)
      VA_TRACE(va_TraceMapBuffer, dpy, buf_id, pbuf);
  
  return va_status;
}

VAStatus vaUnmapBuffer (
    VADisplay dpy,
    VABufferID buf_id	/* in */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);
  int ret = 0;

  VA_FOOL(va_FoolUnmapBuffer, dpy, buf_id);
  if (ret)
      return VA_STATUS_SUCCESS;

  return ctx->vtable->vaUnmapBuffer( ctx, buf_id );
}

VAStatus vaDestroyBuffer (
    VADisplay dpy,
    VABufferID buffer_id
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaDestroyBuffer( ctx, buffer_id );
}

VAStatus vaBufferInfo (
    VADisplay dpy,
    VAContextID context,	/* in */
    VABufferID buf_id,		/* in */
    VABufferType *type,		/* out */
    unsigned int *size,		/* out */
    unsigned int *num_elements	/* out */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaBufferInfo( ctx, buf_id, type, size, num_elements );
}

VAStatus vaBeginPicture (
    VADisplay dpy,
    VAContextID context,
    VASurfaceID render_target
)
{
  VADriverContextP ctx;
  int ret = 0;

  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  VA_TRACE(va_TraceBeginPicture, dpy, context, render_target);

  VA_FOOL(va_FoolBeginPicture, dpy, context, render_target);
  if (ret)
      return VA_STATUS_SUCCESS;

  return ctx->vtable->vaBeginPicture( ctx, context, render_target );
}

VAStatus vaRenderPicture (
    VADisplay dpy,
    VAContextID context,
    VABufferID *buffers,
    int num_buffers
)
{
  VADriverContextP ctx;
  int ret = 0;

  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  VA_FOOL(va_FoolRenderPicture, dpy, context, buffers, num_buffers);
  if (ret)
      return VA_STATUS_SUCCESS;

  VA_TRACE(va_TraceRenderPicture, dpy, context, buffers, num_buffers);

  return ctx->vtable->vaRenderPicture( ctx, context, buffers, num_buffers );
}

VAStatus vaEndPicture (
    VADisplay dpy,
    VAContextID context
)
{
  VAStatus va_status;
  VADriverContextP ctx;
  int ret = 0;

  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  VA_FOOL(va_FoolEndPicture, dpy, context);
  if (ret) {
      VA_TRACE(va_TraceEndPicture, dpy, context);
      return VA_STATUS_SUCCESS;
  }
  
  va_status = ctx->vtable->vaEndPicture( ctx, context );
  
  VA_TRACE(va_TraceEndPicture, dpy, context);

  return va_status;
}

VAStatus vaSyncSurface (
    VADisplay dpy,
    VASurfaceID render_target
)
{
  VAStatus va_status;
  VADriverContextP ctx;
  int ret = 0;

  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  VA_FOOL(va_FoolSyncSurface, dpy, render_target);
  if (ret)
      return VA_STATUS_SUCCESS;
  
  va_status = ctx->vtable->vaSyncSurface( ctx, render_target );
  VA_TRACE(va_TraceSyncSurface, dpy, render_target);

  return va_status;
}

VAStatus vaQuerySurfaceStatus (
    VADisplay dpy,
    VASurfaceID render_target,
    VASurfaceStatus *status	/* out */
)
{
  VAStatus va_status;
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  va_status = ctx->vtable->vaQuerySurfaceStatus( ctx, render_target, status );

  VA_TRACE(va_TraceQuerySurfaceStatus, dpy, render_target, status);

  return va_status;
}

VAStatus vaQuerySurfaceError (
	VADisplay dpy,
	VASurfaceID surface,
	VAStatus error_status,
	void **error_info /*out*/
)
{
  VAStatus va_status;
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  va_status = ctx->vtable->vaQuerySurfaceError( ctx, surface, error_status, error_info );

  VA_TRACE(va_TraceQuerySurfaceError, dpy, surface, error_status, error_info);

  return va_status;
}

/* Get maximum number of image formats supported by the implementation */
int vaMaxNumImageFormats (
    VADisplay dpy
)
{
  if( !vaDisplayIsValid(dpy) )
      return 0;
  
  return CTX(dpy)->max_image_formats;
}

VAStatus vaQueryImageFormats (
    VADisplay dpy,
    VAImageFormat *format_list,	/* out */
    int *num_formats		/* out */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaQueryImageFormats ( ctx, format_list, num_formats);
}

/* 
 * The width and height fields returned in the VAImage structure may get 
 * enlarged for some YUV formats. The size of the data buffer that needs
 * to be allocated will be given in the "data_size" field in VAImage.
 * Image data is not allocated by this function.  The client should
 * allocate the memory and fill in the VAImage structure's data field
 * after looking at "data_size" returned from the library.
 */
VAStatus vaCreateImage (
    VADisplay dpy,
    VAImageFormat *format,
    int width,
    int height,
    VAImage *image	/* out */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaCreateImage ( ctx, format, width, height, image);
}

/*
 * Should call DestroyImage before destroying the surface it is bound to
 */
VAStatus vaDestroyImage (
    VADisplay dpy,
    VAImageID image
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaDestroyImage ( ctx, image);
}

VAStatus vaSetImagePalette (
    VADisplay dpy,
    VAImageID image,
    unsigned char *palette
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaSetImagePalette ( ctx, image, palette);
}

/*
 * Retrieve surface data into a VAImage
 * Image must be in a format supported by the implementation
 */
VAStatus vaGetImage (
    VADisplay dpy,
    VASurfaceID surface,
    int x,	/* coordinates of the upper left source pixel */
    int y,
    unsigned int width, /* width and height of the region */
    unsigned int height,
    VAImageID image
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaGetImage ( ctx, surface, x, y, width, height, image);
}

/*
 * Copy data from a VAImage to a surface
 * Image must be in a format supported by the implementation
 */
VAStatus vaPutImage (
    VADisplay dpy,
    VASurfaceID surface,
    VAImageID image,
    int src_x,
    int src_y,
    unsigned int src_width,
    unsigned int src_height,
    int dest_x,
    int dest_y,
    unsigned int dest_width,
    unsigned int dest_height
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaPutImage ( ctx, surface, image, src_x, src_y, src_width, src_height, dest_x, dest_y, dest_width, dest_height );
}

/*
 * Derive an VAImage from an existing surface.
 * This interface will derive a VAImage and corresponding image buffer from
 * an existing VA Surface. The image buffer can then be mapped/unmapped for
 * direct CPU access. This operation is only possible on implementations with
 * direct rendering capabilities and internal surface formats that can be
 * represented with a VAImage. When the operation is not possible this interface
 * will return VA_STATUS_ERROR_OPERATION_FAILED. Clients should then fall back
 * to using vaCreateImage + vaPutImage to accomplish the same task in an
 * indirect manner.
 *
 * Implementations should only return success when the resulting image buffer
 * would be useable with vaMap/Unmap.
 *
 * When directly accessing a surface special care must be taken to insure
 * proper synchronization with the graphics hardware. Clients should call
 * vaQuerySurfaceStatus to insure that a surface is not the target of concurrent
 * rendering or currently being displayed by an overlay.
 *
 * Additionally nothing about the contents of a surface should be assumed
 * following a vaPutSurface. Implementations are free to modify the surface for
 * scaling or subpicture blending within a call to vaPutImage.
 *
 * Calls to vaPutImage or vaGetImage using the same surface from which the image
 * has been derived will return VA_STATUS_ERROR_SURFACE_BUSY. vaPutImage or
 * vaGetImage with other surfaces is supported.
 *
 * An image created with vaDeriveImage should be freed with vaDestroyImage. The
 * image and image buffer structures will be destroyed; however, the underlying
 * surface will remain unchanged until freed with vaDestroySurfaces.
 */
VAStatus vaDeriveImage (
    VADisplay dpy,
    VASurfaceID surface,
    VAImage *image	/* out */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaDeriveImage ( ctx, surface, image );
}


/* Get maximum number of subpicture formats supported by the implementation */
int vaMaxNumSubpictureFormats (
    VADisplay dpy
)
{
  if( !vaDisplayIsValid(dpy) )
      return 0;
  
  return CTX(dpy)->max_subpic_formats;
}

/* 
 * Query supported subpicture formats 
 * The caller must provide a "format_list" array that can hold at
 * least vaMaxNumSubpictureFormats() entries. The flags arrary holds the flag 
 * for each format to indicate additional capabilities for that format. The actual 
 * number of formats returned in "format_list" is returned in "num_formats".
 */
VAStatus vaQuerySubpictureFormats (
    VADisplay dpy,
    VAImageFormat *format_list,	/* out */
    unsigned int *flags,	/* out */
    unsigned int *num_formats	/* out */
)
{
  VADriverContextP ctx;
  int ret = 0;

  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  VA_FOOL(va_FoolQuerySubpictureFormats, dpy, format_list, flags, num_formats);
  if (ret)
      return VA_STATUS_SUCCESS;

  return ctx->vtable->vaQuerySubpictureFormats ( ctx, format_list, flags, num_formats);
}

/* 
 * Subpictures are created with an image associated. 
 */
VAStatus vaCreateSubpicture (
    VADisplay dpy,
    VAImageID image,
    VASubpictureID *subpicture	/* out */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaCreateSubpicture ( ctx, image, subpicture );
}

/*
 * Destroy the subpicture before destroying the image it is assocated to
 */
VAStatus vaDestroySubpicture (
    VADisplay dpy,
    VASubpictureID subpicture
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaDestroySubpicture ( ctx, subpicture);
}

VAStatus vaSetSubpictureImage (
    VADisplay dpy,
    VASubpictureID subpicture,
    VAImageID image
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaSetSubpictureImage ( ctx, subpicture, image);
}


/*
 * If chromakey is enabled, then the area where the source value falls within
 * the chromakey [min, max] range is transparent
 */
VAStatus vaSetSubpictureChromakey (
    VADisplay dpy,
    VASubpictureID subpicture,
    unsigned int chromakey_min,
    unsigned int chromakey_max,
    unsigned int chromakey_mask
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaSetSubpictureChromakey ( ctx, subpicture, chromakey_min, chromakey_max, chromakey_mask );
}


/*
 * Global alpha value is between 0 and 1. A value of 1 means fully opaque and 
 * a value of 0 means fully transparent. If per-pixel alpha is also specified then
 * the overall alpha is per-pixel alpha multiplied by the global alpha
 */
VAStatus vaSetSubpictureGlobalAlpha (
    VADisplay dpy,
    VASubpictureID subpicture,
    float global_alpha 
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaSetSubpictureGlobalAlpha ( ctx, subpicture, global_alpha );
}

/*
  vaAssociateSubpicture associates the subpicture with the target_surface.
  It defines the region mapping between the subpicture and the target 
  surface through source and destination rectangles (with the same width and height).
  Both will be displayed at the next call to vaPutSurface.  Additional
  associations before the call to vaPutSurface simply overrides the association.
*/
VAStatus vaAssociateSubpicture (
    VADisplay dpy,
    VASubpictureID subpicture,
    VASurfaceID *target_surfaces,
    int num_surfaces,
    short src_x, /* upper left offset in subpicture */
    short src_y,
    unsigned short src_width,
    unsigned short src_height,
    short dest_x, /* upper left offset in surface */
    short dest_y,
    unsigned short dest_width,
    unsigned short dest_height,
    /*
     * whether to enable chroma-keying or global-alpha
     * see VA_SUBPICTURE_XXX values
     */
    unsigned int flags
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaAssociateSubpicture ( ctx, subpicture, target_surfaces, num_surfaces, src_x, src_y, src_width, src_height, dest_x, dest_y, dest_width, dest_height, flags );
}

/*
 * vaDeassociateSubpicture removes the association of the subpicture with target_surfaces.
 */
VAStatus vaDeassociateSubpicture (
    VADisplay dpy,
    VASubpictureID subpicture,
    VASurfaceID *target_surfaces,
    int num_surfaces
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaDeassociateSubpicture ( ctx, subpicture, target_surfaces, num_surfaces );
}


/* Get maximum number of display attributes supported by the implementation */
int vaMaxNumDisplayAttributes (
    VADisplay dpy
)
{
  int tmp;
    
  if( !vaDisplayIsValid(dpy) )
      return 0;
  
  tmp = CTX(dpy)->max_display_attributes;

  VA_TRACE(va_TraceMaxNumDisplayAttributes, dpy, tmp);
  
  return tmp;
}

/* 
 * Query display attributes 
 * The caller must provide a "attr_list" array that can hold at
 * least vaMaxNumDisplayAttributes() entries. The actual number of attributes
 * returned in "attr_list" is returned in "num_attributes".
 */
VAStatus vaQueryDisplayAttributes (
    VADisplay dpy,
    VADisplayAttribute *attr_list,	/* out */
    int *num_attributes			/* out */
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  VAStatus va_status;
  
  va_status = ctx->vtable->vaQueryDisplayAttributes ( ctx, attr_list, num_attributes );

  VA_TRACE(va_TraceQueryDisplayAttributes, dpy, attr_list, num_attributes);

  return va_status;
  
}

/* 
 * Get display attributes 
 * This function returns the current attribute values in "attr_list".
 * Only attributes returned with VA_DISPLAY_ATTRIB_GETTABLE set in the "flags" field
 * from vaQueryDisplayAttributes() can have their values retrieved.  
 */
VAStatus vaGetDisplayAttributes (
    VADisplay dpy,
    VADisplayAttribute *attr_list,	/* in/out */
    int num_attributes
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  VAStatus va_status;
  
  va_status = ctx->vtable->vaGetDisplayAttributes ( ctx, attr_list, num_attributes );

  VA_TRACE(va_TraceGetDisplayAttributes, dpy, attr_list, num_attributes);
  
  return va_status;
}

/* 
 * Set display attributes 
 * Only attributes returned with VA_DISPLAY_ATTRIB_SETTABLE set in the "flags" field
 * from vaQueryDisplayAttributes() can be set.  If the attribute is not settable or 
 * the value is out of range, the function returns VA_STATUS_ERROR_ATTR_NOT_SUPPORTED
 */
VAStatus vaSetDisplayAttributes (
    VADisplay dpy,
    VADisplayAttribute *attr_list,
    int num_attributes
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  VA_TRACE(va_TraceSetDisplayAttributes, dpy, attr_list, num_attributes);

  
  return ctx->vtable->vaSetDisplayAttributes ( ctx, attr_list, num_attributes );
}

VAStatus vaLockSurface(VADisplay dpy,
    VASurfaceID surface,
    unsigned int *fourcc, /* following are output argument */
    unsigned int *luma_stride,
    unsigned int *chroma_u_stride,
    unsigned int *chroma_v_stride,
    unsigned int *luma_offset,
    unsigned int *chroma_u_offset,
    unsigned int *chroma_v_offset,
    unsigned int *buffer_name,
    void **buffer 
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaLockSurface( ctx, surface, fourcc, luma_stride, chroma_u_stride, chroma_v_stride, luma_offset, chroma_u_offset, chroma_v_offset, buffer_name, buffer);
}


VAStatus vaUnlockSurface(VADisplay dpy,
    VASurfaceID surface
)
{
  VADriverContextP ctx;
  CHECK_DISPLAY(dpy);
  ctx = CTX(dpy);

  return ctx->vtable->vaUnlockSurface( ctx, surface );
}

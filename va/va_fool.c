/*
 * Copyright (c) 2009 Intel Corporation. All Rights Reserved.
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
#include "va_trace.h"
#include "va_fool.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

/*
 * Do dummy decode/encode, ignore the input data
 * In order to debug memory leak or low performance issues, we need to isolate driver problems
 * We export env "VA_FOOL", with which, we can do fake decode/encode:
 *
 * LIBVA_FOOL_DECODE:
 * . if set, decode does nothing
 * LIBVA_FOOL_ENCODE=<framename>:
 * . if set, encode does nothing, but fill in the coded buffer from the content of files with
 *   name framename.0,framename.1,framename.2, ..., framename.N, framename.N,framename.N,...
 * LIBVA_FOOL_JPEG=<framename>:fill the content of filename to codedbuf for jpeg encoding
 * LIBVA_FOOL_POSTP:
 * . if set, do nothing for vaPutSurface
 */


/* global settings */
int fool_codec = 0;
int fool_postp  = 0;

#define FOOL_CONTEXT_MAX 4

#define FOOL_BUFID_MAGIC   0x12345600
#define FOOL_BUFID_MASK    0xffffff00
/* per context settings */
static struct _fool_context {
    VADisplay dpy; /* should use context as the key */

    char *fn_enc;/* file pattern with codedbuf content for encode */
    char *segbuf_enc; /* the segment buffer of coded buffer, load frome fn_enc */
    int file_count;

    char *fn_jpg;/* file name of JPEG fool with codedbuf content */
    char *segbuf_jpg; /* the segment buffer of coded buffer, load frome fn_jpg */

    VAEntrypoint entrypoint; /* current entrypoint */
    
    /* all buffers with same type share one malloc-ed memory
     * bufferID = (buffer numbers with the same type << 8) || type
     * the malloc-ed memory can be find by fool_buf[bufferID & 0xff]
     * the size is ignored here
     */
    char *fool_buf[VABufferTypeMax]; /* memory of fool buffers */
    unsigned int fool_buf_size[VABufferTypeMax]; /* size of memory of fool buffers */
    unsigned int fool_buf_element[VABufferTypeMax]; /* element count of created buffers */
    unsigned int fool_buf_count[VABufferTypeMax]; /* count of created buffers */
    VAContextID context;
} fool_context[FOOL_CONTEXT_MAX]; /* trace five context at the same time */

#define DPY2INDEX(dpy)                                  \
    int idx;                                            \
                                                        \
    for (idx = 0; idx < FOOL_CONTEXT_MAX; idx++)        \
        if (fool_context[idx].dpy == dpy)               \
            break;                                      \
                                                        \
    if (idx == FOOL_CONTEXT_MAX)                        \
        return 0;  /* let driver go */

/* Prototype declarations (functions defined in va.c) */

void va_errorMessage(const char *msg, ...);
void va_infoMessage(const char *msg, ...);

int  va_parseConfig(char *env, char *env_value);

void va_FoolInit(VADisplay dpy)
{
    char env_value[1024];
    int fool_index = 0;

    for (fool_index = 0; fool_index < FOOL_CONTEXT_MAX; fool_index++)
        if (fool_context[fool_index].dpy == 0)
            break;

    if (fool_index == FOOL_CONTEXT_MAX)
        return;

    memset(&fool_context[fool_index], 0, sizeof(struct _fool_context));
    if (va_parseConfig("LIBVA_FOOL_POSTP", NULL) == 0) {
        fool_postp = 1;
        va_infoMessage("LIBVA_FOOL_POSTP is on, dummy vaPutSurface\n");
    }
    
    if (va_parseConfig("LIBVA_FOOL_DECODE", NULL) == 0) {
        fool_codec  |= VA_FOOL_FLAG_DECODE;
        va_infoMessage("LIBVA_FOOL_DECODE is on, dummy decode\n");
    }
    if (va_parseConfig("LIBVA_FOOL_ENCODE", &env_value[0]) == 0) {
        fool_codec  |= VA_FOOL_FLAG_ENCODE;
        fool_context[fool_index].fn_enc = strdup(env_value);
        va_infoMessage("LIBVA_FOOL_ENCODE is on, load encode data from file with patten %s\n",
                       fool_context[fool_index].fn_enc);
    }
    if (va_parseConfig("LIBVA_FOOL_JPEG", &env_value[0]) == 0) {
        fool_codec  |= VA_FOOL_FLAG_JPEG;
        fool_context[fool_index].fn_jpg = strdup(env_value);
        va_infoMessage("LIBVA_FOOL_JPEG is on, load encode data from file with patten %s\n",
                       fool_context[fool_index].fn_jpg);
    }
    
    if (fool_codec)
        fool_context[fool_index].dpy = dpy;
}


int va_FoolEnd(VADisplay dpy)
{
    int i;
    DPY2INDEX(dpy);

    for (i = 0; i < VABufferTypeMax; i++) {/* free memory */
        if (fool_context[idx].fool_buf[i])
            free(fool_context[idx].fool_buf[i]);
    }
    if (fool_context[idx].segbuf_enc)
        free(fool_context[idx].segbuf_enc);
    if (fool_context[idx].segbuf_jpg)
        free(fool_context[idx].segbuf_jpg);
    if (fool_context[idx].fn_enc)
        free(fool_context[idx].fn_enc);
    if (fool_context[idx].fn_jpg)
        free(fool_context[idx].fn_jpg);
    
    memset(&fool_context[idx], 0, sizeof(struct _fool_context));
    
    return 0;
}


int va_FoolCreateConfig(
        VADisplay dpy,
        VAProfile profile, 
        VAEntrypoint entrypoint, 
        VAConfigAttrib *attrib_list,
        int num_attribs,
        VAConfigID *config_id /* out */
)
{
    DPY2INDEX(dpy);

    fool_context[idx].entrypoint = entrypoint;
    
    /*
     * check fool_codec to align with current context
     * e.g. fool_codec = decode then for encode, the
     * vaBegin/vaRender/vaEnd also run into fool path
     * which is not desired
     */
    if (((fool_codec & VA_FOOL_FLAG_DECODE) && (entrypoint == VAEntrypointVLD)) ||
        ((fool_codec & VA_FOOL_FLAG_ENCODE) && (entrypoint == VAEntrypointEncSlice)) ||
        ((fool_codec & VA_FOOL_FLAG_JPEG) && (entrypoint == VAEntrypointEncPicture)))
        ; /* the fool_codec is meaningful */
    else
        fool_codec = 0;

    return 0; /* driver continue */
}


VAStatus va_FoolCreateBuffer(
    VADisplay dpy,
    VAContextID context,	/* in */
    VABufferType type,		/* in */
    unsigned int size,		/* in */
    unsigned int num_elements,	/* in */
    void *data,			/* in */
    VABufferID *buf_id		/* out */
)
{
    unsigned int new_size = size * num_elements;
    unsigned int old_size;
    DPY2INDEX(dpy);

    old_size = fool_context[idx].fool_buf_size[type] * fool_context[idx].fool_buf_element[type];

    if (old_size < new_size)
        fool_context[idx].fool_buf[type] = realloc(fool_context[idx].fool_buf[type], new_size);
    
    fool_context[idx].fool_buf_size[type] = size;
    fool_context[idx].fool_buf_element[type] = num_elements;
    fool_context[idx].fool_buf_count[type]++;
    /* because we ignore the vaRenderPicture, 
     * all buffers with same type share same real memory
     * bufferID = (magic number) | type
     */
    *buf_id = FOOL_BUFID_MAGIC | type;

    return 1; /* don't call into driver */
}

VAStatus va_FoolBufferInfo(
    VADisplay dpy,
    VABufferID buf_id,  /* in */
    VABufferType *type, /* out */
    unsigned int *size,         /* out */
    unsigned int *num_elements /* out */
)
{
    unsigned int magic = buf_id & FOOL_BUFID_MASK;
    DPY2INDEX(dpy);

    if (magic != FOOL_BUFID_MAGIC)
        return 0;

    *type = buf_id & 0xff;
    *size = fool_context[idx].fool_buf_size[*type];
    *num_elements = fool_context[idx].fool_buf_element[*type];;
    
    return 1; /* don't call into driver */
}

static int va_FoolFillCodedBufEnc(int idx)
{
    char file_name[1024];
    struct stat file_stat;
    VACodedBufferSegment *codedbuf;
    int i, fd = -1;

    /* try file_name.file_count, if fail, try file_name.file_count-- */
    for (i=0; i<=1; i++) {
        sprintf(file_name, "%s.%d",
                fool_context[idx].fn_enc,
                fool_context[idx].file_count);

        if ((fd = open(file_name, O_RDONLY)) != -1) {
            fstat(fd, &file_stat);
            fool_context[idx].file_count++; /* open next file */
            break;
        }
        
        fool_context[idx].file_count--; /* fall back to previous file */
        if (fool_context[idx].file_count < 0)
            fool_context[idx].file_count = 0;
    }
    if (fd != -1) {
        fool_context[idx].segbuf_enc = realloc(fool_context[idx].segbuf_enc, file_stat.st_size);
        read(fd, fool_context[idx].segbuf_enc, file_stat.st_size);
        close(fd);
    }
    codedbuf = (VACodedBufferSegment *)fool_context[idx].fool_buf[VAEncCodedBufferType];
    codedbuf->size = file_stat.st_size;
    codedbuf->bit_offset = 0;
    codedbuf->status = 0;
    codedbuf->reserved = 0;
    codedbuf->buf = fool_context[idx].segbuf_enc;
    codedbuf->next = NULL;

    return 0;
}


static int va_FoolFillCodedBufJPG(int idx)
{
    struct stat file_stat;
    VACodedBufferSegment *codedbuf;
    int i, fd = -1;

    if ((fd = open(fool_context[idx].fn_jpg, O_RDONLY)) != -1)
        fstat(fd, &file_stat);
        
    if (fd != -1) {
        fool_context[idx].segbuf_jpg = realloc(fool_context[idx].segbuf_jpg, file_stat.st_size);
        read(fd, fool_context[idx].segbuf_jpg, file_stat.st_size);
        close(fd);
    }
    codedbuf = (VACodedBufferSegment *)fool_context[idx].fool_buf[VAEncCodedBufferType];
    codedbuf->size = file_stat.st_size;
    codedbuf->bit_offset = 0;
    codedbuf->status = 0;
    codedbuf->reserved = 0;
    codedbuf->buf = fool_context[idx].segbuf_jpg;
    codedbuf->next = NULL;

    return 0;
}


static int va_FoolFillCodedBuf(int idx)
{
    if (fool_context[idx].entrypoint == VAEntrypointEncSlice)
        va_FoolFillCodedBufEnc(idx);
    else if (fool_context[idx].entrypoint == VAEntrypointEncPicture)
        va_FoolFillCodedBufJPG(idx);
        
    return 0;
}


VAStatus va_FoolMapBuffer(
    VADisplay dpy,
    VABufferID buf_id,	/* in */
    void **pbuf 	/* out */
)
{
    unsigned int buftype = buf_id & 0xff;
    unsigned int magic = buf_id & FOOL_BUFID_MASK;
    DPY2INDEX(dpy);

    if (magic != FOOL_BUFID_MAGIC)
        return 0;

    /* buf_id is the buffer type */
    *pbuf = fool_context[idx].fool_buf[buftype];

    /* it is coded buffer, fill the fake segment buf from file */
    if (*pbuf && (buftype == VAEncCodedBufferType))
        va_FoolFillCodedBuf(idx);
    
    return 1; /* don't call into driver */
}

VAStatus va_FoolUnmapBuffer(
        VADisplay dpy,
        VABufferID buf_id	/* in */
)
{
    unsigned int magic = buf_id & FOOL_BUFID_MASK;

    if (magic != FOOL_BUFID_MAGIC)
        return 0;

    return 1;
}

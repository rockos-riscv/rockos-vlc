/*****************************************************************************
 * vlc_drm.c: esdrm helper for VLC
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vlc_esdrm.h"

#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <fcntl.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_atomic.h>
#include <vlc_fourcc.h>
#include <vlc_filter.h>
#include <vlc_picture_pool.h>
#include <libavcodec/avcodec.h>

struct esw_pic_ctx {
    picture_context_t s;
    picture_t *picref;
    void *data;
};

struct picture_sys_t {
    struct esw_pic_ctx ctx;
};

static void pool_pic_destroy_cb(picture_t *pic) {
    picture_sys_t *p_sys = pic->p_sys;
    // printf("pool_pic_destroy_cb %p\n", pic);
    if (!pic) return;
    if (p_sys) free(p_sys);
    free(pic);
}

static void pic_ctx_destroy_cb(struct picture_context_t *opaque) {
    struct esw_pic_ctx *ctx = (struct esw_pic_ctx *)opaque;
    //   AVFrame *frame = (AVFrame *)ctx->data;
    // av_frame_unref(&frame);
    picture_Release(ctx->picref);
    free(opaque);
}

static void pic_sys_ctx_destroy_cb(struct picture_context_t *opaque) {
    struct esw_pic_ctx *ctx = (struct esw_pic_ctx *)opaque;

    if (ctx->data) {
        AVFrame *frame = ctx->data;
        av_frame_unref(frame);
    }
}

static struct picture_context_t *pic_ctx_copy_cb(struct picture_context_t *opaque) {
    struct esw_pic_ctx *src_ctx = (struct esw_pic_ctx *)opaque;
    struct esw_pic_ctx *dst_ctx = malloc(sizeof *dst_ctx);
    if (dst_ctx == NULL) return NULL;

    dst_ctx->s.destroy = pic_ctx_destroy_cb;
    dst_ctx->s.copy = pic_ctx_copy_cb;
    dst_ctx->data = src_ctx->data;
    // av_frame_ref((AVFrame *)dst_ctx->data, (AVFrame *)src_ctx->data);
    dst_ctx->picref = picture_Hold(src_ctx->picref);

    // printf("pic_ctx_copy_cb frame:%p\n", dst_ctx->data);
    return &dst_ctx->s;
}

picture_pool_t *vlc_esdrm_prime_PoolNew(vlc_object_t *o, const video_format_t *restrict fmt, unsigned count) {
    picture_t *pics[count] = {};

    VLC_UNUSED(o);

    for (unsigned i = 0; i < count; i++) {
        picture_sys_t *p_sys = malloc(sizeof *p_sys);
        if (p_sys == NULL) {
            count = i;
            goto error_pic;
        }

        AVFrame *frame = NULL;
        frame = av_frame_alloc();
        if (unlikely(frame == NULL)) {
            goto error_pic;
        }

        p_sys->ctx.s.destroy = pic_sys_ctx_destroy_cb;
        p_sys->ctx.s.copy = pic_ctx_copy_cb;
        p_sys->ctx.picref = NULL;
        p_sys->ctx.data = (void *)frame;

        picture_resource_t rsc = {
            .p_sys = p_sys,
            .pf_destroy = pool_pic_destroy_cb,
        };
        pics[i] = picture_NewFromResource(fmt, &rsc);
        if (pics[i] == NULL) {
            free(p_sys);
            count = i;
            goto error_pic;
        }
	    // msg_Err(o, "esdrm allocat frame: %p, pic[%d]: %p, p_sys: %p", frame, i, pics[i], pics[i]->p_sys);
    }

    picture_pool_t *pool = picture_pool_New(count, pics);
    if (!pool) goto error_pic;
    return pool;

error_pic:
    while (count > 0) picture_Release(pics[--count]);
    return NULL;
}

void vlc_esdrm_PicAttachContext(picture_t *pic) {
    assert(pic != NULL);
    assert(pic->p_sys != NULL);
    assert(pic->context == NULL);

    pic->p_sys->ctx.picref = pic;
    pic->context = &pic->p_sys->ctx.s;
}

void *vlc_esdrm_PicGetData(picture_t *pic) {
    assert(pic != NULL);
    assert(pic->context);

    return pic->p_sys->ctx.data;
}
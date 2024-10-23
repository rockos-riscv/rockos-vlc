/*****************************************************************************
 * converter_drm_prime.c: OpenGL drm_prime opaque converter
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "converter.h"
#include <vlc_vout_window.h>

#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/frame.h>
#include <drm/drm_fourcc.h>
#include "../../hw/esdrm/vlc_esdrm.h"

struct priv {
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
};

static EGLImageKHR drm_prime_egl_image_create(
    const opengl_tex_converter_t *tc, EGLint w, EGLint h, EGLint fourcc, EGLint fd, EGLint offset, EGLint pitch) {
    EGLint attribs[] = {EGL_WIDTH,
                        w,
                        EGL_HEIGHT,
                        h,
                        EGL_LINUX_DRM_FOURCC_EXT,
                        fourcc,
                        EGL_DMA_BUF_PLANE0_FD_EXT,
                        fd,
                        EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                        offset,
                        EGL_DMA_BUF_PLANE0_PITCH_EXT,
                        pitch,
                        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
                        DRM_FORMAT_MOD_LINEAR & 0xffffffff,
                        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
                        DRM_FORMAT_MOD_LINEAR >> 32,
                        EGL_NONE};

    return tc->gl->egl.createImageKHR(tc->gl, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
}

static void drm_prime_egl_image_destroy(const opengl_tex_converter_t *tc, EGLImageKHR image) {
    tc->gl->egl.destroyImageKHR(tc->gl, image);
}

static int tc_drm_prime_egl_update(const opengl_tex_converter_t *tc,
                                   GLuint *textures,
                                   const GLsizei *tex_width,
                                   const GLsizei *tex_height,
                                   picture_t *pic,
                                   const size_t *plane_offset) {
    (void)plane_offset;
    struct priv *priv = tc->priv;
    vlc_object_t *o = VLC_OBJECT(tc->gl);
    int ret = VLC_SUCCESS;

    AVFrame *frame = (AVFrame *)vlc_esdrm_PicGetData(pic);
    // msg_Err(o, "tc_drm_prime_egl_update, pic: %p, p_sys: %p, frame: %p, w: %d, h: %d", pic, pic->p_sys, frame, frame->width, frame->height);
    if (!frame) {
        msg_Err(o, "tc_drm_prime_egl_update, AVFrame is NULL");
        return VLC_EGENERIC;
    }

    AVDRMFrameDescriptor *drm_desc = (AVDRMFrameDescriptor *)frame->data[0];
    if (!drm_desc) {
        msg_Err(o, "tc_drm_prime_egl_update,drm_desc is NULL");
        return VLC_EGENERIC;
    }

    EGLImageKHR egl_images[3] = { NULL };

    if(drm_desc->layers->nb_planes <= 1) return VLC_EGENERIC;

    EGLint drm_fourccs[3] = {};
    // fourcc converter
    switch(drm_desc->layers->format) {
        case DRM_FORMAT_NV12:
        case DRM_FORMAT_NV21:
            drm_fourccs[0] = DRM_FORMAT_R8;
            drm_fourccs[1] = DRM_FORMAT_GR88;
            break;
        case DRM_FORMAT_YUV420:
            drm_fourccs[0] = DRM_FORMAT_R8;
            drm_fourccs[1] = DRM_FORMAT_R8;
            drm_fourccs[2] = DRM_FORMAT_R8;
            break;
        case DRM_FORMAT_P010:
            drm_fourccs[0] = DRM_FORMAT_R16;
            drm_fourccs[1] = DRM_FORMAT_GR1616;
            break;
        default:
            msg_Err(o, "tc_drm_prime_egl_update not support format: %x", drm_desc->layers->format);
            return VLC_EGENERIC;
    }

    for(int i = 0; i < drm_desc->layers->nb_planes; i++) {
        // msg_Err(o,
        //     "tc_drm_prime_egl_update %d, W: %d, H: %d, format: %x, fd: %d, offset: %d, pitch: %d",
        //                                                                                             i,
        //                                                                                             tex_width[i],
        //                                                                                             tex_height[i],
        //                                                                                             drm_fourccs[i],
        //                                                                                             drm_desc->objects[0].fd,
        //                                                                                             drm_desc->layers->planes[i].offset,
        //                                                                                             drm_desc->layers->planes[i].pitch);
        // create KHR image
        egl_images[i] = drm_prime_egl_image_create( tc,
                                            tex_width[i],
                                            tex_height[i],
                                            drm_fourccs[i],
                                            drm_desc->objects[0].fd,
                                            drm_desc->layers->planes[i].offset,
                                            drm_desc->layers->planes[i].pitch);
        if(!egl_images[i]) {
            msg_Err(o, "tc_drm_prime_egl_update, create egl_image[%d] failed", i);
            ret = VLC_EGENERIC;
            break;
        }
        // bind tex
        tc->vt->BindTexture(tc->tex_target, textures[i]);
        priv->glEGLImageTargetTexture2DOES(tc->tex_target, egl_images[i]);
    }

    for(int i = 0; i < drm_desc->layers->nb_planes; i++) {
        if(egl_images[i]) drm_prime_egl_image_destroy(tc, egl_images[i]);
    }

    return ret;
}

static picture_pool_t *tc_drm_prime_egl_get_pool(const opengl_tex_converter_t *tc, unsigned requested_count) {
    picture_pool_t *pool = vlc_esdrm_prime_PoolNew(VLC_OBJECT(tc->gl), &tc->fmt, requested_count);
    // msg_Dbg(tc, "tc_drm_prime_egl_get_pool, pool: %p", pool);
    return pool;
}

static void Close(vlc_object_t *obj) {
    opengl_tex_converter_t *tc = (void *)obj;
    struct priv *priv = tc->priv;

    free(priv);
}

static int drm_prime_check_chroma(vlc_fourcc_t chroma) {
    return ((chroma == VLC_CODEC_DRMP_NV12)
           || (chroma == VLC_CODEC_DRMP_NV21)
           || (chroma == VLC_CODEC_DRMP_YUV420P)
           || (chroma == VLC_CODEC_DRMP_P010));
}

static vlc_fourcc_t drm_prime_get_chroma(const vlc_fourcc_t chroma) {
    switch(chroma) {
        case VLC_CODEC_DRMP_NV12:
            return VLC_CODEC_NV12;
        case VLC_CODEC_DRMP_NV21:
            return VLC_CODEC_NV21;
        case VLC_CODEC_DRMP_YUV420P:
            return VLC_CODEC_I420;
        case VLC_CODEC_DRMP_P010:
            return VLC_CODEC_P010;
        default:
            return VLC_CODEC_NV12;
    }
}

static int Open(vlc_object_t *obj) {
    opengl_tex_converter_t *tc = (void *)obj;

    // msg_Dbg(tc, "converter_drm_prime, i_chroma: %x", tc->fmt.i_chroma);
    if(!drm_prime_check_chroma(tc->fmt.i_chroma)) {
        msg_Err(tc, "converter_drm_prime fail, not support chroma: %x", tc->fmt.i_chroma);
        return VLC_EGENERIC;
    }

    if (tc->gl->ext != VLC_GL_EXT_EGL || tc->gl->egl.createImageKHR == NULL
        || tc->gl->egl.destroyImageKHR == NULL) {
        msg_Err(tc, "converter_drm_prime fail, not support ImageKHR");
        return VLC_EGENERIC;
    }

    if (!HasExtension(tc->glexts, "GL_OES_EGL_image")) {
        msg_Err(tc, "converter_drm_prime fail, not support GL_OES_EGL_image");
        return VLC_EGENERIC;
    }

    const char *eglexts = tc->gl->egl.queryString(tc->gl, EGL_EXTENSIONS);
    if (eglexts == NULL || !HasExtension(eglexts, "EGL_EXT_image_dma_buf_import")) {
        msg_Err(tc, "converter_drm_prime fail, not support EGL_EXT_image_dma_buf_import");
        return VLC_EGENERIC;
    }

    struct priv *priv = tc->priv = calloc(1, sizeof(struct priv));
    if (unlikely(tc->priv == NULL)) {
        msg_Err(tc, "converter_drm_prime fail, no memory");
        goto error;
    }

    priv->glEGLImageTargetTexture2DOES = vlc_gl_GetProcAddress(tc->gl, "glEGLImageTargetTexture2DOES");
    if (priv->glEGLImageTargetTexture2DOES == NULL) {
        msg_Err(tc, "converter_drm_prime fail, not support glEGLImageTargetTexture2DOES");
        goto error;
    }

    tc->fshader = opengl_fragment_shader_init(tc, GL_TEXTURE_2D, drm_prime_get_chroma(tc->fmt.i_chroma), COLOR_SPACE_UNDEF);
    if (tc->fshader == 0) goto error;

    tc->pf_update = tc_drm_prime_egl_update;
    tc->pf_get_pool = tc_drm_prime_egl_get_pool;

    // msg_Dbg(tc, "converter_drm_prime OK, %d", __LINE__);
    return VLC_SUCCESS;
error:
    free(priv);
    return VLC_EGENERIC;
}

vlc_module_begin()
    set_description("OpenGL surface converter from drm-prime")
    set_capability("glconv", 1)
    set_callbacks(Open, Close)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_shortcut("drm", "drm-prime")
vlc_module_end()

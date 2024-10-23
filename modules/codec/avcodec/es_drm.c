/*****************************************************************************
 * es_drm.c: es drm helpers for the libavcodec decoder
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fourcc.h>
#include <vlc_picture.h>
#include <vlc_picture_pool.h>

#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>

#include "avcodec.h"
#include "va.h"
#include "../../hw/esdrm/vlc_esdrm.h"

struct vlc_va_sys_t {
    // TODO
};

static int Get(vlc_va_t *va, picture_t *pic, uint8_t **data) {
    // printf("es_drm Get\n");
    (void)va;

    vlc_esdrm_PicAttachContext(pic);
    *data = (void *)vlc_esdrm_PicGetData(pic);

    return VLC_SUCCESS;
}

static int Create(vlc_va_t *va,
                  AVCodecContext *ctx,
                  const AVPixFmtDescriptor *desc,
                  enum PixelFormat pix_fmt,
                  const es_format_t *fmt,
                  picture_sys_t *p_sys) {
    VLC_UNUSED(ctx);
    VLC_UNUSED(desc);
    VLC_UNUSED(p_sys);
    if (pix_fmt != AV_PIX_FMT_DRM_PRIME) return VLC_EGENERIC;

    (void)fmt;

    int ret = VLC_EGENERIC;
    vlc_va_sys_t *sys = malloc(sizeof *sys);
    if (unlikely(sys == NULL)) {
        ret = VLC_ENOMEM;
        goto error;
    }
    memset(sys, 0, sizeof(*sys));

    /* TODO print the hardware name/vendor for debugging purposes */
    va->sys = sys;
    va->description = "eswapi";
    va->get = Get;
    return VLC_SUCCESS;

error:
    return ret;
}

static void Delete(vlc_va_t *va, void **hwctx) {
    vlc_va_sys_t *sys = va->sys;
    // vlc_object_t *o = VLC_OBJECT(va);
    (void)hwctx;

    // TODO

    free(sys);
}

vlc_module_begin() 
    set_description(N_("ES-DRM video decoder")) 
    set_capability("hw decoder", 100)
    set_callbacks(Create, Delete) 
    add_shortcut("esdrm") 
    set_category(CAT_INPUT) 
    set_subcategory(SUBCAT_INPUT_VCODEC)
vlc_module_end()

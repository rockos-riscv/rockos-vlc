/*****************************************************************************
 * vlc_drm.h: esdrm helper for VLC
 *****************************************************************************/

#ifndef VLC_ESDRM_H
#define VLC_ESDRM_H

#include <vlc_common.h>
#include <vlc_fourcc.h>
#include <vlc_picture_pool.h>

picture_pool_t *vlc_esdrm_prime_PoolNew(vlc_object_t *o, const video_format_t *restrict fmt, unsigned count);
void vlc_esdrm_PicAttachContext(picture_t *pic);
void *vlc_esdrm_PicGetData(picture_t *pic);
#endif /* VLC_VAAPI_H */

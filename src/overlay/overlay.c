#include "overlay.h"
#include "config.h"
#include "drm_warpper.h"

int overlay_init(overlay_t* overlay,drm_warpper_t* drm_warpper){

    drm_warpper_allocate_buffer(drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &overlay->overlay_buf_1);
    drm_warpper_allocate_buffer(drm_warpper, DRM_WARPPER_LAYER_OVERLAY, &overlay->overlay_buf_2);

    drm_warpper_mount_layer(drm_warpper, DRM_WARPPER_LAYER_OVERLAY, 0, 0, &overlay->overlay_buf_1);

    overlay->drm_warpper = drm_warpper;
    return 0;
}
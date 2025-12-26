// sync with kernel driver
#ifndef _UAPI_SRGN_DRM_H_
#define _UAPI_SRGN_DRM_H_

#include <linux/types.h>
#include <drm/drm.h>
#include <stdint.h>

#define DRM_SRGN_MOUNT_FB 0x00
#define DRM_SRGN_RESET_CACHE 0x01


#define DRM_SRGN_MOUNT_FB_TYPE_NORMAL 0x00
#define DRM_SRGN_MOUNT_FB_TYPE_YUV 0x01

#define DRM_IOCTL_SRGN_MOUNT_FB DRM_IOW(DRM_COMMAND_BASE + DRM_SRGN_MOUNT_FB, struct drm_srgn_mount_fb)
#define DRM_IOCTL_SRGN_RESET_CACHE DRM_IO(DRM_COMMAND_BASE + DRM_SRGN_RESET_CACHE)

struct drm_srgn_mount_fb {
	uint32_t layer_id;
    uint32_t type;
    uint32_t ch0_addr;
    uint32_t ch1_addr;
    uint32_t ch2_addr;
};

#endif
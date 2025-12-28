#ifndef _UAPI_SRGN_DRM_H_
#define _UAPI_SRGN_DRM_H_

#include <linux/types.h>
#include <drm/drm.h>

/* this is actually yet another atomic commit ,
 * just like the atomic commit ioctl in drm,
 * drm ioctl will do full modeset(incl. initalize defe registers,....)
 * which is too slow
 */

#define DRM_SRGN_ATOMIC_COMMIT 0x00
#define DRM_SRGN_RESET_FB_CACHE 0x01

// arg0: fb address(in userspace)
#define DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL 0x00
// arg0: fb Y address(in userspace)
// arg1: fb UV address(in userspace)
#define DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_YUV 0x01
// arg0: y<<16 | x, y,x is in two's complement form,pixel
#define DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_COORD 0x02
// arg0: alpha, 255 = no alpha.
// set alpha = 255 before use pixel-wise alpha.
#define DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_ALPHA 0x03

#define DRM_IOCTL_SRGN_ATOMIC_COMMIT \
	DRM_IOW(DRM_COMMAND_BASE + DRM_SRGN_ATOMIC_COMMIT, struct drm_srgn_atomic_commit)
#define DRM_IOCTL_SRGN_RESET_FB_CACHE DRM_IO(DRM_COMMAND_BASE + DRM_SRGN_RESET_FB_CACHE)

struct drm_srgn_atomic_commit_data {
	__u32 layer_id;
	__u32 type;
	__u32 arg0;
	__u32 arg1;
	__u32 arg2;
};

struct drm_srgn_atomic_commit {
	__u32 size;
	__u32 data;
};

#endif
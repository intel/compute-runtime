/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "third_party/uapi/prelim/drm/i915_drm.h"

namespace NEO {

int IoctlHelperPrelim20::execBuffer(Drm *drm, drm_i915_gem_execbuffer2 *execBuffer, uint64_t completionGpuAddress, uint32_t counterValue) {
    return ioctl(drm, DRM_IOCTL_I915_GEM_EXECBUFFER2, execBuffer);
}

bool IoctlHelperPrelim20::completionFenceExtensionSupported(Drm &drm, const HardwareInfo &hwInfo) {
    return false;
}

} // namespace NEO

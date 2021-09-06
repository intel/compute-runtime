/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace DRM_TIP {
#undef DRM_IOCTL_I915_GEM_CREATE_EXT
#undef __I915_EXEC_UNKNOWN_FLAGS
#include "third_party/uapi/drm_tip/drm/i915_drm.h"
} // namespace DRM_TIP

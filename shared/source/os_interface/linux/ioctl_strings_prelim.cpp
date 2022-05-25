/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "third_party/uapi/prelim/drm/i915_drm.h"

#include <string>
#include <sys/ioctl.h>

namespace NEO {

namespace IoctlToStringHelper {
std::string getIoctlParamStringRemaining(int param) {
    switch (param) {
    case PRELIM_I915_PARAM_HAS_VM_BIND:
        return "PRELIM_I915_PARAM_HAS_VM_BIND";
    default:
        return std::to_string(param);
    }
}
} // namespace IoctlToStringHelper

} // namespace NEO

/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "drm/xe_drm.h"

namespace NEO {
void IoctlHelperXe::setContextProperties(const OsContextLinux &osContext, void *extProperties, uint32_t &extIndexInOut) {

    auto &ext = *reinterpret_cast<std::array<drm_xe_ext_set_property, maxContextSetProperties> *>(extProperties);

    if (osContext.isLowPriority()) {
        ext[extIndexInOut].base.name = DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY;
        ext[extIndexInOut].property = DRM_XE_EXEC_QUEUE_SET_PROPERTY_PRIORITY;
        ext[extIndexInOut].value = 0;
        if (extIndexInOut > 0) {
            ext[extIndexInOut - 1].base.next_extension = castToUint64(&ext[extIndexInOut]);
        }
        extIndexInOut++;
    }
}

} // namespace NEO
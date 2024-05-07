/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

namespace NEO {
bool Drm::isDrmSupported(int fileDescriptor) {
    auto drmVersion = Drm::getDrmVersion(fileDescriptor);
    return "i915" == drmVersion || "xe" == drmVersion;
}

bool Drm::queryDeviceIdAndRevision() {
    auto drmVersion = Drm::getDrmVersion(getFileDescriptor());
    if ("xe" == drmVersion) {
        this->ioctlHelper = IoctlHelperXe::create(*this);
        auto xeIoctlHelperPtr = static_cast<IoctlHelperXe *>(this->ioctlHelper.get());
        this->setPerContextVMRequired(false);
        return xeIoctlHelperPtr->initialize();
    }
    return queryI915DeviceIdAndRevision();
}

} // namespace NEO

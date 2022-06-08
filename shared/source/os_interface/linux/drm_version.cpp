/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {
bool Drm::isDrmSupported(int fileDescriptor) {
    auto drmVersion = Drm::getDrmVersion(fileDescriptor);
    return "i915" == drmVersion;
}

bool Drm::queryDeviceIdAndRevision() {
    return queryI915DeviceIdAndRevision();
}

} // namespace NEO

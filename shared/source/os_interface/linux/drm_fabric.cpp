/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_fabric.h"

namespace NEO {

std::unique_ptr<DrmFabric> DrmFabric::create(Drm &drm) {
    return std::make_unique<DrmFabricStub>();
}

} // namespace NEO

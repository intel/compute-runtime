/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/drm_neo.h"

namespace NEO {

Drm::~Drm() = default;

Drm *Drm::create(std::unique_ptr<HwDeviceId> hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    return nullptr;
}
} // namespace NEO

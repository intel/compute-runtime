/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/os_interface/linux/drm_neo.h"

#include "drm/i915_drm.h"

class DrmWrap : public NEO::Drm {
  public:
    static NEO::Drm *createDrm(RootDeviceEnvironment &rootDeviceEnvironment) {
        auto hwDeviceId = Drm::discoverDevices();
        if (hwDeviceId != nullptr) {
            return NEO::Drm::create(std::move(hwDeviceId), rootDeviceEnvironment);
        }
        return nullptr;
    }
};

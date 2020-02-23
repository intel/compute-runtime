/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

#include "drm/i915_drm.h"

class DrmWrap : public NEO::Drm {
  public:
    static NEO::Drm *createDrm(RootDeviceEnvironment &rootDeviceEnvironment) {
        auto hwDeviceIds = OSInterface::discoverDevices();
        if (!hwDeviceIds.empty()) {
            return NEO::Drm::create(std::move(hwDeviceIds[0]), rootDeviceEnvironment);
        }
        return nullptr;
    }
};

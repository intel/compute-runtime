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
    static NEO::Drm *createDrm(int32_t deviceOrdinal, RootDeviceEnvironment &rootDeviceEnvironment) {
        return NEO::Drm::create(deviceOrdinal, rootDeviceEnvironment);
    }
    static void closeDevice(int32_t deviceOrdinal) {
        NEO::Drm::closeDevice(deviceOrdinal);
    };
};

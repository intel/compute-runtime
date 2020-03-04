/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"

#include "drm/i915_drm.h"

namespace NEO {

class DrmMockDefault : public DrmMock {
  public:
    DrmMockDefault(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {
        StoredRetVal = 0;
        StoredRetValForDeviceID = 0;
        StoredRetValForEUVal = 0;
        StoredRetValForSSVal = 0;
        StoredRetValForDeviceRevID = 0;
        StoredRetValForPooledEU = 0;
        StoredRetValForMinEUinPool = 0;
        setGtType(GTTYPE_GT1);
    }
};

Drm **pDrmToReturnFromCreateFunc = nullptr;

Drm *Drm::create(std::unique_ptr<HwDeviceId> hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    rootDeviceEnvironment.setHwInfo(platformDevices[0]);
    if (pDrmToReturnFromCreateFunc) {
        return *pDrmToReturnFromCreateFunc;
    }
    return new DrmMockDefault(rootDeviceEnvironment);
}
} // namespace NEO

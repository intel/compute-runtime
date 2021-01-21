/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"

#include "drm/i915_drm.h"

namespace NEO {

class DrmMockDefault : public DrmMock {
  public:
    DrmMockDefault(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {
        StoredRetVal = 0;
        StoredRetValForDeviceID = 0;
        StoredRetValForDeviceRevID = 0;
        StoredRetValForPooledEU = 0;
        StoredRetValForMinEUinPool = 0;
        setGtType(GTTYPE_GT1);
    }
};

Drm **pDrmToReturnFromCreateFunc = nullptr;

Drm *Drm::create(std::unique_ptr<HwDeviceId> hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    rootDeviceEnvironment.setHwInfo(defaultHwInfo.get());
    if (pDrmToReturnFromCreateFunc) {
        return *pDrmToReturnFromCreateFunc;
    }
    auto drm = new DrmMockDefault(rootDeviceEnvironment);

    if (drm->isVmBindAvailable() && rootDeviceEnvironment.executionEnvironment.isDebuggingEnabled()) {
        drm->setPerContextVMRequired(true);
    }

    if (!drm->isPerContextVMRequired()) {
        drm->createVirtualMemoryAddressSpace(HwHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()));
    }
    return drm;
}

void Drm::overrideBindSupport(bool &useVmBind) {
    useVmBind = false;
    if (DebugManager.flags.UseVmBind.get() == 1) {
        useVmBind = true;
    }
}

} // namespace NEO

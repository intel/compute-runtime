/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"

namespace NEO {

class DrmMockDefault : public DrmMock {
  public:
    DrmMockDefault(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {
        storedRetVal = 0;
        storedRetValForDeviceID = 0;
        storedRetValForDeviceRevID = 0;
        storedRetValForPooledEU = 0;
        storedRetValForMinEUinPool = 0;

        if (hwDeviceIdIn != nullptr)
            this->hwDeviceId = std::move(hwDeviceIdIn);
    }
};

Drm **pDrmToReturnFromCreateFunc = nullptr;
bool disableBindDefaultInTests = true;

Drm *Drm::create(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    rootDeviceEnvironment.setHwInfo(defaultHwInfo.get());
    if (pDrmToReturnFromCreateFunc) {
        return *pDrmToReturnFromCreateFunc;
    }
    auto drm = new DrmMockDefault(std::move(hwDeviceId), rootDeviceEnvironment);

    const HardwareInfo *hwInfo = rootDeviceEnvironment.getHardwareInfo();

    drm->setupIoctlHelper(hwInfo->platform.eProductFamily);

    drm->queryAdapterBDF();

    drm->queryMemoryInfo();

    drm->queryPageFaultSupport();

    if (drm->isVmBindAvailable() && rootDeviceEnvironment.executionEnvironment.isDebuggingEnabled()) {
        drm->setPerContextVMRequired(true);
    }

    if (!drm->isPerContextVMRequired()) {
        drm->createVirtualMemoryAddressSpace(HwHelper::getSubDevicesCount(hwInfo));
    }

    return drm;
}

void Drm::overrideBindSupport(bool &useVmBind) {
    if (disableBindDefaultInTests) {
        useVmBind = false;
    }
    if (DebugManager.flags.UseVmBind.get() == 1) {
        useVmBind = true;
    }
}

} // namespace NEO

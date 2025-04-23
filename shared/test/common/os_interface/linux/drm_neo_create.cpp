/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test//common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
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
    DebugManagerStateRestore restore;
    debugManager.flags.IgnoreProductSpecificIoctlHelper.set(true);
    rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());
    if (pDrmToReturnFromCreateFunc) {
        return *pDrmToReturnFromCreateFunc;
    }
    auto drm = new DrmMockDefault(std::move(hwDeviceId), rootDeviceEnvironment);

    const HardwareInfo *hwInfo = rootDeviceEnvironment.getHardwareInfo();

    drm->setupIoctlHelper(hwInfo->platform.eProductFamily);

    drm->queryAdapterBDF();

    drm->queryMemoryInfo();

    drm->queryPageFaultSupport();

    if (rootDeviceEnvironment.executionEnvironment.isDebuggingEnabled()) {
        if (drm->getRootDeviceEnvironment().executionEnvironment.getDebuggingMode() == DebuggingMode::offline) {
            drm->setPerContextVMRequired(false);
        } else {
            if (drm->isVmBindAvailable()) {
                drm->setPerContextVMRequired(true);
            }
        }
    }

    if (!drm->isPerContextVMRequired()) {
        drm->createVirtualMemoryAddressSpace(GfxCoreHelper::getSubDevicesCount(hwInfo));
    }

    return drm;
}

void Drm::overrideBindSupport(bool &useVmBind) {
    if (disableBindDefaultInTests) {
        useVmBind = false;
    }
    if (debugManager.flags.UseVmBind.get() == 1) {
        useVmBind = true;
    }
}

bool DrmMemoryManager::isGemCloseWorkerSupported() {
    return ultHwConfig.useGemCloseWorker;
}

} // namespace NEO

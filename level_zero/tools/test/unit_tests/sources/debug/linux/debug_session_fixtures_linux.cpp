/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/linux/debug_session_fixtures_linux.h"

#include "shared/test/common/mocks/mock_device.h"

namespace L0 {
namespace ult {

void DebugApiLinuxFixture::setUp(NEO::HardwareInfo *hwInfo) {
    if (hwInfo != nullptr) {
        auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(hwInfo, 0u);
        DeviceFixture::setupWithExecutionEnvironment(*executionEnvironment);
    } else {
        DeviceFixture::setUp();
    }

    mockDrm = new DrmMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    mockDrm->allowDebugAttach = true;
    mockDrm->queryEngineInfo();

    // set config from HwInfo to have correct topology requested by tests
    if (hwInfo) {
        mockDrm->storedSVal = hwInfo->gtSystemInfo.SliceCount;
        mockDrm->storedSSVal = hwInfo->gtSystemInfo.SubSliceCount;
        mockDrm->storedEUVal = hwInfo->gtSystemInfo.EUCount;
    }
    NEO::DrmQueryTopologyData topologyData = {};
    mockDrm->systemInfoQueried = true;
    mockDrm->queryTopology(neoDevice->getHardwareInfo(), topologyData);
    auto &rootDeviceEnvironment = *neoDevice->executionEnvironment->rootDeviceEnvironments[0];
    auto gtSystemInfo = &rootDeviceEnvironment.getMutableHardwareInfo()->gtSystemInfo;
    for (uint32_t slice = 0; slice < GT_MAX_SLICE; slice++) {
        gtSystemInfo->SliceInfo[slice].Enabled = slice < gtSystemInfo->SliceCount;
    }

    rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface);
    rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
}

} // namespace ult
} // namespace L0

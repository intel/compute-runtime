/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/linux/prelim/debug_session_fixtures_linux.h"

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/test/common/mocks/mock_device.h"

namespace L0 {
namespace ult {

void DebugApiLinuxPrelimFixture::setUp(NEO::HardwareInfo *hwInfo) {
    if (hwInfo != nullptr) {
        auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(hwInfo, 0u);
        DeviceFixture::setupWithExecutionEnvironment(*executionEnvironment);
    } else {
        DeviceFixture::setUp();
    }

    mockDrm = new DrmQueryMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
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

void DebugApiLinuxMultiDeviceFixture::setUp() {
    MultipleDevicesWithCustomHwInfo::setUp();
    neoDevice = driverHandle->devices[0]->getNEODevice();

    L0::Device *device = driverHandle->devices[0];
    deviceImp = static_cast<DeviceImp *>(device);

    mockDrm = new DrmQueryMock(*neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    mockDrm->allowDebugAttach = true;

    // set config from HwInfo to have correct topology requested by tests
    mockDrm->storedSVal = hwInfo.gtSystemInfo.SliceCount;
    mockDrm->storedSSVal = hwInfo.gtSystemInfo.SubSliceCount;
    mockDrm->storedEUVal = hwInfo.gtSystemInfo.EUCount;

    mockDrm->queryEngineInfo();
    auto engineInfo = mockDrm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo->getEngineInstance(1, hwInfo.capabilityTable.defaultEngineType));

    NEO::DrmQueryTopologyData topologyData = {};
    mockDrm->systemInfoQueried = true;
    mockDrm->queryTopology(neoDevice->getHardwareInfo(), topologyData);
    auto &rootDeviceEnvironment = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0];
    auto gtSystemInfo = &rootDeviceEnvironment.getMutableHardwareInfo()->gtSystemInfo;
    for (uint32_t slice = 0; slice < GT_MAX_SLICE; slice++) {
        gtSystemInfo->SliceInfo[slice].Enabled = slice < gtSystemInfo->SliceCount;
    }

    rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface);
    rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
}

TileDebugSessionLinuxi915 *MockDebugSessionLinuxi915::createTileSession(const zet_debug_config_t &config, L0::Device *device, L0::DebugSessionImp *rootDebugSession) {
    auto tileSession = new MockTileDebugSessionLinuxi915(config, device, rootDebugSession);
    tileSession->initialize();
    return tileSession;
}
} // namespace ult
} // namespace L0

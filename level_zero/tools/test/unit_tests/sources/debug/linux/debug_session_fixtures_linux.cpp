/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/linux/debug_session_fixtures_linux.h"

namespace L0 {
namespace ult {

void DebugApiLinuxFixture::setUp(NEO::HardwareInfo *hwInfo) {
    if (hwInfo != nullptr) {
        auto executionEnvironment = MockDevice::prepareExecutionEnvironment(hwInfo, 0u);
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
    NEO::Drm::QueryTopologyData topologyData = {};
    mockDrm->queryTopology(neoDevice->getHardwareInfo(), topologyData);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
}

void DebugApiLinuxMultiDeviceFixture::setUp() {
    MultipleDevicesWithCustomHwInfo::setUp();
    neoDevice = driverHandle->devices[0]->getNEODevice();

    L0::Device *device = driverHandle->devices[0];
    deviceImp = static_cast<DeviceImp *>(device);

    mockDrm = new DrmQueryMock(*neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0], neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getHardwareInfo());
    mockDrm->allowDebugAttach = true;

    // set config from HwInfo to have correct topology requested by tests
    mockDrm->storedSVal = hwInfo.gtSystemInfo.SliceCount;
    mockDrm->storedSSVal = hwInfo.gtSystemInfo.SubSliceCount;
    mockDrm->storedEUVal = hwInfo.gtSystemInfo.EUCount;

    mockDrm->queryEngineInfo();
    auto engineInfo = mockDrm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo->getEngineInstance(1, hwInfo.capabilityTable.defaultEngineType));

    NEO::Drm::QueryTopologyData topologyData = {};
    mockDrm->queryTopology(neoDevice->getHardwareInfo(), topologyData);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
}

TileDebugSessionLinux *MockDebugSessionLinux::createTileSession(const zet_debug_config_t &config, L0::Device *device, L0::DebugSessionImp *rootDebugSession) {
    return new MockTileDebugSessionLinux(config, device, rootDebugSession);
}
} // namespace ult
} // namespace L0

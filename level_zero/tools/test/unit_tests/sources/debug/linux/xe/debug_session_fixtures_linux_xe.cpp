/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/linux/xe/debug_session_fixtures_linux_xe.h"

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/linux/debug_mock_drm_xe.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

namespace L0 {
namespace ult {

void DebugApiLinuxXeFixture::setUp(NEO::HardwareInfo *hwInfo) {
    if (hwInfo != nullptr) {
        auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(hwInfo, 0u);
        DeviceFixture::setupWithExecutionEnvironment(*executionEnvironment);
    } else {
        DeviceFixture::setUp();
    }

    mockDrm = DrmMockXeDebug::create(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]).release();
    mockDrm->allowDebugAttach = true;
    mockDrm->memoryInfoQueried = true;
    mockDrm->queryEngineInfo();

    auto &rootDeviceEnvironment = *neoDevice->executionEnvironment->rootDeviceEnvironments[0];
    auto gtSystemInfo = &rootDeviceEnvironment.getMutableHardwareInfo()->gtSystemInfo;
    for (uint32_t slice = 0; slice < GT_MAX_SLICE; slice++) {
        gtSystemInfo->SliceInfo[slice].Enabled = slice < gtSystemInfo->SliceCount;
    }

    rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface);
    rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));

    mockDrm->setFileDescriptor(SysCalls::fakeFileDescriptor);
    SysCalls::drmVersion = "xe";
}

} // namespace ult
} // namespace L0

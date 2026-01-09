/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/linux/xe/debug_session_fixtures_linux_xe.h"

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/linux/debug_mock_drm_xe.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

extern OpenConnectionUpstreamHelperFunc openConnectionUpstreamFuncXe;

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

    L0::ult::openConnectionUpstreamFuncXe = [this](int pid, L0::Device *device, NEO::EuDebugInterface &debugInterface, NEO::EuDebugConnect *open, int &debugFd) -> ze_result_t {
        if (this->mockDrm->debuggerOpenRetval < 0) {
            debugFd = -1;
            return DebugSessionLinux::translateDebuggerOpenErrno(this->mockDrm->getErrno());
        } else {
            open->version = 1;
            debugFd = 10;
            return ZE_RESULT_SUCCESS;
        }
    };
}

void DebugApiLinuxMultiDeviceFixtureXe::setUp() {
    MultipleDevicesWithCustomHwInfo::setUp();
    neoDevice = driverHandle->devices[0]->getNEODevice();

    l0Device = driverHandle->devices[0];

    mockDrm = DrmMockXeDebug::create(*neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]).release();
    mockDrm->allowDebugAttach = true;
    mockDrm->memoryInfoQueried = true;

    mockDrm->queryEngineInfo();

    auto &rootDeviceEnvironment = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0];
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

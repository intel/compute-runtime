/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/mocks/mock_sysman_device_info.h"

#include "shared/test/common/mocks/mock_device.h"

#include "mock_sysman_env_vars.h"

namespace L0 {
namespace ult {

void SysmanMultiDeviceInfoFixture::SetUp() {
    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }
    hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = numSubDevices;
    hwInfo.gtSystemInfo.MultiTileArchInfo.Tile0 = 1;
    hwInfo.gtSystemInfo.MultiTileArchInfo.Tile1 = 1;
    auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, 0u);

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];
}

} // namespace ult
} // namespace L0

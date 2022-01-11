/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "mock_sysman_env_vars.h"

using namespace NEO;
using ::testing::_;

namespace L0 {
namespace ult {

class SysmanMultiDeviceInfoFixture : public ::testing::Test {
  public:
    void SetUp() {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        hwInfo = *NEO::defaultHwInfo.get();
        hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = 1;
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = numSubDevices;
        hwInfo.gtSystemInfo.MultiTileArchInfo.Tile0 = 1;
        hwInfo.gtSystemInfo.MultiTileArchInfo.Tile1 = 1;
        auto executionEnvironment = MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
        neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, 0u);

        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }
    void TearDown() {}
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::HardwareInfo hwInfo;

    const uint32_t numRootDevices = 1u;
    const uint32_t numSubDevices = 2u;
};

} // namespace ult
} // namespace L0
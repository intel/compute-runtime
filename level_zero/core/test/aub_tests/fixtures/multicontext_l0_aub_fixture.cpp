/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/aub_tests/fixtures/multicontext_l0_aub_fixture.h"

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device_imp.h"

void MulticontextL0AubFixture::setUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool implicitScaling) {
    if (implicitScaling) {
        debugManager.flags.EnableImplicitScaling.set(1);
    }
    MulticontextAubFixture::setUp(numberOfTiles, enabledCommandStreamers, false);

    if (skipped) {
        GTEST_SKIP();
    }
}

CommandStreamReceiver *MulticontextL0AubFixture::getGpgpuCsr(uint32_t tile, uint32_t engine) {
    return subDevices[tile]->getNEODevice()->getEngine(engine).commandStreamReceiver;
}

void MulticontextL0AubFixture::createDevices(const HardwareInfo &hwInfo, uint32_t numTiles) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1u);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();

    auto neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, 0u);

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<L0::ult::Mock<L0::DriverHandleImp>>();

    driverHandle->initialize(std::move(devices));

    rootDevice = driverHandle->devices[0];

    std::vector<ze_device_handle_t> subDevicesH;
    subDevicesH.resize(numTiles);

    EXPECT_EQ(ZE_RESULT_SUCCESS, rootDevice->getSubDevices(&numberOfEnabledTiles, subDevicesH.data()));

    for (uint32_t i = 0; i < numTiles; i++) {
        subDevices.push_back(L0::Device::fromHandle(subDevicesH[i]));
    }
    this->svmAllocsManager = driverHandle->getSvmAllocsManager();
}

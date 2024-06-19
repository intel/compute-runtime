/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace NEO {
std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid, uint32_t adapterNodeOrdinal);
} // namespace NEO

namespace L0 {
namespace ult {

struct LuidDeviceTest : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(LuidDeviceTest, givenLuidDevicePropertiesStructureThenLuidAndNodeMaskSetForWDDMDriverType) {
    NEO::MockDevice *neoDevice = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    L0::Device *device = nullptr;
    WddmMock *luidMock = nullptr;

    auto gdi = new MockGdi();
    auto osEnvironment = new OsEnvironmentWin();
    osEnvironment->gdi.reset(gdi);

    auto mockBuiltIns = new MockBuiltins();
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(NEO::defaultHwInfo.get(), 0u);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());

    executionEnvironment->osEnvironment.reset(osEnvironment);
    auto osInterface = new OSInterface();
    luidMock = new WddmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    LUID adapterLuid = {0x12, 0x1234};
    luidMock->hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 1u);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(luidMock));
    luidMock->init();
    neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), executionEnvironment, 0u);

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_luid_ext_properties_t deviceLuidProperties = {ZE_STRUCTURE_TYPE_DEVICE_LUID_EXT_PROPERTIES};
    deviceProperties.pNext = &deviceLuidProperties;
    ze_result_t result = device->getProperties(&deviceProperties);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    uint64_t luid = 0;
    memcpy_s(&luid, sizeof(uint64_t), &deviceLuidProperties.luid, sizeof(deviceLuidProperties.luid));
    uint32_t lowLUID = luid & 0xFFFFFFFF;
    uint32_t highLUID = ((luid & 0xFFFFFFFF00000000) >> 32);
    EXPECT_EQ(lowLUID, (uint32_t)adapterLuid.LowPart);
    EXPECT_EQ(highLUID, (uint32_t)adapterLuid.HighPart);
    EXPECT_NE(deviceLuidProperties.nodeMask, (uint32_t)0);
}

} // namespace ult
} // namespace L0

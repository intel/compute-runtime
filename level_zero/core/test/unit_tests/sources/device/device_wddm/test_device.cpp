/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/device/root_device.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/mock_hw_info_config_hw.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cache/cache_reservation.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/host_pointer_manager.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

namespace NEO {
std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid);
} // namespace NEO

namespace L0 {
namespace ult {

struct LuidDeviceTest : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(LuidDeviceTest, givenLuidDevicePropertiesStructureThenLuidAndNodeMaskSetForWDDMDriverType) {
    DebugManager.flags.EnableL0ReadLUIDExtension.set(true);
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
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());

    executionEnvironment->osEnvironment.reset(osEnvironment);
    auto osInterface = new OSInterface();
    luidMock = new WddmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    LUID adapterLuid = {0x12, 0x1234};
    luidMock->hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid);
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
    DebugManager.flags.EnableL0ReadLUIDExtension.set(false);
}

} // namespace ult
} // namespace L0
/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/ze_intel_gpu.h"

namespace L0 {
namespace ult {

using DeviceXe2HpgCoreTest = Test<DeviceFixture>;

HWTEST2_F(DeviceXe2HpgCoreTest, whenCallingGetMemoryPropertiesWithNonNullPtrThenPropertiesAreReturned, IsXe2HpgCore) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    ze_device_memory_properties_t memProperties = {};
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    auto hwInfo = *NEO::defaultHwInfo;
    auto &productHelper = device->getProductHelper();
    EXPECT_EQ(memProperties.maxClockRate, productHelper.getDeviceMemoryMaxClkRate(hwInfo, nullptr, 0));
    EXPECT_EQ(memProperties.maxBusWidth, this->neoDevice->getDeviceInfo().addressBits);
    EXPECT_EQ(memProperties.totalSize, this->neoDevice->getDeviceInfo().globalMemSize);
}

HWTEST2_F(DeviceXe2HpgCoreTest, GivenTargetXeHpgCoreWhenGettingDpSupportThenReturnsTrue, IsXe2HpgCore) {
    ze_device_module_properties_t deviceModProps = {ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES};
    ze_intel_device_module_dp_exp_properties_t moduleDpProps = {ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES};
    moduleDpProps.flags = 0u;
    deviceModProps.pNext = &moduleDpProps;

    ze_result_t res = device->getKernelProperties(&deviceModProps);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    bool dp4a = moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DP4A;
    bool dpas = moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DPAS;
    EXPECT_TRUE(dp4a);
    EXPECT_TRUE(dpas);
}

class DeviceCopyQueueGroupXe2HpgCoreFixture : public DeviceFixture {
  public:
    void setUp() {
        debugManager.flags.EnableBlitterOperationsSupport.set(0);
        DeviceFixture::setUp();
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
};

using DeviceCopyQueueGroupXe2HpgCoreTest = Test<DeviceCopyQueueGroupXe2HpgCoreFixture>;

HWTEST2_F(DeviceCopyQueueGroupXe2HpgCoreTest,
          givenBlitterSupportAndEnableBlitterOperationsSupportSetToZeroThenNoCopyEngineIsReturned, IsXe2HpgCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    MockDeviceImp mockDevice(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = mockDevice.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = mockDevice.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (auto &engineGroup : neoMockDevice->getRegularEngineGroups()) {
        EXPECT_NE(NEO::EngineGroupType::copy, engineGroup.engineGroupType);
    }
}

HWTEST2_F(DeviceXe2HpgCoreTest, givenReturnedDevicePropertiesThenExpectedPageFaultSupportReturned, IsXe2HpgCore) {
    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    device->getProperties(&deviceProps);
    EXPECT_NE(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
}
} // namespace ult
} // namespace L0

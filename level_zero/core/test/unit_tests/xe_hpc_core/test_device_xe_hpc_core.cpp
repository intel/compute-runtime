/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_imp.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(AppendMemoryCopy, givenCopyOnlyCommandListAndHostPointersWhenMemoryCopyCalledThenPipeControlWithDcFlushAddedIsNotAddedAfterBlitCopy, IGFX_XE_HPC_CORE);

using DeviceTestXeHpc = Test<DeviceFixture>;

HWTEST2_F(DeviceTestXeHpc, whenCallingGetMemoryPropertiesWithNonNullPtrAndBdRevisionIsNotA0ThenmaxClockRateReturnedIsZero, IsXeHpcCore) {
    uint32_t count = 0;
    auto device = driverHandle->devices[0];
    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usRevId = 0x8; // not BD A0

    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    ze_device_memory_properties_t memProperties = {};
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    EXPECT_EQ(memProperties.maxClockRate, 0u);
}

HWTEST2_F(DeviceTestXeHpc, givenXeHpcAStepAndDebugFlagOverridesWhenCreatingMultiTileDeviceThenExpectImplicitScalingEnabled, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    DebugManager.flags.EnableImplicitScaling.set(1);
    VariableBackup<bool> apiSupportBackup(&NEO::ImplicitScaling::apiSupport, true);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.platform.usRevId = 0x3;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue);
    ASSERT_NE(nullptr, device);

    EXPECT_TRUE(device->isImplicitScalingCapable());

    static_cast<DeviceImp *>(device)->releaseResources();
    delete device;
}

HWTEST2_F(DeviceTestXeHpc, givenXeHpcBStepWhenCreatingMultiTileDeviceThenExpectImplicitScalingEnabled, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> apiSupportBackup(&NEO::ImplicitScaling::apiSupport, true);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.platform.usRevId = 0x6;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue);
    ASSERT_NE(nullptr, device);

    EXPECT_TRUE(device->isImplicitScalingCapable());

    static_cast<DeviceImp *>(device)->releaseResources();
    delete device;
}

using DeviceTestPvc = Test<DeviceFixture>;

HWTEST2_F(DeviceTestPvc, givenPvcAStepWhenCreatingMultiTileDeviceThenExpectImplicitScalingDisabled, IsPVC) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> apiSupportBackup(&NEO::ImplicitScaling::apiSupport, true);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.platform.usRevId = 0x3;

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue);
    ASSERT_NE(nullptr, device);

    EXPECT_FALSE(device->isImplicitScalingCapable());

    static_cast<DeviceImp *>(device)->releaseResources();
    delete device;
}

HWTEST2_F(DeviceTestPvc, whenCallingGetMemoryPropertiesWithNonNullPtrThenPropertiesAreReturned, IsPVC) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    ze_device_memory_properties_t memProperties = {};
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    EXPECT_EQ(memProperties.maxClockRate, 3200u);
    EXPECT_EQ(memProperties.maxBusWidth, this->neoDevice->getDeviceInfo().addressBits);
    EXPECT_EQ(memProperties.totalSize, this->neoDevice->getDeviceInfo().globalMemSize);
}

using MultiDeviceCommandQueueGroupWithNineCopyEnginesTest = Test<SingleRootMultiSubDeviceFixtureWithImplicitScaling<9, 1>>;

HWTEST2_F(MultiDeviceCommandQueueGroupWithNineCopyEnginesTest, givenMainAndLinkCopyEngineSupportAndCCSAndImplicitScalingThenExpectedQueueGroupsAreReturned, IsXeHpcCore) {
    uint32_t count = 0;
    ze_result_t res = deviceImp->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, numEngineGroups + subDeviceNumEngineGroups);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp->getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t numCopyQueues = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if ((properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) &&
                   !(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
            numCopyQueues += properties[i].numQueues;
        }
    }
    EXPECT_EQ(numCopyQueues, expectedCopyEngineCount);
}

HWTEST2_F(MultiDeviceCommandQueueGroupWithNineCopyEnginesTest,
          givenMainAndLinkCopyEngineSupportAndCCSAndImplicitScalingThenCommandListCreatedWithCorrectDevice, IsXeHpcCore) {
    uint32_t count = 0;
    ze_result_t res = deviceImp->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, numEngineGroups + subDeviceNumEngineGroups);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp->getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t numCopyQueues = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if ((properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) &&
                   !(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
            numCopyQueues += properties[i].numQueues;
        }
    }
    EXPECT_EQ(numCopyQueues, expectedCopyEngineCount);

    ze_command_list_handle_t hComputeCommandList{};
    ze_command_list_desc_t computeDesc{};
    computeDesc.commandQueueGroupOrdinal = numEngineGroups - 1;
    res = deviceImp->createCommandList(&computeDesc, &hComputeCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    CommandListImp *computeCommandList = static_cast<CommandListImp *>(CommandList::fromHandle(hComputeCommandList));
    EXPECT_FALSE(computeCommandList->isCopyOnly());

    ze_command_queue_handle_t hCommandQueue{};
    ze_command_queue_desc_t computeCommandQueueDesc{};
    computeCommandQueueDesc.ordinal = computeDesc.commandQueueGroupOrdinal;
    res = device->createCommandQueue(&computeCommandQueueDesc, &hCommandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    CommandQueue *computeCommandQueue = static_cast<CommandQueue *>(CommandQueue::fromHandle(hCommandQueue));
    EXPECT_FALSE(computeCommandQueue->peekIsCopyOnlyCommandQueue());

    ze_command_list_handle_t hCopyCommandList{};
    ze_command_list_desc_t copyDesc{};
    copyDesc.commandQueueGroupOrdinal = numEngineGroups + 1;
    res = deviceImp->createCommandList(&copyDesc, &hCopyCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    CommandListImp *copyCommandList = static_cast<CommandListImp *>(CommandList::fromHandle(hCopyCommandList));
    EXPECT_TRUE(copyCommandList->isCopyOnly());

    computeCommandQueue->destroy();
    computeCommandList->destroy();
    copyCommandList->destroy();
}

HWTEST2_F(MultiDeviceCommandQueueGroupWithNineCopyEnginesTest,
          givenMainAndLinkCopyEngineSupportAndCCSAndImplicitScalingWhenPassingIncorrectIndexThenInvalidArgumentIsReturned, IsXeHpcCore) {
    uint32_t count = 0;
    ze_result_t res = deviceImp->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, numEngineGroups + subDeviceNumEngineGroups);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp->getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t numCopyQueues = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if ((properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) &&
                   !(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
            numCopyQueues += properties[i].numQueues;
        }
    }
    EXPECT_EQ(numCopyQueues, expectedCopyEngineCount);

    ze_command_queue_handle_t hCommandQueue{};
    ze_command_queue_desc_t computeCommandQueueDesc{};
    computeCommandQueueDesc.ordinal = numEngineGroups + 1;
    computeCommandQueueDesc.index = numCopyQueues + 2;
    res = device->createCommandQueue(&computeCommandQueueDesc, &hCommandQueue);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

HWTEST2_F(MultiDeviceCommandQueueGroupWithNineCopyEnginesTest,
          givenMainAndLinkCopyEngineSupportAndCCSAndImplicitScalingThenImmediateCommandListCreatedWithCorrectDevice, IsXeHpcCore) {
    uint32_t count = 0;
    ze_result_t res = deviceImp->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, numEngineGroups + subDeviceNumEngineGroups);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp->getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t numCopyQueues = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if ((properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) &&
                   !(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
            numCopyQueues += properties[i].numQueues;
        }
    }
    EXPECT_EQ(numCopyQueues, expectedCopyEngineCount);

    ze_command_list_handle_t hComputeCommandList{};
    ze_command_queue_desc_t computeDesc{};
    computeDesc.ordinal = numEngineGroups - 1;
    res = deviceImp->createCommandListImmediate(&computeDesc, &hComputeCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    CommandListImp *computeCommandList = static_cast<CommandListImp *>(CommandList::fromHandle(hComputeCommandList));
    EXPECT_FALSE(computeCommandList->isCopyOnly());

    ze_command_list_handle_t hCopyCommandList{};
    ze_command_queue_desc_t copyDesc{};
    copyDesc.ordinal = numEngineGroups + 1;
    res = deviceImp->createCommandListImmediate(&copyDesc, &hCopyCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    CommandListImp *copyCommandList = static_cast<CommandListImp *>(CommandList::fromHandle(hCopyCommandList));
    EXPECT_TRUE(copyCommandList->isCopyOnly());

    computeCommandList->destroy();
    copyCommandList->destroy();
}

HWTEST2_F(MultiDeviceCommandQueueGroupWithNineCopyEnginesTest, givenMainAndLinkCopyEngineSupportAndCCSAndImplicitScalingWhenRequestingFewerGroupsThenExpectedGroupsAreReturned, IsXeHpcCore) {
    uint32_t count = 0;
    ze_result_t res = deviceImp->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, numEngineGroups + subDeviceNumEngineGroups);

    count--;
    std::vector<ze_command_queue_group_properties_t> properties(count);
    deviceImp->getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t numCopyQueues = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            EXPECT_EQ(properties[i].numQueues, 1u);
        } else if ((properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) &&
                   !(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
            numCopyQueues += properties[i].numQueues;
        }
    }
    EXPECT_LE(numCopyQueues, expectedCopyEngineCount);
}

HWTEST2_F(MultiDeviceCommandQueueGroupWithNineCopyEnginesTest, givenMainAndLinkCopyEngineSupportAndCCSAndImplicitScalingWhenRequestingOnlyOneGroupThenOneQueueGroupIsReturned, IsXeHpcCore) {
    uint32_t count = 0;
    ze_result_t res = deviceImp->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, numEngineGroups + subDeviceNumEngineGroups);

    count = 1;
    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp->getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < count; i++) {
        if (properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            EXPECT_EQ(properties[i].numQueues, expectedComputeEngineCount);
        }
    }
}

using MultiDeviceCommandQueueGroupWithNoCopyEnginesTest = Test<SingleRootMultiSubDeviceFixtureWithImplicitScaling<0, 1>>;
HWTEST2_F(MultiDeviceCommandQueueGroupWithNoCopyEnginesTest,
          givenNoCopyEngineSupportAndCCSAndImplicitScalingThenExpectedQueueGroupsAreReturned, IsXeHpcCore) {
    uint32_t count = 0;
    ze_result_t res = deviceImp->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, numEngineGroups);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp->getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < count; i++) {
        if (properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            EXPECT_EQ(properties[i].numQueues, expectedComputeEngineCount);
        }
    }
}

using MultiDeviceCommandQueueGroupWithNoCopyEnginesAndNoImplicitScalingTest = Test<SingleRootMultiSubDeviceFixtureWithImplicitScaling<0, 0>>;
HWTEST2_F(MultiDeviceCommandQueueGroupWithNoCopyEnginesAndNoImplicitScalingTest,
          givenNoCopyEngineSupportAndCCSAndNoImplicitScalingThenOnlyTheQueueGroupsFromSubDeviceAreReturned, IsXeHpcCore) {
    uint32_t count = 0;
    ze_result_t res = deviceImp->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, numEngineGroups);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp->getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (uint32_t i = 0; i < count; i++) {
        if (properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            EXPECT_EQ(properties[i].numQueues, expectedComputeEngineCount);
        }
    }
}

using CommandQueueGroupTest = Test<DeviceFixture>;

HWTEST2_F(CommandQueueGroupTest, givenNoBlitterSupportAndNoCCSThenOneQueueGroupIsReturned, IsXeHpcCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 1u);
}

HWTEST2_F(CommandQueueGroupTest, givenNoBlitterSupportAndCCSThenTwoQueueGroupsAreReturned, IsXeHpcCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 2u);
}

HWTEST2_F(CommandQueueGroupTest, givenBlitterDisabledAndAllBcsSetThenTwoQueueGroupsAreReturned, IsXeHpcCore) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableBlitterOperationsSupport.set(0);
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo.set();
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 2u);
}

class DeviceCopyQueueGroupFixture : public DeviceFixture {
  public:
    void SetUp() {
        DebugManager.flags.EnableBlitterOperationsSupport.set(0);
        DeviceFixture::SetUp();
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

using DeviceCopyQueueGroupTest = Test<DeviceCopyQueueGroupFixture>;

HWTEST2_F(DeviceCopyQueueGroupTest,
          givenBlitterSupportAndEnableBlitterOperationsSupportSetToZeroThenNoCopyEngineIsReturned, IsXeHpcCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo,
                                                                                              rootDeviceIndex);
    Mock<L0::DeviceImp> deviceImp(neoMockDevice, neoMockDevice->getExecutionEnvironment());

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (auto &engineGroup : neoMockDevice->getRegularEngineGroups()) {
        EXPECT_NE(NEO::EngineGroupType::Copy, engineGroup.engineGroupType);
    }
}

HWTEST2_F(DeviceTestXeHpc, givenReturnedDevicePropertiesThenExpectedPropertyFlagsSet, IsXeHpcCore) {
    ze_device_properties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    device->getProperties(&deviceProps);
    EXPECT_EQ(ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_ECC);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
    EXPECT_EQ(0u, deviceProps.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
}

HWTEST2_F(DeviceTestXeHpc, GivenPvcWhenGettingPhysicalEuSimdWidthThenReturn16, IsPVC) {
    ze_device_properties_t properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device->getProperties(&properties);
    EXPECT_EQ(16u, properties.physicalEUSimdWidth);
}

} // namespace ult
} // namespace L0

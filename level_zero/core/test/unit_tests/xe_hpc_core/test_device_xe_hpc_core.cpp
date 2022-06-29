/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_imp.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(AppendMemoryCopy, givenCopyOnlyCommandListAndHostPointersWhenMemoryCopyCalledThenPipeControlWithDcFlushAddedIsNotAddedAfterBlitCopy, IGFX_XE_HPC_CORE);

using DeviceTestXeHpc = Test<DeviceFixture>;

HWTEST2_F(DeviceTestXeHpc, WhenGettingImagePropertiesThenPropertiesAreNotSet, IsXeHpcCore) {
    ze_image_properties_t properties{};

    ze_image_desc_t desc = {};

    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.format.x = desc.format.y = desc.format.z = desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_R;
    desc.width = 10;
    desc.height = 10;
    desc.depth = 10;

    auto result = device->imageGetProperties(&desc, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(properties.samplerFilterFlags, 0u);
}

HWTEST2_F(DeviceTestXeHpc, givenXeHpcAStepAndDebugFlagOverridesWhenCreatingMultiTileDeviceThenExpectImplicitScalingEnabled, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    DebugManager.flags.EnableImplicitScaling.set(1);
    VariableBackup<bool> apiSupportBackup(&NEO::ImplicitScaling::apiSupport, true);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);

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
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue);
    ASSERT_NE(nullptr, device);

    EXPECT_TRUE(device->isImplicitScalingCapable());

    static_cast<DeviceImp *>(device)->releaseResources();
    delete device;
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

class DeviceCopyQueueGroupXeHpcFixture : public DeviceFixture {
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
using DeviceCopyQueueGroupXeHpcTest = Test<DeviceCopyQueueGroupXeHpcFixture>;

HWTEST2_F(DeviceCopyQueueGroupXeHpcTest,
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

} // namespace ult
} // namespace L0

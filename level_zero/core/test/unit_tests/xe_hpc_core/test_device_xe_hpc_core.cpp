/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_imp.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/ze_intel_gpu.h"

namespace L0 {
namespace ult {

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
    debugManager.flags.CreateMultipleSubDevices.set(2);
    debugManager.flags.EnableImplicitScaling.set(1);
    VariableBackup<bool> apiSupportBackup(&NEO::ImplicitScaling::apiSupport, true);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    NEO::MockExecutionEnvironment mockExecutionEnvironment;
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue);
    ASSERT_NE(nullptr, device);

    EXPECT_TRUE(device->isImplicitScalingCapable());

    static_cast<DeviceImp *>(device)->releaseResources();
    delete device;
}

HWTEST2_F(DeviceTestXeHpc, givenXeHpcBStepWhenCreatingMultiTileDeviceThenExpectImplicitScalingEnabled, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> apiSupportBackup(&NEO::ImplicitScaling::apiSupport, true);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);
    auto hwInfo = *NEO::defaultHwInfo;
    NEO::MockExecutionEnvironment mockExecutionEnvironment;
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue);
    ASSERT_NE(nullptr, device);

    EXPECT_TRUE(device->isImplicitScalingCapable());

    static_cast<DeviceImp *>(device)->releaseResources();
    delete device;
}

HWTEST2_F(DeviceTestXeHpc, GivenTargetXeHPCWhenGettingDpSupportThenReturnsTrue, IsXeHpcCore) {
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
    EXPECT_FALSE(computeCommandList->isCopyOnly(false));

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
    EXPECT_TRUE(copyCommandList->isCopyOnly(false));

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

HWTEST2_F(MultiDeviceCommandQueueGroupWithNineCopyEnginesTest, givenDebugFlagWithLinkedEngineSetWhenCreatingCommandQueueThenOverrideEngineIndex, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    const uint32_t newIndex = 2;
    debugManager.flags.ForceBcsEngineIndex.set(newIndex);

    auto &engineGroups = static_cast<MockDeviceImp *>(deviceImp)->subDeviceCopyEngineGroups;

    uint32_t expectedCopyOrdinal = 0;
    for (uint32_t i = 0; i < engineGroups.size(); i++) {
        if (engineGroups[i].engineGroupType == EngineGroupType::linkedCopy) {
            expectedCopyOrdinal = i;
            break;
        }
    }

    for (uint32_t ordinal = 0; ordinal < engineGroups.size(); ordinal++) {
        for (uint32_t index = 0; index < engineGroups[ordinal].engines.size(); index++) {
            ze_command_queue_handle_t commandQueue = {};

            ze_command_queue_desc_t desc = {};
            desc.ordinal = ordinal + 1;
            desc.index = index;
            ze_result_t res = context->createCommandQueue(deviceImp, &desc, &commandQueue);

            EXPECT_EQ(ZE_RESULT_SUCCESS, res);
            EXPECT_NE(nullptr, commandQueue);

            auto queue = whiteboxCast(L0::CommandQueue::fromHandle(commandQueue));

            EXPECT_EQ(engineGroups[expectedCopyOrdinal].engines[newIndex - 1].commandStreamReceiver, queue->csr);

            queue->destroy();
        }
    }
}

HWTEST2_F(MultiDeviceCommandQueueGroupWithNineCopyEnginesTest, givenDebugFlagWithInvalidIndexSetWhenCreatingCommandQueueThenReturnError, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    const uint32_t newIndex = 999;
    debugManager.flags.ForceBcsEngineIndex.set(newIndex);

    auto &engineGroups = static_cast<MockDeviceImp *>(deviceImp)->subDeviceCopyEngineGroups;

    uint32_t expectedCopyOrdinal = 0;
    for (uint32_t i = 0; i < engineGroups.size(); i++) {
        if (engineGroups[i].engineGroupType == EngineGroupType::linkedCopy) {
            expectedCopyOrdinal = i;
            break;
        }
    }

    ze_command_queue_handle_t commandQueue = {};

    ze_command_queue_desc_t desc = {};
    desc.ordinal = expectedCopyOrdinal + 1;
    desc.index = 0;
    ze_result_t res = context->createCommandQueue(deviceImp, &desc, &commandQueue);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);
}

HWTEST2_F(MultiDeviceCommandQueueGroupWithNineCopyEnginesTest, givenDebugFlagWithMainEngineSetWhenCreatingCommandQueueThenOverrideEngineIndex, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    const uint32_t newIndex = 0;
    debugManager.flags.ForceBcsEngineIndex.set(newIndex);

    auto &engineGroups = static_cast<MockDeviceImp *>(deviceImp)->subDeviceCopyEngineGroups;

    uint32_t expectedCopyOrdinal = 0;
    for (uint32_t i = 0; i < engineGroups.size(); i++) {
        if (engineGroups[i].engineGroupType == EngineGroupType::copy) {
            expectedCopyOrdinal = i;
            break;
        }
    }

    for (uint32_t ordinal = 0; ordinal < engineGroups.size(); ordinal++) {
        for (uint32_t index = 0; index < engineGroups[ordinal].engines.size(); index++) {
            ze_command_queue_handle_t commandQueue = {};

            ze_command_queue_desc_t desc = {};
            desc.ordinal = ordinal + 1;
            desc.index = index;
            ze_result_t res = context->createCommandQueue(deviceImp, &desc, &commandQueue);

            EXPECT_EQ(ZE_RESULT_SUCCESS, res);
            EXPECT_NE(nullptr, commandQueue);

            auto queue = whiteboxCast(L0::CommandQueue::fromHandle(commandQueue));

            EXPECT_EQ(engineGroups[expectedCopyOrdinal].engines[newIndex].commandStreamReceiver, queue->csr);

            queue->destroy();
        }
    }
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
    EXPECT_FALSE(computeCommandList->isCopyOnly(false));

    ze_command_list_handle_t hCopyCommandList{};
    ze_command_queue_desc_t copyDesc{};
    copyDesc.ordinal = numEngineGroups + 1;
    res = deviceImp->createCommandListImmediate(&copyDesc, &hCopyCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    CommandListImp *copyCommandList = static_cast<CommandListImp *>(CommandList::fromHandle(hCopyCommandList));
    EXPECT_TRUE(copyCommandList->isCopyOnly(false));

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
    MockDeviceImp deviceImp(neoMockDevice);

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
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 2u);
}

HWTEST2_F(CommandQueueGroupTest, givenBlitterDisabledAndAllBcsSetThenTwoQueueGroupsAreReturned, IsXeHpcCore) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableBlitterOperationsSupport.set(0);
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo.set();
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 2u);
}

class DeviceCopyQueueGroupXeHpcFixture : public DeviceFixture {
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
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    for (auto &engineGroup : neoMockDevice->getRegularEngineGroups()) {
        EXPECT_NE(NEO::EngineGroupType::copy, engineGroup.engineGroupType);
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

using CommandQueueGroupTest = Test<DeviceFixture>;

HWTEST2_F(CommandQueueGroupTest, givenBlitterSupportWithBcsVirtualEnginesEnabledThenOneByteFillPatternReturned, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseDrmVirtualEnginesForBcs.set(1);
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set();
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 4u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        }
    }
}

HWTEST2_F(CommandQueueGroupTest, givenBlitterSupportWithBcsVirtualEnginesDisabledThenCorrectFillPatternReturned, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseDrmVirtualEnginesForBcs.set(0);
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set();
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 4u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, 4 * sizeof(uint32_t));
        }
    }
}

HWTEST2_F(CommandQueueGroupTest, givenBlitterSupportAndCCSThenFourQueueGroupsAreReturned, IsXeHpcCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set();
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 4u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            uint32_t numerOfCCSEnabled = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
            EXPECT_EQ(properties[i].numQueues, numerOfCCSEnabled);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::linkedCopy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, hwInfo.featureTable.ftrBcsInfo.count() - 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        }
    }
}

HWTEST2_F(CommandQueueGroupTest, givenBlitterSupportCCSAndLinkedBcsDisabledThenThreeQueueGroupsAreReturned, IsXeHpcCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 3u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            uint32_t numerOfCCSEnabled = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
            EXPECT_EQ(properties[i].numQueues, numerOfCCSEnabled);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        }
    }
}

class CommandQueueGroupTestXeHpc : public DeviceFixture, public testing::TestWithParam<uint32_t> {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
};

HWTEST2_P(CommandQueueGroupTestXeHpc, givenVaryingBlitterSupportAndCCSThenBCSGroupContainsCorrectNumberOfEngines, IsXeHpcCore) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(2);
    hwInfo.featureTable.ftrBcsInfo.set(GetParam());
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp deviceImp(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = deviceImp.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 3u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = deviceImp.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            uint32_t numerOfCCSEnabled = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
            EXPECT_EQ(properties[i].numQueues, numerOfCCSEnabled);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::linkedCopy) {
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, hwInfo.featureTable.ftrBcsInfo.count() - 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    CommandQueueGroupTestXeHpcValues,
    CommandQueueGroupTestXeHpc,
    testing::Values(0, 1, 2, 3));

} // namespace ult
} // namespace L0

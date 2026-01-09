/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using CommandQueueGroupTestCRI = Test<DeviceFixture>;

CRITEST_F(CommandQueueGroupTestCRI, givenBlitterSupportAndCCSThenTwoQueueGroupsAreReturned) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set();
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp mockDevice(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = mockDevice.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 2u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = mockDevice.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    bool renderComputeFound = false;
    bool computeFound = false;
    bool copyFound = false;

    auto &gfxCoreHelper = mockDevice.getGfxCoreHelper();
    const auto regularBcsCount = static_cast<uint32_t>(gfxCoreHelper.areSecondaryContextsSupported() ? 7   // BCS1-BCS7, BCS8 used as HP
                                                                                                     : 8); // BCS1-BCS8

    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute) {
            renderComputeFound = true;
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
            computeFound = true;
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            uint32_t numerOfCCSEnabled = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
            EXPECT_EQ(properties[i].numQueues, numerOfCCSEnabled);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) { // BCS0 ignored, rest of BCS engines grouped in copy
            copyFound = true;
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_EQ(properties[i].numQueues, regularBcsCount);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, sizeof(uint8_t));
        }
    }
    EXPECT_FALSE(renderComputeFound);
    EXPECT_TRUE(computeFound);
    EXPECT_TRUE(copyFound);
}

CRITEST_F(CommandQueueGroupTestCRI, givenBlitterSupportCCSAndLinkedBcsDisabledThenOneQueueGroupIsReturned) {
    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    auto *neoMockDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex);
    MockDeviceImp mockDevice(neoMockDevice);

    uint32_t count = 0;
    ze_result_t res = mockDevice.getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GE(count, 1u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = mockDevice.getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto &engineGroups = neoMockDevice->getRegularEngineGroups();
    bool renderComputeFound = false;
    bool computeFound = false;

    for (uint32_t i = 0; i < count; i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute) {
            renderComputeFound = true;
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS);
            EXPECT_EQ(properties[i].numQueues, 1u);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        } else if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
            computeFound = true;
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY);
            EXPECT_TRUE(properties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS);
            uint32_t numerOfCCSEnabled = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
            EXPECT_EQ(properties[i].numQueues, numerOfCCSEnabled);
            EXPECT_EQ(properties[i].maxMemoryFillPatternSize, std::numeric_limits<size_t>::max());
        }
    }
    EXPECT_FALSE(renderComputeFound);
    EXPECT_TRUE(computeFound);
}

using CriKernelSetupTests = ::testing::Test;
CRITEST_F(CriKernelSetupTests, givenParamsWhenFlagEnable64BitAddressingSetAndCallSetupGroupSizeThenNumThreadsPerThreadGroupAreCorrectly) {
    DebugManagerStateRestore restore;
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());

    debugManager.flags.Enable64BitAddressing.set(1);
    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 160u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<uint32_t, 2> values = {
            {32u, 32u}, // SIMT Size,  Max Num of threads
        };

        auto &[simtSize, expectedNumThreadsPerThreadGroup] = values;
        kernel.descriptor.kernelAttributes.simdSize = simtSize;
        kernel.setGroupSize(1024u, 1024u, 1024u);
        EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.privateState.numThreadsPerThreadGroup);
        kernel.privateState.groupSize[0] = kernel.privateState.groupSize[1] = kernel.privateState.groupSize[2] = 0;
    }
    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 512u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 32u}, // SIMT Size, Max Num of threads
            {32u, 32u},
        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.privateState.numThreadsPerThreadGroup);
            kernel.privateState.groupSize[0] = kernel.privateState.groupSize[1] = kernel.privateState.groupSize[2] = 0;
        }
    }
}

} // namespace ult
} // namespace L0

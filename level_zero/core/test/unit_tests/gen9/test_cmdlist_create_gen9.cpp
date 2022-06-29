/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/string.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include <vector>

namespace L0 {
namespace ult {
class CommandListCreateGen9 : public DeviceFixture, public testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();

        dispatchFunctionArguments.groupCountX = 1u;
        dispatchFunctionArguments.groupCountY = 2u;
        dispatchFunctionArguments.groupCountZ = 3u;
    }

    void TearDown() override {
        if (buffer) {
            alignedFree(buffer);
        }
        for (auto ptr : isaBuffers) {
            free(ptr);
        }

        DeviceFixture::TearDown();
    }

    std::vector<void *> isaBuffers;
    ze_group_count_t dispatchFunctionArguments;
    void *buffer = nullptr;

    void initializeFunction(WhiteBox<::L0::Kernel> &function,
                            WhiteBox<::L0::KernelImmutableData> &functionData,
                            L0::Device *device) {

        uint32_t isaSize = 4096;
        void *isaBuffer = malloc(isaSize);
        isaBuffers.push_back(isaBuffer);

        functionData.device = device;
        if (!buffer) {
            buffer = alignedMalloc(isaSize, 64);
        }
        auto allocation = new NEO::GraphicsAllocation(0,
                                                      NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                                      buffer,
                                                      reinterpret_cast<uint64_t>(buffer),
                                                      0,
                                                      isaSize,
                                                      MemoryPool::System4KBPages,
                                                      MemoryManager::maxOsContextCount);
        if (isaBuffer != nullptr) {
            memcpy_s(allocation->getUnderlyingBuffer(), allocation->getUnderlyingBufferSize(), isaBuffer, isaSize);
        }
        functionData.isaGraphicsAllocation.reset(allocation);

        uint32_t crossThreadDataSize = 128;

        function.crossThreadData.reset(new uint8_t[crossThreadDataSize]);
        function.crossThreadDataSize = crossThreadDataSize;

        uint32_t perThreadDataSize = 128;

        function.perThreadDataForWholeThreadGroup = static_cast<uint8_t *>(alignedMalloc(perThreadDataSize, 32));
        function.perThreadDataSize = perThreadDataSize;

        function.kernelImmData = &functionData;
    }
    void cleanupFunction(WhiteBox<::L0::KernelImmutableData> &functionData) {
        functionData.isaGraphicsAllocation.reset(nullptr);
    }
};

GEN9TEST_F(CommandListCreateGen9, WhenGettingCommandListPreemptionModeThenMatchesDevicePreemptionMode) {
    ze_result_t returnValue;
    auto commandList = whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(device->getDevicePreemptionMode(), commandList->getCommandListPreemptionMode());

    delete commandList;
}

GEN9TEST_F(CommandListCreateGen9, GivenDisabledMidThreadPreemptionWhenLaunchingKernelThenThreadGroupModeSet) {
    WhiteBox<::L0::KernelImmutableData> funcInfoThreadGroupData = {};
    NEO::KernelDescriptor kernelDescriptor;
    funcInfoThreadGroupData.kernelDescriptor = &kernelDescriptor;
    WhiteBox<::L0::Kernel> functionThreadGroup;

    funcInfoThreadGroupData.kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = 1;

    initializeFunction(functionThreadGroup, funcInfoThreadGroupData, device);

    ze_result_t returnValue;
    auto commandList = whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    EXPECT_EQ(NEO::PreemptionMode::MidThread, commandList->getCommandListPreemptionMode());

    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(functionThreadGroup.toHandle(),
                                    &dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, commandList->getCommandListPreemptionMode());

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, commandList->getCommandListPreemptionMode());
    cleanupFunction(funcInfoThreadGroupData);
    delete commandList;
}

GEN9TEST_F(CommandListCreateGen9, GivenUsesFencesForReadWriteImagesWhenLaunchingKernelThenMidBatchModeSet) {
    WhiteBox<::L0::KernelImmutableData> funcInfoMidBatchData = {};
    NEO::KernelDescriptor kernelDescriptor;
    funcInfoMidBatchData.kernelDescriptor = &kernelDescriptor;
    WhiteBox<::L0::Kernel> functionMidBatch;

    funcInfoMidBatchData.kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = 1;
    funcInfoMidBatchData.kernelDescriptor->kernelAttributes.flags.usesFencesForReadWriteImages = 1;

    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waDisableLSQCROPERFforOCL = true;

    initializeFunction(functionMidBatch, funcInfoMidBatchData, device);

    ze_result_t returnValue;
    auto commandList = whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    EXPECT_EQ(NEO::PreemptionMode::MidThread, commandList->getCommandListPreemptionMode());

    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(functionMidBatch.toHandle(),
                                    &dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(NEO::PreemptionMode::MidBatch, commandList->getCommandListPreemptionMode());

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::PreemptionMode::MidBatch, commandList->getCommandListPreemptionMode());
    cleanupFunction(funcInfoMidBatchData);
    delete commandList;
}

GEN9TEST_F(CommandListCreateGen9, WhenCommandListHasLowerPreemptionLevelThenDoNotIncreaseAgain) {
    WhiteBox<::L0::KernelImmutableData> funcInfoThreadGroupData = {};
    NEO::KernelDescriptor kernelDescriptor;
    funcInfoThreadGroupData.kernelDescriptor = &kernelDescriptor;
    WhiteBox<::L0::Kernel> functionThreadGroup;

    funcInfoThreadGroupData.kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = 1;

    initializeFunction(functionThreadGroup, funcInfoThreadGroupData, device);

    WhiteBox<::L0::KernelImmutableData> funcInfoMidThreadData = {};
    NEO::KernelDescriptor kernelDescriptor2;
    funcInfoMidThreadData.kernelDescriptor = &kernelDescriptor2;

    WhiteBox<::L0::Kernel> functionMidThread;

    initializeFunction(functionMidThread, funcInfoMidThreadData, device);

    ze_result_t returnValue;
    auto commandList = whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    EXPECT_EQ(NEO::PreemptionMode::MidThread, commandList->getCommandListPreemptionMode());

    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(functionThreadGroup.toHandle(),
                                    &dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, commandList->getCommandListPreemptionMode());

    commandList->appendLaunchKernel(functionMidThread.toHandle(),
                                    &dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, commandList->getCommandListPreemptionMode());

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, commandList->getCommandListPreemptionMode());
    cleanupFunction(funcInfoThreadGroupData);
    cleanupFunction(funcInfoMidThreadData);
    delete commandList;
}
} // namespace ult
} // namespace L0

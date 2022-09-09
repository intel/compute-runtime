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
        DeviceFixture::setUp();

        dispatchKernelArguments.groupCountX = 1u;
        dispatchKernelArguments.groupCountY = 2u;
        dispatchKernelArguments.groupCountZ = 3u;
    }

    void TearDown() override {
        if (buffer) {
            alignedFree(buffer);
        }
        for (auto ptr : isaBuffers) {
            free(ptr);
        }

        DeviceFixture::tearDown();
    }

    std::vector<void *> isaBuffers;
    ze_group_count_t dispatchKernelArguments;
    void *buffer = nullptr;

    void initializeKernel(WhiteBox<::L0::Kernel> &kernel,
                          WhiteBox<::L0::KernelImmutableData> &kernelData,
                          L0::Device *device) {

        uint32_t isaSize = 4096;
        void *isaBuffer = malloc(isaSize);
        isaBuffers.push_back(isaBuffer);

        kernelData.device = device;
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
        kernelData.isaGraphicsAllocation.reset(allocation);

        uint32_t crossThreadDataSize = 128;

        kernel.crossThreadData.reset(new uint8_t[crossThreadDataSize]);
        kernel.crossThreadDataSize = crossThreadDataSize;

        uint32_t perThreadDataSize = 128;

        kernel.perThreadDataForWholeThreadGroup = static_cast<uint8_t *>(alignedMalloc(perThreadDataSize, 32));
        kernel.perThreadDataSize = perThreadDataSize;

        kernel.kernelImmData = &kernelData;
    }
    void cleanupKernel(WhiteBox<::L0::KernelImmutableData> &kernelData) {
        kernelData.isaGraphicsAllocation.reset(nullptr);
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
    WhiteBox<::L0::KernelImmutableData> kernelInfoThreadGroupData = {};
    NEO::KernelDescriptor kernelDescriptor;
    kernelInfoThreadGroupData.kernelDescriptor = &kernelDescriptor;
    WhiteBox<::L0::Kernel> kernelThreadGroup;

    kernelInfoThreadGroupData.kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = 1;

    initializeKernel(kernelThreadGroup, kernelInfoThreadGroupData, device);

    ze_result_t returnValue;
    auto commandList = whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    EXPECT_EQ(NEO::PreemptionMode::MidThread, commandList->getCommandListPreemptionMode());

    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernelThreadGroup.toHandle(),
                                    &dispatchKernelArguments, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, commandList->getCommandListPreemptionMode());

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, commandList->getCommandListPreemptionMode());
    cleanupKernel(kernelInfoThreadGroupData);
    delete commandList;
}

GEN9TEST_F(CommandListCreateGen9, GivenUsesFencesForReadWriteImagesWhenLaunchingKernelThenMidBatchModeSet) {
    WhiteBox<::L0::KernelImmutableData> kernelInfoMidBatchData = {};
    NEO::KernelDescriptor kernelDescriptor;
    kernelInfoMidBatchData.kernelDescriptor = &kernelDescriptor;
    WhiteBox<::L0::Kernel> kernelMidBatch;

    kernelInfoMidBatchData.kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = 1;
    kernelInfoMidBatchData.kernelDescriptor->kernelAttributes.flags.usesFencesForReadWriteImages = 1;

    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waDisableLSQCROPERFforOCL = true;

    initializeKernel(kernelMidBatch, kernelInfoMidBatchData, device);

    ze_result_t returnValue;
    auto commandList = whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    EXPECT_EQ(NEO::PreemptionMode::MidThread, commandList->getCommandListPreemptionMode());

    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernelMidBatch.toHandle(),
                                    &dispatchKernelArguments, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(NEO::PreemptionMode::MidBatch, commandList->getCommandListPreemptionMode());

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::PreemptionMode::MidBatch, commandList->getCommandListPreemptionMode());
    cleanupKernel(kernelInfoMidBatchData);
    delete commandList;
}

GEN9TEST_F(CommandListCreateGen9, WhenCommandListHasLowerPreemptionLevelThenDoNotIncreaseAgain) {
    WhiteBox<::L0::KernelImmutableData> kernelInfoThreadGroupData = {};
    NEO::KernelDescriptor kernelDescriptor;
    kernelInfoThreadGroupData.kernelDescriptor = &kernelDescriptor;
    WhiteBox<::L0::Kernel> kernelThreadGroup;

    kernelInfoThreadGroupData.kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption = 1;

    initializeKernel(kernelThreadGroup, kernelInfoThreadGroupData, device);

    WhiteBox<::L0::KernelImmutableData> kernelInfoMidThreadData = {};
    NEO::KernelDescriptor kernelDescriptor2;
    kernelInfoMidThreadData.kernelDescriptor = &kernelDescriptor2;

    WhiteBox<::L0::Kernel> kernelMidThread;

    initializeKernel(kernelMidThread, kernelInfoMidThreadData, device);

    ze_result_t returnValue;
    auto commandList = whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    EXPECT_EQ(NEO::PreemptionMode::MidThread, commandList->getCommandListPreemptionMode());

    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernelThreadGroup.toHandle(),
                                    &dispatchKernelArguments, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, commandList->getCommandListPreemptionMode());

    commandList->appendLaunchKernel(kernelMidThread.toHandle(),
                                    &dispatchKernelArguments, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, commandList->getCommandListPreemptionMode());

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::PreemptionMode::ThreadGroup, commandList->getCommandListPreemptionMode());
    cleanupKernel(kernelInfoThreadGroupData);
    cleanupKernel(kernelInfoMidThreadData);
    delete commandList;
}
} // namespace ult
} // namespace L0

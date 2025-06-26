/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_kernel.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_kernel_group.h"

namespace L0 {
namespace ult {

struct MutableKernelGroupFixture : public MutableCommandListFixture<false> {
    using AlignedUniquePtr = std::unique_ptr<void, std::function<decltype(alignedFree)>>;

    void createMutableKernelGroup() {
        mutableKernels.clear();

        workingMutableKernelGroup = std::make_unique<::L0::MCL::MutableKernelGroup>(2u,
                                                                                    kernelMutationGroup,
                                                                                    mutableCommandList->inlineDataSize,
                                                                                    mutableCommandList->maxPerThreadDataSize);
        mockMutableKernelGroup = static_cast<MockMutableKernelGroup *>(workingMutableKernelGroup.get());

        for (auto &mutableKernel : mockMutableKernelGroup->getKernelsInGroup()) {
            mutableKernels.push_back(static_cast<MockMutableKernel *>(mutableKernel.get()));
        }
    }

    template <typename FamilyType, typename WalkerType>
    void createMutableComputeWalker() {
        auto walkerCmdSize = L0::MCL::MutableComputeWalkerHw<FamilyType>::getCommandSize();
        auto cpuWalkerBuffer = L0::MCL::MutableComputeWalkerHw<FamilyType>::createCommandBuffer();

        this->cmdBuffer = AlignedUniquePtr(alignedMalloc(walkerCmdSize, sizeof(uint32_t)), alignedFree);

        auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
        *reinterpret_cast<WalkerType *>(cpuWalkerBuffer) = walkerTemplate;
        *reinterpret_cast<WalkerType *>(this->cmdBuffer.get()) = walkerTemplate;

        mutableComputeWalker = std::make_unique<L0::MCL::MutableComputeWalkerHw<FamilyType>>(reinterpret_cast<void *>(this->cmdBuffer.get()),
                                                                                             0,
                                                                                             8,
                                                                                             cpuWalkerBuffer,
                                                                                             true);
    }

    std::vector<MockMutableKernel *> mutableKernels;
    std::unique_ptr<::L0::MCL::MutableKernelGroup> workingMutableKernelGroup;
    AlignedUniquePtr cmdBuffer;
    std::unique_ptr<L0::MCL::MutableComputeWalker> mutableComputeWalker;
    MockMutableKernelGroup *mockMutableKernelGroup = nullptr;
};

using MutableKernelGroupTest = Test<MutableKernelGroupFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableKernelGroupTest,
            givenKernelsWithScratchPropertiesWhenGroupQueryForScratchThenReturnCorrectValue) {
    createMutableKernelGroup();
    EXPECT_FALSE(mockMutableKernelGroup->isScratchNeeded());

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x100;
    createMutableKernelGroup();
    EXPECT_TRUE(mockMutableKernelGroup->isScratchNeeded());
    EXPECT_EQ(0x100u, mockMutableKernelGroup->getMaxAppendScratchSize(0));

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x0;
    mockKernelImmData2->kernelDescriptor->kernelAttributes.perThreadScratchSize[1] = 0x200;
    createMutableKernelGroup();
    EXPECT_TRUE(mockMutableKernelGroup->isScratchNeeded());
    EXPECT_EQ(0x200u, mockMutableKernelGroup->getMaxAppendScratchSize(1));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableKernelGroupTest,
            givenKernelsWithDifferentPayloadSizeWhenGroupQueryForHeapSizeThenReturnMaxValue) {
    constexpr uint32_t kernel2CrossThreadInitSize = 2 * crossThreadInitSize;

    mockKernelImmData2->kernelDescriptor->kernelAttributes.crossThreadDataSize = kernel2CrossThreadInitSize;
    mockKernelImmData2->crossThreadDataSize = kernel2CrossThreadInitSize;
    mockKernelImmData2->crossThreadDataTemplate.reset(new uint8_t[kernel2CrossThreadInitSize]);
    kernel2->crossThreadDataSize = kernel2CrossThreadInitSize;
    kernel2->crossThreadData.reset(new uint8_t[kernel2CrossThreadInitSize]);

    createMutableKernelGroup();

    EXPECT_EQ(kernel2CrossThreadInitSize, mockMutableKernelGroup->getMaxAppendIndirectHeapSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableKernelGroupTest,
            givenMutableKernelHasPassInlineDataWhenCreateHostViewIndirectThenInitializeAndCopyCorrectly) {
    using WalkerType = typename FamilyType::PorWalkerType;

    mutableCommandIdDesc.flags = kernelIsaMutationFlags;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernelMutationGroup, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel2Data = mutableCommandList->kernelData[1].get();
    auto kdPtr = std::make_unique<L0::MCL::KernelDispatch>();
    auto kernelDispatch = kdPtr.get();

    mockKernelImmData2->kernelDescriptor->kernelAttributes.flags.passInlineData = true;
    auto crossThreadDataSize = kernel2->getCrossThreadDataSize();

    createMutableComputeWalker<FamilyType, WalkerType>();
    createMutableKernelGroup();
    mutableKernels[1]->setComputeWalker(mutableComputeWalker.get());
    mutableKernels[1]->setKernelDispatch(kernelDispatch);
    auto expectedCrossThreadSize = crossThreadDataSize - mutableKernels[1]->inlineDataSize;
    kernelDispatch->kernelData = kernel2Data;
    kernelDispatch->offsets.perThreadOffset = expectedCrossThreadSize;
    kernel2->perThreadDataSizeForWholeThreadGroup = 0x40;
    kernel2->perThreadDataForWholeThreadGroup = static_cast<uint8_t *>(alignedMalloc(kernel2->perThreadDataSizeForWholeThreadGroup, 32));

    mutableKernels[1]->createHostViewIndirectData(true);
    auto actualCrossThreadDataSize = mutableKernels[1]->getHostViewIndirectData()->getCrossThreadDataSize();

    EXPECT_EQ(expectedCrossThreadSize, actualCrossThreadDataSize);
}

} // namespace ult
} // namespace L0

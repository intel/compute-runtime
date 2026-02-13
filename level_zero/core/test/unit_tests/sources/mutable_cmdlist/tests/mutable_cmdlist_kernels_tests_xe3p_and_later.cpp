/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"

namespace L0 {
namespace ult {

using MutableCommandListKernelTest = Test<MutableCommandListFixture<false>>;
using MutableCommandListInOrderTest = Test<MutableCommandListFixture<true>>;

HWTEST2_F(MutableCommandListInOrderTest,
          givenInOrderMutableCmdListWhenAppendingKernelWithSignalEventAggregatedAndMutateItThenExpectPatchNewEventAddress,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::PorWalkerType;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(true, false, false, true);
    auto newEvent = createTestEvent(true, false, false, true);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, event->toHandle(), 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    uint64_t baseGpuVa = reinterpret_cast<uint64_t>(this->externalStorages[0]);

    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, newEvent->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    baseGpuVa = reinterpret_cast<uint64_t>(this->externalStorages[1]);
    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);
}

HWTEST2_F(MutableCommandListKernelTest,
          givenKernelsWithScratchWhenKernelIsMutatedThenScratchPointerUpdated, IsAtLeastXe3pCore) {

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x100;
    mockKernelImmData2->kernelDescriptor->kernelAttributes.perThreadScratchSize[1] = 0x200;

    constexpr uint64_t scratchAddress1 = 0x1248000;
    constexpr uint64_t scratchAddress2 = 0x248A000;

    mutableCommandIdDesc.flags = kernelIsaMutationFlags;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernelMutationGroup, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(0u, mutableCommandList->kernelMutations.size());
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];
    ASSERT_NE(nullptr, mutation.kernelGroup);
    EXPECT_TRUE(mutation.kernelGroup->isScratchNeeded());

    result = mutableCommandList->appendLaunchKernel(kernelHandle, this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 1u);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // override scratch address - in regular execution set by patchCommands
    overridePatchedScratchAddress(scratchAddress1);

    // mutation should update scratch address in second kernel
    result = mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernel2Handle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 1u);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(nullptr, mutation.kernelGroup->getCurrentMutableKernel());

    auto walkerCmdInlineBuffer = mutation.kernelGroup->getCurrentMutableKernel()->getMutableComputeWalker()->getInlineDataPointer();
    auto scratchSrcPtr = ptrOffset(walkerCmdInlineBuffer, mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress.offset);

    auto ssh = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::surfaceState);
    uint64_t expectedScratchAddress = scratchAddress1;
    if (ssh != nullptr) {
        expectedScratchAddress += ssh->getGpuBase();
    }

    uint64_t programmedScratchAddress = 0;
    memcpy(&programmedScratchAddress, scratchSrcPtr, sizeof(uint64_t));
    EXPECT_EQ(expectedScratchAddress, programmedScratchAddress);

    // simulate change of scratch address and verify new address is mutated back
    overridePatchedScratchAddress(scratchAddress2);
    expectedScratchAddress = scratchAddress2;
    if (ssh != nullptr) {
        expectedScratchAddress += ssh->getGpuBase();
    }

    result = mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernelHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&programmedScratchAddress, scratchSrcPtr, sizeof(uint64_t));
    EXPECT_EQ(expectedScratchAddress, programmedScratchAddress);
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 1u);
}

HWTEST2_F(MutableCommandListKernelTest, givenScratchKernelWhenAppendLaunchKernelIsDoneThenScratchPatchAndScratchPatchIndexAreValid, IsAtLeastXe3pCore) {
    mutableCommandIdDesc.flags = kernelIsaMutationFlags;

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x80;

    ze_kernel_handle_t kernels[2] = {kernel->toHandle(), kernel2->toHandle()};
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernels[0], this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));

    uint64_t commandId2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId2));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernels[1], this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    // cmdsToPatch should contain two scratch-specific commands to patch
    auto baseCommandList = mutableCommandList->base;
    auto &cmdsToPatch = baseCommandList->getCommandsToPatch();

    auto scratchPatchNum = std::count_if(cmdsToPatch.begin(), cmdsToPatch.end(),
                                         [](const CommandToPatch &cmd) { return std::holds_alternative<PatchComputeWalkerInlineDataScratch>(cmd); });

    EXPECT_EQ(scratchPatchNum, 2);
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 2u);

    auto scratchPatchIndex1 = mutableCommandList->kernelMutations[0].kernelGroup->getScratchAddressPatchIndex();
    EXPECT_TRUE(isDefined(scratchPatchIndex1));
    EXPECT_TRUE(cmdsToPatch.size() > scratchPatchIndex1);

    auto *scratchPatchCommand1 = std::get_if<PatchComputeWalkerInlineDataScratch>(&cmdsToPatch[scratchPatchIndex1]);
    ASSERT_NE(nullptr, scratchPatchCommand1);
    EXPECT_EQ(scratchPatchCommand1->pDestination,
              mutableCommandList->kernelMutations[0].kernelGroup->getCurrentMutableKernel()->getMutableComputeWalker()->getWalkerCmdPointer());

    auto scratchPatchIndex2 = mutableCommandList->kernelMutations[1].kernelGroup->getScratchAddressPatchIndex();
    EXPECT_NE(scratchPatchIndex1, scratchPatchIndex2);

    EXPECT_TRUE(isDefined(scratchPatchIndex2));
    EXPECT_TRUE(cmdsToPatch.size() > scratchPatchIndex2);

    auto *scratchPatchCommand2 = std::get_if<PatchComputeWalkerInlineDataScratch>(&cmdsToPatch[scratchPatchIndex2]);
    ASSERT_NE(nullptr, scratchPatchCommand2);
    EXPECT_EQ(scratchPatchCommand2->pDestination,
              mutableCommandList->kernelMutations[1].kernelGroup->getCurrentMutableKernel()->getMutableComputeWalker()->getWalkerCmdPointer());
}

HWTEST2_F(MutableCommandListKernelTest, givenNonScratchKernelWhenMutatingToScratchKernelAndBackThenScratchPatchCommandIsProperlyUpdated, IsAtLeastXe3pCore) {
    mutableCommandIdDesc.flags = kernelIsaMutationFlags;

    const uint8_t expectedScratchOffset = 8u;
    const uint8_t expectedScratchPtrSize = sizeof(uint64_t);

    auto &scratchPointerAddress = mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress.offset = undefined<decltype(scratchPointerAddress.offset)>;
    scratchPointerAddress.pointerSize = undefined<decltype(scratchPointerAddress.pointerSize)>;
    ASSERT_EQ(0u, mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0]);
    ASSERT_EQ(0u, mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[1]);

    mockKernelImmData2->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x80;
    auto &scratchPointerAddress2 = mockKernelImmData2->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress2.offset = expectedScratchOffset;
    scratchPointerAddress2.pointerSize = expectedScratchPtrSize;

    ze_kernel_handle_t kernels[2] = {kernel->toHandle(), kernel2->toHandle()};
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernels[0], this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 0u);

    // Patch command should contain undefined values
    auto baseCommandList = mutableCommandList->base;
    auto scratchPatchIndex = mutableCommandList->kernelMutations[0].kernelGroup->getScratchAddressPatchIndex();
    EXPECT_TRUE(isDefined(scratchPatchIndex));

    auto &cmdsToPatch = baseCommandList->getCommandsToPatch();
    EXPECT_TRUE(cmdsToPatch.size() > scratchPatchIndex);

    auto *scratchPatchCommand = std::get_if<PatchComputeWalkerInlineDataScratch>(&cmdsToPatch[scratchPatchIndex]);
    ASSERT_NE(nullptr, scratchPatchCommand);

    EXPECT_TRUE(isUndefined(scratchPatchCommand->offset));
    EXPECT_TRUE(isUndefined(scratchPatchCommand->patchSize));

    // Now mutate to kernel2 which has a defined scratch offset
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernels[1]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    // Patch commands should have been updated with the new scratch offset
    auto *scratchPatchCommandAfterMutate = std::get_if<PatchComputeWalkerInlineDataScratch>(&cmdsToPatch[scratchPatchIndex]);
    ASSERT_NE(nullptr, scratchPatchCommandAfterMutate);

    EXPECT_EQ(expectedScratchOffset,
              static_cast<uint8_t>(scratchPatchCommandAfterMutate->offset - mutableCommandList->mutableWalkerCmds[0]->getInlineDataOffset()));
    EXPECT_EQ(expectedScratchPtrSize, static_cast<uint8_t>(scratchPatchCommandAfterMutate->patchSize));
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 1u);

    // Mutate kernel back to kernel with undefined scratch offset
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernels[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    auto *scratchPatchCommandAfterBack = std::get_if<PatchComputeWalkerInlineDataScratch>(&cmdsToPatch[scratchPatchIndex]);
    ASSERT_NE(nullptr, scratchPatchCommandAfterBack);

    EXPECT_TRUE(isUndefined(scratchPatchCommandAfterBack->offset));
    EXPECT_TRUE(isUndefined(scratchPatchCommandAfterBack->patchSize));
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 0u);
}

HWTEST2_F(MutableCommandListKernelTest, givenScratchKernelGroupWhenMutatingFromNonScratchKerneToOtherNonScratchKernelThenScratchPatchCommandIsPresentAndRemainsUndefined,
          IsAtLeastXe3pCore) {
    mutableCommandIdDesc.flags = kernelIsaMutationFlags;

    auto &scratchPointerAddress = mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress.offset = undefined<decltype(scratchPointerAddress.offset)>;
    scratchPointerAddress.pointerSize = undefined<decltype(scratchPointerAddress.pointerSize)>;

    mockKernelImmData2->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x80;
    auto &scratchPointerAddress2 = mockKernelImmData2->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress2.offset = undefined<decltype(scratchPointerAddress.offset)>;
    scratchPointerAddress2.pointerSize = undefined<decltype(scratchPointerAddress.pointerSize)>;

    auto scratchKernelImmData = prepareKernelImmData(0x1000);
    auto scratchModule = prepareModule(scratchKernelImmData.get());
    auto scratchKernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(scratchModule.get());
    createKernel(scratchKernel.get());

    ze_kernel_handle_t kernels[3] = {kernel->toHandle(), kernel2->toHandle(), scratchKernel->toHandle()};
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 3, kernels, &commandId));
    EXPECT_EQ(ZE_RESULT_SUCCESS,
              mutableCommandList->appendLaunchKernel(kernels[0], this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 0u);

    // Patch command should exist and contain undefined values
    auto baseCommandList = mutableCommandList->base;
    auto scratchPatchIndex = mutableCommandList->kernelMutations[0].kernelGroup->getScratchAddressPatchIndex();
    EXPECT_TRUE(isDefined(scratchPatchIndex));

    auto &cmdsToPatch = baseCommandList->getCommandsToPatch();
    ASSERT_TRUE(cmdsToPatch.size() > scratchPatchIndex);

    auto &scratchPatchCommandVariant = cmdsToPatch[scratchPatchIndex];
    auto *scratchPatchCommand = std::get_if<PatchComputeWalkerInlineDataScratch>(&scratchPatchCommandVariant);
    ASSERT_NE(nullptr, scratchPatchCommand);

    EXPECT_TRUE(isUndefined(scratchPatchCommand->offset));
    EXPECT_TRUE(isUndefined(scratchPatchCommand->patchSize));

    // Mutate to kernel2 which also has an undefined scratch offset
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernels[1]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    // Patch commands should still contain undefined values
    auto &scratchPatchCommandVariantAfter = cmdsToPatch[scratchPatchIndex];
    auto *scratchPatchCommandAfter = std::get_if<PatchComputeWalkerInlineDataScratch>(&scratchPatchCommandVariantAfter);
    ASSERT_NE(nullptr, scratchPatchCommandAfter);

    EXPECT_TRUE(isUndefined(scratchPatchCommandAfter->offset));
    EXPECT_TRUE(isUndefined(scratchPatchCommandAfter->patchSize));
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 0u);
}

HWTEST2_F(MutableCommandListKernelTest, givenScratchKernelWhenMutatingWithSameScratchOffsetThenWalkerIsUpdated, IsAtLeastXe3pCore) {
    mutableCommandIdDesc.flags = kernelIsaMutationFlags;
    const uint64_t scratchOffset = 8u;

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x80;
    auto &scratchPointerAddress = mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress.pointerSize = sizeof(uint64_t);
    scratchPointerAddress.offset = scratchOffset;

    mockKernelImmData2->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x80;
    auto &scratchPointerAddress2 = mockKernelImmData2->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress2.pointerSize = sizeof(uint64_t);
    scratchPointerAddress2.offset = scratchOffset;

    ze_kernel_handle_t kernels[2] = {kernel->toHandle(), kernel2->toHandle()};

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernels[0], this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 1u);

    // Set up a mock scratch patch address
    uint64_t mockScratchPatchAddress = 0xABCD0000;
    overridePatchedScratchAddress(mockScratchPatchAddress);

    // Now mutate to kernel2 which has the same scratch offset
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernels[1]));

    // After mutation with same scratch offset, compute walker should have the scratch address programmed properly
    uint64_t expectedProgrammedScratchPatchAddress = mockScratchPatchAddress;
    auto ssh = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState);
    if (ssh != nullptr) {
        expectedProgrammedScratchPatchAddress += ssh->getGpuBase();
    }

    using WalkerType = typename FamilyType::DefaultWalkerType;
    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[1]->getWalkerCmdPointer());
    auto programmedScratchAddress = reinterpret_cast<uint64_t *>(ptrOffset(walkerCmd->getInlineDataPointer(), scratchOffset));
    EXPECT_EQ(*programmedScratchAddress, expectedProgrammedScratchPatchAddress);
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 1u);
}

HWTEST2_F(MutableCommandListKernelTest, givenTwoAppendsWhenMutatingFromNoScratchToScratchKernelThenProperScratchPatchCommandIsUpdated, IsAtLeastXe3pCore) {
    mutableCommandIdDesc.flags = kernelIsaMutationFlags;
    uint64_t commandId2 = 0;

    const uint8_t expectedScratchOffset = 8u;
    const uint8_t expectedScratchPtrSize = sizeof(uint64_t);

    auto &scratchPointerAddress = mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress.offset = undefined<decltype(scratchPointerAddress.offset)>;
    scratchPointerAddress.pointerSize = undefined<decltype(scratchPointerAddress.pointerSize)>;
    ASSERT_EQ(0u, mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0]);
    ASSERT_EQ(0u, mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[1]);

    mockKernelImmData2->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x80;
    auto &scratchPointerAddress2 = mockKernelImmData2->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress2.offset = expectedScratchOffset;
    scratchPointerAddress2.pointerSize = expectedScratchPtrSize;

    ze_kernel_handle_t kernels[2] = {kernel->toHandle(), kernel2->toHandle()};
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernels[0], this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId2));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernels[0], this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    // There should be two patch commands with different indexes on both kernel groups
    auto scratchPatchIndex1 = mutableCommandList->kernelMutations[0].kernelGroup->getScratchAddressPatchIndex();
    auto scratchPatchIndex2 = mutableCommandList->kernelMutations[1].kernelGroup->getScratchAddressPatchIndex();
    EXPECT_TRUE(isDefined(scratchPatchIndex1));
    EXPECT_TRUE(isDefined(scratchPatchIndex2));
    EXPECT_NE(scratchPatchIndex1, scratchPatchIndex2);

    auto baseCommandList = mutableCommandList->base;
    auto &cmdsToPatch = baseCommandList->getCommandsToPatch();
    EXPECT_TRUE(cmdsToPatch.size() > scratchPatchIndex2);

    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 0u);

    // Now mutate first append to kernel2 which has a defined scratch offset
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernels[1]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    // Patch commands should have been updated with the new scratch offset in the first kernel group
    auto &scratchPatchCommand1 = std::get<PatchComputeWalkerInlineDataScratch>(cmdsToPatch[scratchPatchIndex1]);

    EXPECT_EQ(expectedScratchOffset, static_cast<uint8_t>(scratchPatchCommand1.offset - mutableCommandList->mutableWalkerCmds[0]->getInlineDataOffset()));
    EXPECT_EQ(expectedScratchPtrSize, static_cast<uint8_t>(scratchPatchCommand1.patchSize));
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 1u);

    // Second kernel group should still have scratch patch undefined
    auto &scratchPatchCommand2 = std::get<PatchComputeWalkerInlineDataScratch>(cmdsToPatch[scratchPatchIndex2]);
    EXPECT_TRUE(isUndefined(scratchPatchCommand2.offset));
    EXPECT_TRUE(isUndefined(scratchPatchCommand2.patchSize));

    // Mutate second append to kernel2 which has a defined scratch offset
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandKernelsExp(1, &commandId2, &kernels[1]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    EXPECT_EQ(expectedScratchOffset, static_cast<uint8_t>(scratchPatchCommand2.offset - mutableCommandList->mutableWalkerCmds[0]->getInlineDataOffset()));
    EXPECT_EQ(expectedScratchPtrSize, static_cast<uint8_t>(scratchPatchCommand2.patchSize));
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 2u);
}

HWTEST2_F(MutableCommandListKernelTest, givenScratchKernelsWhenMutatingWithDifferentScratchOffsetsThenWalkerIsUpdatedAndNoPatchingIsRequired, IsAtLeastXe3pCore) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT;
    const uint64_t scratchOffset1 = 8u;
    const uint64_t scratchOffset2 = 16u;

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x80;
    auto &scratchPointerAddress = mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress.pointerSize = sizeof(uint64_t);
    scratchPointerAddress.offset = scratchOffset1;

    mockKernelImmData2->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x80;
    auto &scratchPointerAddress2 = mockKernelImmData2->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress2.pointerSize = sizeof(uint64_t);
    scratchPointerAddress2.offset = scratchOffset2;

    ze_kernel_handle_t kernels[2] = {kernel->toHandle(), kernel2->toHandle()};

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernels[0], this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    // Set up a mock scratch patch address (in regular execution cmdQueue::patchCommands would do it)
    uint64_t mockScratchPatchAddress = 0xABCD0000;
    overridePatchedScratchAddress(mockScratchPatchAddress);

    // Scratch patch should be set to first kernel offset
    auto scratchPatchIndex = mutableCommandList->kernelMutations[0].kernelGroup->getScratchAddressPatchIndex();
    auto &baseCommandList = mutableCommandList->base;
    auto &cmdsToPatch = baseCommandList->getCommandsToPatch();
    EXPECT_TRUE(cmdsToPatch.size() > scratchPatchIndex);
    auto &scratchPatchCommand = std::get<PatchComputeWalkerInlineDataScratch>(cmdsToPatch[scratchPatchIndex]);

    EXPECT_EQ(scratchOffset1, static_cast<uint8_t>(scratchPatchCommand.offset - mutableCommandList->mutableWalkerCmds[0]->getInlineDataOffset()));
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 1u);

    // Now mutate to kernel2 which has different scratch offset
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernels[1]));

    // After mutation, compute walker should have the scratch address programmed properly without need of patchCommands
    uint64_t expectedProgrammedScratchPatchAddress = mockScratchPatchAddress;
    auto ssh = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState);
    if (ssh != nullptr) {
        expectedProgrammedScratchPatchAddress += ssh->getGpuBase();
    }

    using WalkerType = typename FamilyType::DefaultWalkerType;
    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[1]->getWalkerCmdPointer());
    auto programmedScratchAddress = reinterpret_cast<uint64_t *>(ptrOffset(walkerCmd->getInlineDataPointer(), scratchOffset2));
    EXPECT_EQ(*programmedScratchAddress, expectedProgrammedScratchPatchAddress);

    // Scratch patch should be updated to second kernel offset
    EXPECT_EQ(scratchOffset2, static_cast<uint8_t>(scratchPatchCommand.offset - mutableCommandList->mutableWalkerCmds[0]->getInlineDataOffset()));
    EXPECT_EQ(mutableCommandList->getBase()->getActiveScratchPatchElements(), 1u);
}

HWTEST2_F(MutableCommandListKernelTest, givenScratchKernelsAndScratchPatchIndexUndefinedWhenMutatingKernelThenWalkerIsNotUpdated, IsAtLeastXe3pCore) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT;
    const uint64_t scratchOffset1 = 8u;
    const uint64_t scratchOffset2 = 16u;

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x80;
    auto &scratchPointerAddress = mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress.pointerSize = sizeof(uint64_t);
    scratchPointerAddress.offset = scratchOffset1;

    mockKernelImmData2->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x80;
    auto &scratchPointerAddress2 = mockKernelImmData2->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    scratchPointerAddress2.pointerSize = sizeof(uint64_t);
    scratchPointerAddress2.offset = scratchOffset2;

    ze_kernel_handle_t kernels[2] = {kernel->toHandle(), kernel2->toHandle()};

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernels[0], this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->close());

    // Set up a mock scratch patch address (in regular execution cmdQueue::patchCommands would do it)
    uint64_t mockScratchPatchAddress = 0xABCD0000;
    overridePatchedScratchAddress(mockScratchPatchAddress);

    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];
    auto &kernelsInGroup = mutation.kernelGroup->getKernelsInGroup();
    ASSERT_EQ(2u, kernelsInGroup.size());

    const size_t undefinedScratchPatchIndex = L0::MCL::undefined<size_t>;

    mutableCommandList->updateScratchAddress(undefinedScratchPatchIndex, *kernelsInGroup[0]->getMutableComputeWalker(), *kernelsInGroup[1]->getMutableComputeWalker());

    auto scratchPatchAddress = ptrOffset(kernelsInGroup[1]->getMutableComputeWalker()->getInlineDataPointer(), kernelsInGroup[1]->getMutableComputeWalker()->getScratchOffset());
    uint64_t scratchPatchAddressValue = 0;
    memcpy(&scratchPatchAddressValue, scratchPatchAddress, sizeof(uint64_t));
    EXPECT_EQ(0u, scratchPatchAddressValue);
}

} // namespace ult
} // namespace L0

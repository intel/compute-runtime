/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_variable.h"

namespace L0 {
namespace ult {

using MutableCommandListKernelTest = Test<MutableCommandListFixture<false>>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListKernelTest,
            givenTwoKernelsWhenFirstAppendedAndSecondMutatedThenDataIsChangedAndCommandIsPatched) {
    using WalkerType = typename FamilyType::PorWalkerType;

    mutableCommandIdDesc.flags = kernelIsaMutationFlags;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernelMutationGroup, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(0u, mutableCommandList->mutations.size());
    auto &mutation = mutableCommandList->mutations[commandId - 1];
    ASSERT_NE(nullptr, mutation.kernelGroup);

    result = mutableCommandList->appendLaunchKernel(kernelHandle, this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(nullptr, mutation.kernelGroup->getCurrentMutableKernel());
    EXPECT_EQ(kernel.get(), mutation.kernelGroup->getCurrentMutableKernel()->getKernel());

    auto walkerCmdBuffer = mutation.kernelGroup->getCurrentMutableKernel()->getMutableComputeWalker()->getWalkerCmdPointer();
    auto walkerCmd = reinterpret_cast<WalkerType *>(walkerCmdBuffer);

    auto kernelIsaAlloc = kernel->getIsaAllocation();
    EXPECT_EQ(kernelIsaAlloc->getGpuAddressToPatch(), walkerCmd->getInterfaceDescriptor().getKernelStartPointer());

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto kernelIsaAllocIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                         whiteBoxAllocations.addedAllocations.end(),
                                         [&kernelIsaAlloc](const L0::MCL::AllocationReference &ref) {
                                             return ref.allocation == kernelIsaAlloc;
                                         });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), kernelIsaAllocIt);

    result = mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernel2Handle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(kernel2.get(), mutation.kernelGroup->getCurrentMutableKernel()->getKernel());

    kernelIsaAlloc = kernel2->getIsaAllocation();
    EXPECT_EQ(kernelIsaAlloc->getGpuAddressToPatch(), walkerCmd->getInterfaceDescriptor().getKernelStartPointer());

    kernelIsaAllocIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                    whiteBoxAllocations.addedAllocations.end(),
                                    [&kernelIsaAlloc](const L0::MCL::AllocationReference &ref) {
                                        return ref.allocation == kernelIsaAlloc;
                                    });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), kernelIsaAllocIt);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListKernelTest,
            givenTwoKernelsWithSignalEventWhenFirstAppendedAndSecondMutatedThenPostSyncIsPreserved) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto event = createTestEvent(false, false, false, false);
    auto eventAddress = event->getGpuAddress(this->device);

    mutableCommandIdDesc.flags = kernelIsaMutationFlags | ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernelMutationGroup, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(0u, mutableCommandList->mutations.size());
    auto &mutation = mutableCommandList->mutations[commandId - 1];
    ASSERT_NE(nullptr, mutation.kernelGroup);

    result = mutableCommandList->appendLaunchKernel(kernelHandle, this->testGroupCount, event->toHandle(), 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(nullptr, mutation.kernelGroup->getCurrentMutableKernel());

    auto walkerCmdBuffer = mutation.kernelGroup->getCurrentMutableKernel()->getMutableComputeWalker()->getWalkerCmdPointer();
    auto walkerCmd = reinterpret_cast<WalkerType *>(walkerCmdBuffer);

    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(eventAddress, walkerPostSyncAddress);

    result = mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernel2Handle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(eventAddress, walkerPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListKernelTest,
            givenTwoKernelsWithBufferArgumentsWhenFirstAppendedAndSecondMutatedThenBufferResidencyIsChanged) {

    void *usm1 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm1);
    auto usm1Allocation = getUsmAllocation(usm1);
    void *usm2 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm2);
    auto usm2Allocation = getUsmAllocation(usm2);

    // set kernel arg 0 => buffer
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    auto result = kernel->setArgBuffer(0, sizeof(void *), &usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    mutableCommandIdDesc.flags = kernelIsaMutationFlags;

    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernelMutationGroup, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernelHandle, this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto usmAllocIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                   whiteBoxAllocations.addedAllocations.end(),
                                   [&usm1Allocation](const L0::MCL::AllocationReference &ref) {
                                       return ref.allocation == usm1Allocation;
                                   });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), usmAllocIt);

    result = mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernel2Handle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_kernel_argument_exp_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &usm2;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    usmAllocIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                              whiteBoxAllocations.addedAllocations.end(),
                              [&usm2Allocation](const L0::MCL::AllocationReference &ref) {
                                  return ref.allocation == usm2Allocation;
                              });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), usmAllocIt);

    bool poolAllocations = (usm1Allocation == usm2Allocation);
    if (poolAllocations == false) {
        usmAllocIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                  whiteBoxAllocations.addedAllocations.end(),
                                  [&usm1Allocation](const L0::MCL::AllocationReference &ref) {
                                      return ref.allocation == usm1Allocation;
                                  });
        EXPECT_EQ(whiteBoxAllocations.addedAllocations.end(), usmAllocIt);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListKernelTest,
            givenTwoKernelsWithBufferAndSlmArgumentsWhenFirstAppendedAsNullSurfaceAndSecondMutatedThenFirstKernelBufferVariableIsNulled) {

    void *usm2 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm2);
    auto usm2Allocation = getUsmAllocation(usm2);
    size_t slm2Size = 1024;

    // set kernel arg 0, 1 => buffer, slm
    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::slmBuffer, kernelAllMask);

    auto result = kernel->setArgBuffer(0, sizeof(void *), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgBuffer(1, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    mutableCommandIdDesc.flags = kernelIsaMutationFlags;

    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernelMutationGroup, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernelHandle, this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernel2Handle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel1BufferVariables = getVariableList(commandId, L0::MCL::VariableType::buffer, kernel.get());
    ASSERT_EQ(1u, kernel1BufferVariables.size());
    auto kernel1BufferVariable = kernel1BufferVariables[0];

    void *expectedArgValue = reinterpret_cast<void *>(undefined<size_t>);
    EXPECT_EQ(expectedArgValue, kernel1BufferVariable->getDesc().argValue);
    EXPECT_EQ(nullptr, kernel1BufferVariable->getDesc().bufferAlloc);
    EXPECT_EQ(undefined<uint64_t>, kernel1BufferVariable->getDesc().bufferGpuAddress);

    auto kernel1SlmVariables = getVariableList(commandId, L0::MCL::VariableType::slmBuffer, kernel.get());
    ASSERT_EQ(1u, kernel1SlmVariables.size());
    auto kernel1SlmVariable = kernel1SlmVariables[0];

    EXPECT_EQ(expectedArgValue, kernel1SlmVariable->getDesc().argValue);
    EXPECT_EQ(nullptr, kernel1SlmVariable->getDesc().bufferAlloc);
    EXPECT_EQ(undefined<uint64_t>, kernel1SlmVariable->getDesc().bufferGpuAddress);

    ze_mutable_kernel_argument_exp_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &usm2;

    ze_mutable_kernel_argument_exp_desc_t kernelSlmArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    kernelBufferArg.pNext = &kernelSlmArg;

    kernelSlmArg.argIndex = 1;
    kernelSlmArg.argSize = slm2Size;
    kernelSlmArg.commandId = commandId;
    kernelSlmArg.pArgValue = nullptr;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto usmAllocIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                   whiteBoxAllocations.addedAllocations.end(),
                                   [&usm2Allocation](const L0::MCL::AllocationReference &ref) {
                                       return ref.allocation == usm2Allocation;
                                   });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), usmAllocIt);

    auto kernel2BufferVariables = getVariableList(commandId, L0::MCL::VariableType::buffer, kernel2.get());
    ASSERT_EQ(1u, kernel2BufferVariables.size());
    auto kernel2BufferVariable = kernel2BufferVariables[0];

    EXPECT_EQ(usm2Allocation, kernel2BufferVariable->getDesc().bufferAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListKernelTest,
            givenTwoKernelsOneWithBiggerPayloadSizeWhenFirstAppendedWithSmallerAndSecondMutatedThenBiggerPayloadSizeConsumedAsReserveAtAppendTime) {
    constexpr uint32_t kernel2CrossThreadInitSize = 2 * crossThreadInitSize;

    mockKernelImmData2->kernelDescriptor->kernelAttributes.crossThreadDataSize = kernel2CrossThreadInitSize;
    mockKernelImmData2->crossThreadDataSize = kernel2CrossThreadInitSize;
    mockKernelImmData2->crossThreadDataTemplate.reset(new uint8_t[kernel2CrossThreadInitSize]);
    kernel2->crossThreadDataSize = kernel2CrossThreadInitSize;
    kernel2->crossThreadData.reset(new uint8_t[kernel2CrossThreadInitSize]);

    mutableCommandIdDesc.flags = kernelIsaMutationFlags;

    size_t totalPayloadConsumedSize = kernel2CrossThreadInitSize + mutableCommandList->maxPerThreadDataSize;
    totalPayloadConsumedSize = alignUp(totalPayloadConsumedSize, mutableCommandList->iohAlignment);

    void *usm1 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm1);
    void *usm2 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm2);

    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernel1Bit);
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernel1Bit);

    // make sure kernel 2 offsets are bigger than cross thread data size of kernel 1
    this->crossThreadOffset = crossThreadInitSize + 8;

    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernel2Bit);
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernel2Bit);

    auto result = kernel->setArgBuffer(0, sizeof(void *), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgBuffer(1, sizeof(void *), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernelMutationGroup, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(0u, mutableCommandList->mutations.size());
    auto &mutation = mutableCommandList->mutations[commandId - 1];
    ASSERT_NE(nullptr, mutation.kernelGroup);
    EXPECT_EQ(kernel2CrossThreadInitSize, mutation.kernelGroup->getMaxAppendIndirectHeapSize());

    result = mutableCommandList->appendLaunchKernel(kernelHandle, this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedPayloadSize = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject)->getUsed();
    EXPECT_EQ(totalPayloadConsumedSize, usedPayloadSize);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernel2Handle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_kernel_argument_exp_desc_t kernelBufferArg1 = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutableCommandsDesc.pNext = &kernelBufferArg1;

    kernelBufferArg1.argIndex = 0;
    kernelBufferArg1.argSize = sizeof(void *);
    kernelBufferArg1.commandId = commandId;
    kernelBufferArg1.pArgValue = &usm1;

    ze_mutable_kernel_argument_exp_desc_t kernelBufferArg2 = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    kernelBufferArg1.pNext = &kernelBufferArg2;

    kernelBufferArg2.argIndex = 1;
    kernelBufferArg2.argSize = sizeof(void *);
    kernelBufferArg2.commandId = commandId;
    kernelBufferArg2.pArgValue = &usm2;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel2BufferVariables = getVariableList(commandId, L0::MCL::VariableType::buffer, kernel2.get());
    ASSERT_EQ(2u, kernel2BufferVariables.size());

    auto bufferVarArg1 = kernel2BufferVariables[0];
    auto gpuVa1PatchFullAddress = reinterpret_cast<void *>(bufferVarArg1->getBufferUsages().statelessWithoutOffset[0]);

    auto bufferVarArg2 = kernel2BufferVariables[1];
    auto gpuVa2PatchFullAddress = reinterpret_cast<void *>(bufferVarArg2->getBufferUsages().statelessWithoutOffset[0]);

    uint64_t usmPatchAddressValue = 0;
    memcpy(&usmPatchAddressValue, gpuVa1PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    memcpy(&usmPatchAddressValue, gpuVa2PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListKernelTest,
            givenKernelGroupWhenNextCommandIdCalledThenSetMaxIsaSize) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;

    ze_kernel_handle_t kernels[2] = {kernel->toHandle(), kernel2->toHandle()};

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &mutation = mutableCommandList->mutations[commandId - 1];
    ASSERT_NE(nullptr, mutation.kernelGroup);

    auto maxIsaSize = std::max(kernel->getImmutableData()->getIsaSize(), kernel2->getImmutableData()->getIsaSize());

    EXPECT_EQ(maxIsaSize, mutation.kernelGroup->getMaxIsaSize());
}

HWTEST2_F(MutableCommandListKernelTest,
          givenPrefetchEnabledWhenAppendCalledThenSetAllRequiredParams,
          IsAtLeastXeHpcCore) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    using MI_NOOP = typename FamilyType::MI_NOOP;
    debugManager.flags.EnableMemoryPrefetch.set(1);
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT;

    auto ioh = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject);
    ioh->getSpace(MemoryConstants::cacheLineSize);

    ze_kernel_handle_t kernels[2] = {kernel->toHandle(), kernel2->toHandle()};

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &mutation = mutableCommandList->mutations[commandId - 1];
    ASSERT_NE(nullptr, mutation.kernelGroup);

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    mutableCommandList->close();

    auto &prefetchCmdToPatch = mutation.kernelGroup->getPrefetchCmd();
    ASSERT_EQ(CommandToPatch::CommandType::PrefetchKernelMemory, prefetchCmdToPatch.type);
    ASSERT_NE(nullptr, mutation.kernelGroup->getIohForPrefetch());

    uint32_t expectedIohPrefetchSize = alignUp(kernel->getIndirectSize(), MemoryConstants::cacheLineSize);
    size_t expectedIohPrefetchPadding = NEO::EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(mutation.kernelGroup->getMaxAppendIndirectHeapSize() - expectedIohPrefetchSize,
                                                                                                        this->device->getNEODevice()->getRootDeviceEnvironment());

    uint32_t expectedIsaPrefetchSize = kernel->getImmutableData()->getIsaSize();
    size_t expectedIsaPrefetchPadding = NEO::EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(mutation.kernelGroup->getMaxIsaSize() - expectedIsaPrefetchSize,
                                                                                                        this->device->getNEODevice()->getRootDeviceEnvironment());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, prefetchCmdToPatch.pDestination, prefetchCmdToPatch.patchSize));

    auto itor = cmdList.begin();

    auto prefetchCmd = genCmdCast<STATE_PREFETCH *>(*itor);
    ASSERT_NE(nullptr, prefetchCmd);
    EXPECT_EQ(expectedIohPrefetchSize / MemoryConstants::cacheLineSize, prefetchCmd->getPrefetchSize());
    EXPECT_EQ(mutation.kernelGroup->getIohForPrefetch()->getGpuAddress() + prefetchCmdToPatch.offset, prefetchCmd->getAddress());
    itor++;

    for (size_t i = 0; i < expectedIohPrefetchPadding; i += sizeof(MI_NOOP)) {
        auto noopCmd = genCmdCast<MI_NOOP *>(*itor);
        ASSERT_NE(nullptr, noopCmd);
        itor++;
    }

    auto isaPrefetchCmd = genCmdCast<STATE_PREFETCH *>(*itor);
    ASSERT_NE(nullptr, isaPrefetchCmd);
    EXPECT_EQ(expectedIsaPrefetchSize / MemoryConstants::cacheLineSize, isaPrefetchCmd->getPrefetchSize());
    EXPECT_EQ(kernel->getIsaAllocation()->getGpuAddress(), isaPrefetchCmd->getAddress());
    itor++;

    for (size_t i = 0; i < expectedIsaPrefetchPadding; i += sizeof(MI_NOOP)) {
        auto noopCmd = genCmdCast<MI_NOOP *>(*itor);
        ASSERT_NE(nullptr, noopCmd);
        itor++;
    }
}

HWTEST2_F(MutableCommandListKernelTest,
          givenPrefetchEnabledWhenAppendingKernelThenEnsureEnoughSpace,
          IsAtLeastXeHpcCore) {
    debugManager.flags.EnableMemoryPrefetch.set(1);
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT;

    ze_kernel_handle_t kernels[2] = {kernel->toHandle(), kernel2->toHandle()};

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &mutation = mutableCommandList->mutations[commandId - 1];
    ASSERT_NE(nullptr, mutation.kernelGroup);

    auto requiredSize = NEO::EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(mutation.kernelGroup->getMaxAppendIndirectHeapSize(), device->getNEODevice()->getRootDeviceEnvironment()) +
                        NEO::EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(mutation.kernelGroup->getMaxIsaSize(), device->getNEODevice()->getRootDeviceEnvironment());

    auto cmdStream = mutableCommandList->getBase()->getCmdContainer().getCommandStream();
    cmdStream->getSpace(cmdStream->getAvailableSpace() - (requiredSize - 1));

    auto oldCmdBuffer = cmdStream->getGraphicsAllocation();

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    mutableCommandList->close();

    EXPECT_NE(oldCmdBuffer, cmdStream->getGraphicsAllocation());

    auto &prefetchCmdToPatch = mutation.kernelGroup->getPrefetchCmd();
    EXPECT_EQ(cmdStream->getCpuBase(), prefetchCmdToPatch.pDestination);
}

HWTEST2_F(MutableCommandListKernelTest,
          givenPrefetchEnabledWhenMutatingThenSetAllRequiredParams,
          IsAtLeastXeHpcCore) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    using MI_NOOP = typename FamilyType::MI_NOOP;
    debugManager.flags.EnableMemoryPrefetch.set(1);
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT;

    auto ioh = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject);
    ioh->getSpace(MemoryConstants::cacheLineSize);

    ze_kernel_handle_t kernels[2] = {kernel->toHandle(), kernel2->toHandle()};

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 2, kernels, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &mutation = mutableCommandList->mutations[commandId - 1];
    ASSERT_NE(nullptr, mutation.kernelGroup);

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams));
    mutableCommandList->close();

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->updateMutableCommandKernelsExp(1, &commandId, &kernels[1]));
    mutableCommandList->close();

    auto &prefetchCmdToPatch = mutation.kernelGroup->getPrefetchCmd();
    ASSERT_EQ(CommandToPatch::CommandType::PrefetchKernelMemory, prefetchCmdToPatch.type);
    ASSERT_NE(nullptr, mutation.kernelGroup->getIohForPrefetch());

    uint32_t expectedIohPrefetchSize = alignUp(kernel2->getIndirectSize(), MemoryConstants::cacheLineSize);
    size_t expectedIohPrefetchPadding = NEO::EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(mutation.kernelGroup->getMaxAppendIndirectHeapSize() - expectedIohPrefetchSize,
                                                                                                        this->device->getNEODevice()->getRootDeviceEnvironment());

    uint32_t expectedIsaPrefetchSize = kernel2->getImmutableData()->getIsaSize();
    size_t expectedIsaPrefetchPadding = NEO::EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(mutation.kernelGroup->getMaxIsaSize() - expectedIsaPrefetchSize,
                                                                                                        this->device->getNEODevice()->getRootDeviceEnvironment());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, prefetchCmdToPatch.pDestination, prefetchCmdToPatch.patchSize));

    auto itor = cmdList.begin();

    auto prefetchCmd = genCmdCast<STATE_PREFETCH *>(*itor);
    ASSERT_NE(nullptr, prefetchCmd);
    EXPECT_EQ(expectedIohPrefetchSize / MemoryConstants::cacheLineSize, prefetchCmd->getPrefetchSize());
    EXPECT_EQ(mutation.kernelGroup->getIohForPrefetch()->getGpuAddress() + prefetchCmdToPatch.offset, prefetchCmd->getAddress());
    itor++;

    for (size_t i = 0; i < expectedIohPrefetchPadding; i += sizeof(MI_NOOP)) {
        auto noopCmd = genCmdCast<MI_NOOP *>(*itor);
        ASSERT_NE(nullptr, noopCmd);
        itor++;
    }

    auto isaPrefetchCmd = genCmdCast<STATE_PREFETCH *>(*itor);
    ASSERT_NE(nullptr, isaPrefetchCmd);
    EXPECT_EQ(expectedIsaPrefetchSize / MemoryConstants::cacheLineSize, isaPrefetchCmd->getPrefetchSize());
    EXPECT_EQ(kernel2->getIsaAllocation()->getGpuAddress(), isaPrefetchCmd->getAddress());
    itor++;

    for (size_t i = 0; i < expectedIsaPrefetchPadding; i += sizeof(MI_NOOP)) {
        auto noopCmd = genCmdCast<MI_NOOP *>(*itor);
        ASSERT_NE(nullptr, noopCmd);
        itor++;
    }
}

} // namespace ult
} // namespace L0

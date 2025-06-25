/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_load_register_imm_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_pipe_control_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_semaphore_wait_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_store_data_imm_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_store_register_mem_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_variable.h"

namespace L0 {
namespace ult {

using MutableCommandListTest = Test<MutableCommandListFixture<false>>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenGettingCommandIdThenGetCorrectFlags) {
    EXPECT_EQ(0u, mutableCommandList->nextCommandId);
    EXPECT_FALSE(mutableCommandList->nextAppendKernelMutable);

    mutableCommandIdDesc.flags = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);

    ASSERT_NE(0u, mutableCommandList->mutations.size());
    auto &mutation = mutableCommandList->mutations[commandId - 1];

    ze_mutable_command_exp_flags_t expectedFlags = ZE_MUTABLE_COMMAND_EXP_FLAG_FORCE_UINT32 & (~ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION);
    EXPECT_EQ(expectedFlags, mutation.mutationFlags);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenCommandListResetThenZeroContainers) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);

    ASSERT_NE(0u, mutableCommandList->mutations.size());
    auto &mutation = mutableCommandList->mutations[commandId - 1];

    ze_mutable_command_exp_flags_t expectedFlags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    EXPECT_EQ(expectedFlags, mutation.mutationFlags);

    result = mutableCommandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, mutableCommandList->nextCommandId);
    EXPECT_FALSE(mutableCommandList->nextAppendKernelMutable);
    EXPECT_EQ(0u, mutableCommandList->mutations.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenAppendingKernelAndMutatingKernelArgumentsThenCorrectVariablesCreatedAndUpdated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    uint64_t usmPatchAddressValue = 0;
    uint32_t valueVariablePatchValue = 0;
    uint32_t slmVariablePatchValue = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->mutations[commandId - 1];

    uint32_t value1 = 2, value2 = 4;
    void *usm1 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm1);
    auto usm1Allocation = getUsmAllocation(usm1);
    void *usm2 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm2);
    auto usm2Allocation = getUsmAllocation(usm2);
    bool poolAllocations = (usm1Allocation == usm2Allocation);
    size_t slm1arg1 = 4;
    size_t slm2arg1 = 2048;
    size_t slm1arg2 = 512;
    size_t slm2arg2 = 1024;

    // set kernel arg 0,1,2,3 => buffer, value, slm, slm2
    resizeKernelArg(4);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::value, kernelAllMask);
    prepareKernelArg(2, L0::MCL::VariableType::slmBuffer, kernelAllMask);
    prepareKernelArg(3, L0::MCL::VariableType::slmBuffer, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), &usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgImmediate(1, sizeof(value1), &value1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgBuffer(2, slm1arg1, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgBuffer(3, slm1arg2, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto itUsm1 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                               whiteBoxAllocations.addedAllocations.end(),
                               [&usm1Allocation](const L0::MCL::AllocationReference &ref) {
                                   return ref.allocation == usm1Allocation;
                               });
    ASSERT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm1);
    auto kernelDispatch = mutableCommandList->dispatchs[0].get();

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &bufferVarMutDesc = mutation.variables[0];
    EXPECT_EQ(ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS, bufferVarMutDesc.varType);
    auto &bufferInternalDesc = bufferVarMutDesc.var->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, bufferInternalDesc.type);
    auto gpuVaPatchFullAddress = reinterpret_cast<void *>(bufferVarMutDesc.var->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto &valueVarMutDesc = mutation.variables[1];
    EXPECT_EQ(ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS, valueVarMutDesc.varType);
    auto &valueInternalDesc = valueVarMutDesc.var->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::value, valueInternalDesc.type);
    auto immediatePatchFullAddress = reinterpret_cast<void *>(valueVarMutDesc.var->getValueUsages().statelessWithoutOffset[0]);
    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value1, valueVariablePatchValue);

    auto &slmVarMutDesc = mutation.variables[2];
    EXPECT_EQ(ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS, slmVarMutDesc.varType);
    auto &slmInternalDesc = slmVarMutDesc.var->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::slmBuffer, slmInternalDesc.type);

    auto &slm2VarMutDesc = mutation.variables[3];
    EXPECT_EQ(ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS, slm2VarMutDesc.varType);
    auto &slm2InternalDesc = slm2VarMutDesc.var->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::slmBuffer, slm2InternalDesc.type);
    auto slmPatchFullAddress = reinterpret_cast<void *>(slm2VarMutDesc.var->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&slmVariablePatchValue, slmPatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(static_cast<uint32_t>(slm1arg1), slmVariablePatchValue);

    EXPECT_EQ(alignUp(slm1arg1 + slm1arg2, MemoryConstants::kiloByte), static_cast<size_t>(kernelDispatch->slmTotalSize));

    ze_mutable_kernel_argument_exp_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    ze_mutable_kernel_argument_exp_desc_t kernelValueArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    ze_mutable_kernel_argument_exp_desc_t kernelSlmArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    ze_mutable_kernel_argument_exp_desc_t kernelSlm2Arg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &usm2;
    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelValueArg.argIndex = 1;
    kernelValueArg.argSize = sizeof(value2);
    kernelValueArg.commandId = commandId;
    kernelValueArg.pArgValue = &value2;
    kernelBufferArg.pNext = &kernelValueArg;

    kernelSlmArg.argIndex = 2;
    kernelSlmArg.argSize = slm2arg1;
    kernelSlmArg.commandId = commandId;
    kernelSlmArg.pArgValue = nullptr;
    kernelValueArg.pNext = &kernelSlmArg;

    kernelSlm2Arg.argIndex = 3;
    kernelSlm2Arg.argSize = slm2arg2;
    kernelSlm2Arg.commandId = commandId;
    kernelSlm2Arg.pArgValue = nullptr;
    kernelSlmArg.pNext = &kernelSlm2Arg;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    itUsm1 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                          whiteBoxAllocations.addedAllocations.end(),
                          [&usm1Allocation](const L0::MCL::AllocationReference &ref) {
                              return ref.allocation == usm1Allocation;
                          });
    if (poolAllocations) {
        EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm1);
    } else {
        EXPECT_EQ(whiteBoxAllocations.addedAllocations.end(), itUsm1);
    }

    auto itUsm2 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                               whiteBoxAllocations.addedAllocations.end(),
                               [&usm2Allocation](const L0::MCL::AllocationReference &ref) {
                                   return ref.allocation == usm2Allocation;
                               });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm2);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);

    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value2, valueVariablePatchValue);

    memcpy(&slmVariablePatchValue, slmPatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(static_cast<uint32_t>(slm2arg1), slmVariablePatchValue);

    EXPECT_EQ(alignUp(slm2arg1 + slm2arg2, MemoryConstants::kiloByte), static_cast<size_t>(kernelDispatch->slmTotalSize));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndKernelBufferNullArgumentsWhenAppendingKernelAndMutatingIntoNullOrBufferKernelArgumentsThenCorrectValuesPatched) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    uint64_t usmPatchAddressValue = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->mutations[commandId - 1];

    void *usm1 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm1);
    auto usm1Allocation = getUsmAllocation(usm1);
    void *usm2 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm2);
    auto usm2Allocation = getUsmAllocation(usm2);
    bool poolAllocations = (usm1Allocation == usm2Allocation);
    void *nullSurface = nullptr;

    // set kernel arg 0,1,=> buffer1, buffer2
    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), &usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgBuffer(1, sizeof(void *), nullSurface);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto itUsm1 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                               whiteBoxAllocations.addedAllocations.end(),
                               [&usm1Allocation](const L0::MCL::AllocationReference &ref) {
                                   return ref.allocation == usm1Allocation;
                               });
    ASSERT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm1);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &buffer1VarMutDesc = mutation.variables[0];
    EXPECT_EQ(ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS, buffer1VarMutDesc.varType);
    auto &buffer1InternalDesc = buffer1VarMutDesc.var->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, buffer1InternalDesc.type);
    auto gpuVa1PatchFullAddress = reinterpret_cast<void *>(buffer1VarMutDesc.var->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVa1PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto &buffer2VarMutDesc = mutation.variables[1];
    EXPECT_EQ(ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS, buffer2VarMutDesc.varType);
    auto &buffer2InternalDesc = buffer2VarMutDesc.var->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, buffer2InternalDesc.type);
    auto gpuVa2PatchFullAddress = reinterpret_cast<void *>(buffer2VarMutDesc.var->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVa2PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(nullSurface), usmPatchAddressValue);

    ze_mutable_kernel_argument_exp_desc_t kernelBuffer1Arg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    ze_mutable_kernel_argument_exp_desc_t kernelBuffer2Arg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};

    kernelBuffer1Arg.argIndex = 0;
    kernelBuffer1Arg.argSize = sizeof(void *);
    kernelBuffer1Arg.commandId = commandId;
    kernelBuffer1Arg.pArgValue = nullSurface;
    mutableCommandsDesc.pNext = &kernelBuffer1Arg;

    kernelBuffer2Arg.argIndex = 1;
    kernelBuffer2Arg.argSize = sizeof(void *);
    kernelBuffer2Arg.commandId = commandId;
    kernelBuffer2Arg.pArgValue = &usm2;
    kernelBuffer1Arg.pNext = &kernelBuffer2Arg;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    itUsm1 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                          whiteBoxAllocations.addedAllocations.end(),
                          [&usm1Allocation](const L0::MCL::AllocationReference &ref) {
                              return ref.allocation == usm1Allocation;
                          });
    if (poolAllocations) {
        EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm1);
    } else {
        EXPECT_EQ(whiteBoxAllocations.addedAllocations.end(), itUsm1);
    }

    auto itUsm2 = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                               whiteBoxAllocations.addedAllocations.end(),
                               [&usm2Allocation](const L0::MCL::AllocationReference &ref) {
                                   return ref.allocation == usm2Allocation;
                               });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), itUsm2);

    memcpy(&usmPatchAddressValue, gpuVa1PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(nullSurface), usmPatchAddressValue);

    memcpy(&usmPatchAddressValue, gpuVa2PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndInlineKernelWhenAppendingKernelAndMutatingArgumentsThenDataIsPatchedIntoInlineData) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.passInlineData = 1;

    uint64_t usmPatchAddressValue = 0;
    uint32_t valueVariablePatchValue = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->mutations[commandId - 1];

    void *usm1 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm1);
    void *usm2 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm2);
    uint32_t value1 = 2, value2 = 4;

    // set kernel arg 0,1,=> buffer, immediate
    this->crossThreadOffset = 8;
    this->nextArgOffset = 16;
    resizeKernelArg(2);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);
    prepareKernelArg(1, L0::MCL::VariableType::value, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), &usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = kernel->setArgImmediate(1, sizeof(value1), &value1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &bufferVarMutDesc = mutation.variables[0];
    EXPECT_EQ(ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS, bufferVarMutDesc.varType);
    auto &bufferInternalDesc = bufferVarMutDesc.var->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, bufferInternalDesc.type);
    ASSERT_NE(0u, bufferVarMutDesc.var->getBufferUsages().commandBufferWithoutOffset.size());
    auto gpuVaPatchFullAddress = reinterpret_cast<void *>(bufferVarMutDesc.var->getBufferUsages().commandBufferWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto &immediateVarMutDesc = mutation.variables[1];
    EXPECT_EQ(ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS, immediateVarMutDesc.varType);
    auto &immediateInternalDesc = immediateVarMutDesc.var->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::value, immediateInternalDesc.type);
    ASSERT_NE(0u, immediateVarMutDesc.var->getValueUsages().commandBufferWithoutOffset.size());
    auto immediatePatchFullAddress = reinterpret_cast<void *>(immediateVarMutDesc.var->getValueUsages().commandBufferWithoutOffset[0]);
    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value1, valueVariablePatchValue);

    ze_mutable_kernel_argument_exp_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    ze_mutable_kernel_argument_exp_desc_t kernelImmediateArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};

    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &usm2;
    kernelBufferArg.pNext = &kernelImmediateArg;

    kernelImmediateArg.argIndex = 1;
    kernelImmediateArg.argSize = sizeof(uint32_t);
    kernelImmediateArg.commandId = commandId;
    kernelImmediateArg.pArgValue = &value2;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);

    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value2, valueVariablePatchValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenKernelDispatchIsSelectedToMutateGroupCountThenUpdatePayloadUponMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    dispatchTraits.globalWorkSize[0] = this->crossThreadOffset + 3 * sizeof(uint32_t);
    dispatchTraits.numWorkGroups[0] = dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t);
    dispatchTraits.workDim = dispatchTraits.numWorkGroups[0] + 3 * sizeof(uint32_t);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto groupCounts = getVariableList(commandId, L0::MCL::VariableType::groupCount, nullptr);
    ASSERT_EQ(1u, groupCounts.size());
    auto groupCountVar = groupCounts[0];
    auto varDispatchGc = groupCountVar->getDispatchs()[0];

    auto crossThreadDataSize = varDispatchGc->getIndirectData()->getCrossThreadDataSize();
    EXPECT_EQ(crossThreadInitSize, crossThreadDataSize);
    auto &offsets = varDispatchGc->getIndirectDataOffsets();

    ze_mutable_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};

    mutableCommandsDesc.pNext = &groupCountDesc;

    ze_group_count_t mutatedGroupCount = {8, 2, 2};

    groupCountDesc.commandId = commandId;
    groupCountDesc.pGroupCount = &mutatedGroupCount;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *payloadBase = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject)->getCpuBase();

    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkSize));
    uint32_t *numWorkGroupsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.numWorkGroups));
    uint32_t *workDimBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.workDimensions));

    EXPECT_EQ(mutatedGroupCount.groupCountX * 1u, gwsBuffer[0]);
    EXPECT_EQ(mutatedGroupCount.groupCountY * 1u, gwsBuffer[1]);
    EXPECT_EQ(mutatedGroupCount.groupCountZ * 1u, gwsBuffer[2]);

    EXPECT_EQ(mutatedGroupCount.groupCountX, numWorkGroupsBuffer[0]);
    EXPECT_EQ(mutatedGroupCount.groupCountY, numWorkGroupsBuffer[1]);
    EXPECT_EQ(mutatedGroupCount.groupCountZ, numWorkGroupsBuffer[2]);

    EXPECT_EQ(3u, *workDimBuffer);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenKernelDispatchIsSelectedToMutateGroupSizeThenUpdatePayloadUponMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    dispatchTraits.enqueuedLocalWorkSize[0] = this->crossThreadOffset;
    dispatchTraits.globalWorkSize[0] = dispatchTraits.enqueuedLocalWorkSize[0] + 3 * sizeof(uint32_t);
    dispatchTraits.workDim = dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto groupSizes = getVariableList(commandId, L0::MCL::VariableType::groupSize, nullptr);
    ASSERT_EQ(1u, groupSizes.size());
    auto groupSizeVar = groupSizes[0];
    auto varDispatchGs = groupSizeVar->getDispatchs()[0];

    auto crossThreadDataSize = varDispatchGs->getIndirectData()->getCrossThreadDataSize();
    EXPECT_EQ(crossThreadInitSize, crossThreadDataSize);
    auto &offsets = varDispatchGs->getIndirectDataOffsets();

    ze_mutable_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};

    mutableCommandsDesc.pNext = &groupSizeDesc;

    uint32_t mutatedGroupSizeX = 4;
    uint32_t mutatedGroupSizeY = 2;
    uint32_t mutatedGroupSizeZ = 1;

    groupSizeDesc.commandId = commandId;
    groupSizeDesc.groupSizeX = mutatedGroupSizeX;
    groupSizeDesc.groupSizeY = mutatedGroupSizeY;
    groupSizeDesc.groupSizeZ = mutatedGroupSizeZ;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *payloadBase = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject)->getCpuBase();

    uint32_t *lwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.enqLocalWorkSize));
    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkSize));
    uint32_t *workDimBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.workDimensions));

    EXPECT_EQ(this->testGroupCount.groupCountX * mutatedGroupSizeX, gwsBuffer[0]);
    EXPECT_EQ(this->testGroupCount.groupCountY * mutatedGroupSizeY, gwsBuffer[1]);
    EXPECT_EQ(this->testGroupCount.groupCountZ * mutatedGroupSizeZ, gwsBuffer[2]);

    EXPECT_EQ(mutatedGroupSizeX, lwsBuffer[0]);
    EXPECT_EQ(mutatedGroupSizeY, lwsBuffer[1]);
    EXPECT_EQ(mutatedGroupSizeZ, lwsBuffer[2]);

    EXPECT_EQ(2u, *workDimBuffer);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenKernelDispatchIsSelectedToMutateGlobalOffsetThenUpdatePayloadUponMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    dispatchTraits.globalWorkOffset[0] = this->crossThreadOffset;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto globalOffsets = getVariableList(commandId, L0::MCL::VariableType::globalOffset, nullptr);
    ASSERT_EQ(1u, globalOffsets.size());
    auto globalOffsetVar = globalOffsets[0];
    auto varDispatchGo = globalOffsetVar->getDispatchs()[0];

    auto crossThreadDataSize = varDispatchGo->getIndirectData()->getCrossThreadDataSize();
    EXPECT_EQ(crossThreadInitSize, crossThreadDataSize);
    auto &offsets = varDispatchGo->getIndirectDataOffsets();

    ze_mutable_global_offset_exp_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXP_DESC};

    mutableCommandsDesc.pNext = &globalOffsetDesc;

    uint32_t mutatedGlobalOffsetX = 2;
    uint32_t mutatedGlobalOffsetY = 3;
    uint32_t mutatedGlobalOffsetZ = 5;

    globalOffsetDesc.commandId = commandId;
    globalOffsetDesc.offsetX = mutatedGlobalOffsetX;
    globalOffsetDesc.offsetY = mutatedGlobalOffsetY;
    globalOffsetDesc.offsetZ = mutatedGlobalOffsetZ;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *payloadBase = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject)->getCpuBase();

    uint32_t *globalOffsetBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkOffset));

    EXPECT_EQ(mutatedGlobalOffsetX, globalOffsetBuffer[0]);
    EXPECT_EQ(mutatedGlobalOffsetY, globalOffsetBuffer[1]);
    EXPECT_EQ(mutatedGlobalOffsetZ, globalOffsetBuffer[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndInlineKernelAppendedWhenKernelDispatchIsSelectedToMutateThenUpdateInlineUponMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE | ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.passInlineData = 1;
    this->crossThreadOffset = 8;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    dispatchTraits.enqueuedLocalWorkSize[0] = this->crossThreadOffset;
    dispatchTraits.globalWorkSize[0] = dispatchTraits.enqueuedLocalWorkSize[0] + 3 * sizeof(uint32_t);
    dispatchTraits.numWorkGroups[0] = dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t);
    dispatchTraits.globalWorkOffset[0] = dispatchTraits.numWorkGroups[0] + 3 * sizeof(uint32_t);
    dispatchTraits.workDim = dispatchTraits.globalWorkOffset[0] + 3 * sizeof(uint32_t);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto groupCounts = getVariableList(commandId, L0::MCL::VariableType::groupCount, nullptr);
    ASSERT_EQ(1u, groupCounts.size());
    auto groupCountVar = groupCounts[0];
    auto varDispatchGc = groupCountVar->getDispatchs()[0];

    auto groupSizes = getVariableList(commandId, L0::MCL::VariableType::groupSize, nullptr);
    ASSERT_EQ(1u, groupSizes.size());
    auto groupSizeVar = groupSizes[0];
    auto varDispatchGs = groupSizeVar->getDispatchs()[0];

    auto globalOffsets = getVariableList(commandId, L0::MCL::VariableType::globalOffset, nullptr);
    ASSERT_EQ(1u, globalOffsets.size());
    auto globalOffsetVar = globalOffsets[0];
    auto varDispatchGo = globalOffsetVar->getDispatchs()[0];

    EXPECT_EQ(varDispatchGc, varDispatchGs);
    EXPECT_EQ(varDispatchGc, varDispatchGo);

    auto crossThreadDataSize = varDispatchGc->getIndirectData()->getCrossThreadDataSize();
    EXPECT_EQ(crossThreadInitSize - mutableCommandList->inlineDataSize, crossThreadDataSize);
    auto &offsets = varDispatchGc->getIndirectDataOffsets();

    ze_mutable_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
    ze_mutable_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    ze_mutable_global_offset_exp_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXP_DESC};

    mutableCommandsDesc.pNext = &groupCountDesc;

    ze_group_count_t mutatedGroupCount = {8, 2, 1};
    uint32_t mutatedGroupSizeX = 4;
    uint32_t mutatedGroupSizeY = 1;
    uint32_t mutatedGroupSizeZ = 4;

    uint32_t mutatedGlobalOffsetX = 2;
    uint32_t mutatedGlobalOffsetY = 3;
    uint32_t mutatedGlobalOffsetZ = 5;

    groupCountDesc.commandId = commandId;
    groupCountDesc.pGroupCount = &mutatedGroupCount;
    groupCountDesc.pNext = &groupSizeDesc;

    groupSizeDesc.commandId = commandId;
    groupSizeDesc.groupSizeX = mutatedGroupSizeX;
    groupSizeDesc.groupSizeY = mutatedGroupSizeY;
    groupSizeDesc.groupSizeZ = mutatedGroupSizeZ;
    groupSizeDesc.pNext = &globalOffsetDesc;

    globalOffsetDesc.commandId = commandId;
    globalOffsetDesc.offsetX = mutatedGlobalOffsetX;
    globalOffsetDesc.offsetY = mutatedGlobalOffsetY;
    globalOffsetDesc.offsetZ = mutatedGlobalOffsetZ;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *payloadBase = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject)->getCpuBase();
    void *inlineBase = mutableCommandList->mutableWalkerCmds[0]->getInlineDataPointer();

    void *currentBase = nullptr;
    size_t offset = 0;
    if (offsets.enqLocalWorkSize < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.enqLocalWorkSize;
    } else {
        currentBase = payloadBase;
        offset = offsets.enqLocalWorkSize - mutableCommandList->inlineDataSize;
    }
    uint32_t *lwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.globalWorkSize < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.globalWorkSize;
    } else {
        currentBase = payloadBase;
        offset = offsets.globalWorkSize - mutableCommandList->inlineDataSize;
    }
    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.numWorkGroups < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.numWorkGroups;
    } else {
        currentBase = payloadBase;
        offset = offsets.numWorkGroups - mutableCommandList->inlineDataSize;
    }
    uint32_t *numWorkGroupsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.globalWorkOffset < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.globalWorkOffset;
    } else {
        currentBase = payloadBase;
        offset = offsets.globalWorkOffset - mutableCommandList->inlineDataSize;
    }
    uint32_t *globalOffsetBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.workDimensions < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.workDimensions;
    } else {
        currentBase = payloadBase;
        offset = offsets.workDimensions - mutableCommandList->inlineDataSize;
    }
    uint32_t *workDimBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    EXPECT_EQ(mutatedGroupCount.groupCountX * mutatedGroupSizeX, gwsBuffer[0]);
    EXPECT_EQ(mutatedGroupCount.groupCountY * mutatedGroupSizeY, gwsBuffer[1]);
    EXPECT_EQ(mutatedGroupCount.groupCountZ * mutatedGroupSizeZ, gwsBuffer[2]);

    EXPECT_EQ(mutatedGroupSizeX, lwsBuffer[0]);
    EXPECT_EQ(mutatedGroupSizeY, lwsBuffer[1]);
    EXPECT_EQ(mutatedGroupSizeZ, lwsBuffer[2]);

    EXPECT_EQ(mutatedGroupCount.groupCountX, numWorkGroupsBuffer[0]);
    EXPECT_EQ(mutatedGroupCount.groupCountY, numWorkGroupsBuffer[1]);
    EXPECT_EQ(mutatedGroupCount.groupCountZ, numWorkGroupsBuffer[2]);

    EXPECT_EQ(3u, *workDimBuffer);

    EXPECT_EQ(mutatedGlobalOffsetX, globalOffsetBuffer[0]);
    EXPECT_EQ(mutatedGlobalOffsetY, globalOffsetBuffer[1]);
    EXPECT_EQ(mutatedGlobalOffsetZ, globalOffsetBuffer[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndEventsSelectedToMutateWhenAppendingWithNoSigalAndNoWaitEventThenNoMutationVariableCreated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT | ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    EXPECT_EQ(0u, signalEvents.size());

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    EXPECT_EQ(0u, waitEvents.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndSignalPlainEventSelectedWhenAppendingWithSignalEventThenMutationIsPerformed) {
    using WalkerType = typename FamilyType::PorWalkerType;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto originalEvent = createTestEvent(false, false, false, false);
    auto originalEventAddress = originalEvent->getGpuAddress(this->device);

    auto mutatedEvent = createTestEvent(false, false, false, false);
    auto newEventAddress = mutatedEvent->getGpuAddress(this->device);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    ASSERT_EQ(1u, mutableCommandList->mutableWalkerCmds.size());
    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());

    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(originalEventAddress, walkerPostSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(newEventAddress, walkerPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListOnDcFlushPlatformAndSignalEventWithSignalScopeSelectedWhenAppendingWithSignalEventThenMutationIsPerformedOnDcFlushCommand) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto mockBaseCmdList = static_cast<L0::ult::MockCommandList *>(this->mutableCommandList.get()->base);
    mockBaseCmdList->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto originalEvent = createTestEvent(false, true, false, false);
    auto originalEventAddress = originalEvent->getGpuAddress(this->device);

    auto mutatedEvent = createTestEvent(false, true, false, false);
    auto newEventAddress = mutatedEvent->getGpuAddress(this->device);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    ASSERT_EQ(1u, mutableCommandList->mutablePipeControlCmds.size());
    auto mockMutablePipeControl = static_cast<MockMutablePipeControlHw<FamilyType> *>(mutableCommandList->mutablePipeControlCmds[0].get());
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(mockMutablePipeControl->pipeControl);

    auto postSyncAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl);
    EXPECT_EQ(originalEventAddress, postSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    postSyncAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl);
    EXPECT_EQ(newEventAddress, postSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListOnDcFlushPlatformAndSignalEventWithTimestampSignalScopeSelectedWhenAppendingWithSignalEventThenMutationIsPerformedOnRegisterCommand) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto mockBaseCmdList = static_cast<L0::ult::MockCommandList *>(this->mutableCommandList.get()->base);
    mockBaseCmdList->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto originalEvent = createTestEvent(false, true, true, false);
    auto originalEventAddress = originalEvent->getGpuAddress(this->device);

    auto mutatedEvent = createTestEvent(false, true, true, false);
    auto newEventAddress = mutatedEvent->getGpuAddress(this->device);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_NE(0u, mutableCommandList->mutableStoreRegMemCmds.size());

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());
    EXPECT_EQ(signalEvents[0]->getStoreRegMemList().size(), mutableCommandList->mutableStoreRegMemCmds.size());

    MI_STORE_REGISTER_MEM *storeRegMemCmd = nullptr;
    for (auto &mutableStoreRegMem : mutableCommandList->mutableStoreRegMemCmds) {
        auto mockMutableStoreRegMem = static_cast<MockMutableStoreRegisterMemHw<FamilyType> *>(mutableStoreRegMem.get());
        storeRegMemCmd = reinterpret_cast<MI_STORE_REGISTER_MEM *>(mockMutableStoreRegMem->storeRegMem);
        if (storeRegMemCmd->getMemoryAddress() == originalEventAddress) {
            break;
        }
    }
    ASSERT_NE(nullptr, storeRegMemCmd);

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(newEventAddress, storeRegMemCmd->getMemoryAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithWaitRegularEventWhenMutateIntoEventThenDataIsUpdatedAndCommandChanged) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto signalEvent = createTestEvent(false, false, false, false);
    auto signalEventHandle = signalEvent->toHandle();
    auto event = createTestEvent(false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT | ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, signalEventHandle, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);

    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithWaitRegularTimestampEventWhenMutateIntoEventThenDataIsUpdatedAndCommandChanged) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = createTestEvent(false, false, true, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, true, false);
    auto newEventHandle = newEvent->toHandle();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);

    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithWaitRegularEventWhenNoopAndMutateIntoEventThenDataIsUpdatedAndCommandNoopedAndRestored) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    uint8_t semWaitNoop[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopEventHandle = nullptr;

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);

    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &noopEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_EQ(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoop, sizeof(MI_SEMAPHORE_WAIT)));

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithTwoWaitRegularEventWhenNoopFirstAndMutateSecondThenDataIsUpdatedAndCommandsAreNoopedAndUpdated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    uint8_t semWaitNoop[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(false, false, true, false);
    auto eventHandle = event->toHandle();
    auto event2 = createTestEvent(false, false, true, false);
    auto eventHandle2 = event2->toHandle();
    auto newEvent = createTestEvent(false, false, true, false);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopEventHandle = nullptr;

    ze_event_handle_t appendEvents[] = {eventHandle, eventHandle2};
    ze_event_handle_t mutateEvents[] = {noopEventHandle, newEventHandle};
    ze_event_handle_t restoreFirstEvents[] = {eventHandle, newEventHandle};

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 2, appendEvents, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    ASSERT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);
    EXPECT_EQ(2u, eventAllocationIt->refCount); // two wait events from the same pool added

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(2u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    auto waitEvent2Var = waitEvents[1];
    ASSERT_EQ(1u, waitEvent2Var->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);

    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());

    auto mutableSemWait2 = waitEvent2Var->getSemWaitList()[0];
    auto mockMutableSemWait2 = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait2);
    auto semWait2Cmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait2->semWait);

    auto waitAddress2 = event2->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress2, semWait2Cmd->getSemaphoreGraphicsAddress());

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 2, mutateEvents);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    ASSERT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);
    EXPECT_EQ(1u, eventAllocationIt->refCount); // one event was nooped, one was updated with event from the same pool

    // semaphore wait command for event is nooped
    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoop, sizeof(MI_SEMAPHORE_WAIT)));

    // semaphore wait command for event2 is updated
    waitAddress2 = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress2, semWait2Cmd->getSemaphoreGraphicsAddress());

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 2, restoreFirstEvents);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    ASSERT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);
    EXPECT_EQ(2u, eventAllocationIt->refCount); // first event was restored, second event remains the same as after mutation

    // semaphore wait command for first event was restored
    waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());

    // semaphore wait command for mutated event remains the same
    EXPECT_EQ(waitAddress2, semWait2Cmd->getSemaphoreGraphicsAddress());
}

using MutableCommandListInOrderTest = Test<MutableCommandListFixture<true>>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenCounterBasedEventWhenAppendLaunchKernelThenGetDeviceCounterAllocIsAddedForResidency) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    createTestEvent(true, false, false, false);
    ze_event_handle_t hEvent = this->eventHandles[0];

    MockGraphicsAllocation counterDeviceAlloc(this->device->getRootDeviceIndex(), nullptr, 0x1);
    auto inOrderExecInfo = NEO::InOrderExecInfo::createFromExternalAllocation(*device->getNEODevice(), &counterDeviceAlloc, 0x1, &counterDeviceAlloc, 0, 1, 1, 1);
    this->events[0]->updateInOrderExecState(inOrderExecInfo, 1, 0);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &hEvent, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    const auto &addedAllocations = whiteBoxAllocations.addedAllocations;
    EXPECT_FALSE(addedAllocations.empty());
    auto it = std::find_if(addedAllocations.begin(), addedAllocations.end(), [&counterDeviceAlloc](const L0::MCL::AllocationReference &ref) {
        return ref.allocation == &counterDeviceAlloc;
    });
    EXPECT_NE(it, addedAllocations.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenCounterBasedEventFromPeerDeviceWhenAppendLaunchKernelThenGetPeerDeviceCounterAllocIsAddedForResidency) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    createTestEvent(true, false, false, false);
    ze_event_handle_t hEvent = this->eventHandles[0];

    uint32_t peerDeviceIndex = this->device->getRootDeviceIndex() + 1;
    MockGraphicsAllocation peerCounterDeviceAlloc(peerDeviceIndex, reinterpret_cast<void *>(0x1234), 0x0u);
    auto inOrderExecInfo = NEO::InOrderExecInfo::createFromExternalAllocation(*device->getNEODevice(), &peerCounterDeviceAlloc, 0x1, &peerCounterDeviceAlloc, 0, 1, 1, 1);
    this->events[0]->updateInOrderExecState(inOrderExecInfo, 1, 0);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &hEvent, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    const auto &addedAllocations = whiteBoxAllocations.addedAllocations;
    EXPECT_FALSE(addedAllocations.empty());
    auto it = std::find_if(addedAllocations.begin(), addedAllocations.end(), [&peerCounterDeviceAlloc](const L0::MCL::AllocationReference &ref) {
        return ref.allocation->getGpuAddress() == peerCounterDeviceAlloc.getGpuAddress();
    });
    EXPECT_NE(it, addedAllocations.end());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListWhenAppendingKernelWithCbSignalEventAndMutateItThenExpectNewEventGetCmdListInOrderExecInfo) {
    auto mockBaseCmdList = static_cast<L0::ult::MockCommandList *>(this->mutableCommandList.get()->base);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    createTestEvent(true, false, false, false);
    auto newEvent = createTestEvent(true, false, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockBaseCmdList->inOrderExecInfo.get(), newEvent->getInOrderExecInfo().get());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListWhenAppendingKernelWithSignalEventCbTimestampAndMutateItThenExpectPatchNewEventAddress) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(true, false, true, false);
    auto newEvent = createTestEvent(true, false, true, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    auto baseGpuVa = event->getGpuAddress(this->device);

    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockBaseCmdListHw->inOrderExecInfo.get(), newEvent->getInOrderExecInfo().get());

    baseGpuVa = newEvent->getGpuAddress(this->device);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListOnDcFlushPlatformWhenAppendingKernelWithSignalEventCbSignalScopeTimestampAndMutateItThenExpectPatchNewEventAddress) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);
    mockBaseCmdListHw->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(true, true, true, false);
    auto newEvent = createTestEvent(true, true, true, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    auto baseGpuVa = event->getGpuAddress(this->device);

    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(mockBaseCmdListHw->inOrderExecInfo.get(), newEvent->getInOrderExecInfo().get());

    baseGpuVa = newEvent->getGpuAddress(this->device);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListOnDcFlushPlatformWhenAppendingKernelWithSignalEventCbSignalScopeAndMutateItThenExpectPatchNewEventAddress) {
    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);
    mockBaseCmdListHw->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(true, true, false, false);
    auto newEvent = createTestEvent(true, true, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(nullptr, event->getAllocation(this->device));

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(nullptr, newEvent->getAllocation(this->device));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListWhenAppendingKernelWithSignalEventRegularAndMutateItThenExpectPatchNewEventAddress) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerType = typename FamilyType::PorWalkerType;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(false, false, false, false);
    auto newEvent = createTestEvent(false, false, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    auto baseGpuVa = event->getGpuAddress(this->device);
    auto completionGpuVa = event->getCompletionFieldGpuAddress(this->device);

    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    MI_STORE_DATA_IMM *sdiCmd = nullptr;
    MI_SEMAPHORE_WAIT *semCmd = nullptr;
    if (mockBaseCmdListHw->duplicatedInOrderCounterStorageEnabled == false) {
        ASSERT_EQ(1u, signalEvents[0]->getStoreDataImmList().size());
        auto mutableSdi = signalEvents[0]->getStoreDataImmList()[0];
        auto mockMutableSdi = static_cast<MockMutableStoreDataImmHw<FamilyType> *>(mutableSdi);
        sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(mockMutableSdi->storeDataImm);
        EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());

        ASSERT_EQ(1u, signalEvents[0]->getSemWaitList().size());
        auto mutableSem = signalEvents[0]->getSemWaitList()[0];
        auto mockMutableSem = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSem);
        semCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSem->semWait);
        EXPECT_EQ(completionGpuVa, semCmd->getSemaphoreGraphicsAddress());
    }

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    baseGpuVa = newEvent->getGpuAddress(this->device);
    completionGpuVa = newEvent->getCompletionFieldGpuAddress(this->device);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    if (mockBaseCmdListHw->duplicatedInOrderCounterStorageEnabled == false) {
        EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());
        EXPECT_EQ(completionGpuVa, semCmd->getSemaphoreGraphicsAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListWhenAppendingKernelWithSignalEventRegularTimestampAndMutateItThenExpectPatchNewEventAddress) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using WalkerType = typename FamilyType::PorWalkerType;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(false, false, true, false);
    auto newEvent = createTestEvent(false, false, true, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    auto baseGpuVa = event->getGpuAddress(this->device);
    auto completionGpuVa = event->getCompletionFieldGpuAddress(this->device);

    auto walkerCmd = reinterpret_cast<WalkerType *>(mutableCommandList->mutableWalkerCmds[0]->getWalkerCmdPointer());
    auto walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    MI_STORE_DATA_IMM *sdiCmd = nullptr;
    MI_SEMAPHORE_WAIT *semCmd = nullptr;
    if (mockBaseCmdListHw->duplicatedInOrderCounterStorageEnabled == false) {
        ASSERT_EQ(1u, signalEvents[0]->getStoreDataImmList().size());
        auto mutableSdi = signalEvents[0]->getStoreDataImmList()[0];
        auto mockMutableSdi = static_cast<MockMutableStoreDataImmHw<FamilyType> *>(mutableSdi);
        sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(mockMutableSdi->storeDataImm);
        EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());

        ASSERT_EQ(1u, signalEvents[0]->getSemWaitList().size());
        auto mutableSem = signalEvents[0]->getSemWaitList()[0];
        auto mockMutableSem = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSem);
        semCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSem->semWait);
        EXPECT_EQ(completionGpuVa, semCmd->getSemaphoreGraphicsAddress());
    }

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    baseGpuVa = newEvent->getGpuAddress(this->device);
    completionGpuVa = newEvent->getCompletionFieldGpuAddress(this->device);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(baseGpuVa, walkerPostSyncAddress);

    if (mockBaseCmdListHw->duplicatedInOrderCounterStorageEnabled == false) {
        EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());
        EXPECT_EQ(completionGpuVa, semCmd->getSemaphoreGraphicsAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListOnDcFlushPlatformWhenAppendingKernelWithSignalEventRegularSignalScopeAndMutateItThenExpectPatchNewEventAddress) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);
    mockBaseCmdListHw->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(false, true, false, false);
    auto newEvent = createTestEvent(false, true, false, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());

    auto baseGpuVa = event->getGpuAddress(this->device);
    auto completionGpuVa = event->getCompletionFieldGpuAddress(this->device);

    MI_STORE_DATA_IMM *sdiCmd = nullptr;
    MI_SEMAPHORE_WAIT *semCmd = nullptr;
    PIPE_CONTROL *pipeControlCmd = nullptr;

    ASSERT_EQ(1u, signalEvents[0]->getStoreDataImmList().size());
    auto mutableSdi = signalEvents[0]->getStoreDataImmList()[0];
    auto mockMutableSdi = static_cast<MockMutableStoreDataImmHw<FamilyType> *>(mutableSdi);
    sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(mockMutableSdi->storeDataImm);
    EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());

    ASSERT_EQ(1u, signalEvents[0]->getSemWaitList().size());
    auto mutableSem = signalEvents[0]->getSemWaitList()[0];
    auto mockMutableSem = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSem);
    semCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSem->semWait);
    EXPECT_EQ(completionGpuVa, semCmd->getSemaphoreGraphicsAddress());

    ASSERT_EQ(1u, mutableCommandList->mutablePipeControlCmds.size());
    auto mockMutablePipeControl = static_cast<MockMutablePipeControlHw<FamilyType> *>(mutableCommandList->mutablePipeControlCmds[0].get());
    pipeControlCmd = reinterpret_cast<PIPE_CONTROL *>(mockMutablePipeControl->pipeControl);
    auto postSyncAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);
    EXPECT_EQ(baseGpuVa, postSyncAddress);

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    baseGpuVa = newEvent->getGpuAddress(this->device);
    completionGpuVa = newEvent->getCompletionFieldGpuAddress(this->device);

    EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());
    EXPECT_EQ(completionGpuVa, semCmd->getSemaphoreGraphicsAddress());
    postSyncAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);
    EXPECT_EQ(baseGpuVa, postSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListOnDcFlushPlatformWhenAppendingKernelWithSignalEventRegularSignalScopeTimestampAndMutateItThenExpectPatchNewEventAddress) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);
    mockBaseCmdListHw->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(false, true, true, false);
    auto newEvent = createTestEvent(false, true, true, false);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, this->eventHandles[0], 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto signalEvents = getVariableList(commandId, L0::MCL::VariableType::signalEvent, nullptr);
    ASSERT_EQ(1u, signalEvents.size());
    auto baseGpuVa = event->getGpuAddress(this->device);
    auto completionGpuVa = event->getCompletionFieldGpuAddress(this->device);

    MI_STORE_DATA_IMM *sdiCmd = nullptr;
    MI_STORE_REGISTER_MEM *srmBaseCmd = nullptr;
    MI_STORE_REGISTER_MEM *srmComplCmd = nullptr;
    MI_SEMAPHORE_WAIT *semCmd = nullptr;

    ASSERT_EQ(1u, signalEvents[0]->getStoreDataImmList().size());
    auto mutableSdi = signalEvents[0]->getStoreDataImmList()[0];
    auto mockMutableSdi = static_cast<MockMutableStoreDataImmHw<FamilyType> *>(mutableSdi);
    sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(mockMutableSdi->storeDataImm);
    EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());

    ASSERT_EQ(1u, signalEvents[0]->getSemWaitList().size());
    auto mutableSem = signalEvents[0]->getSemWaitList()[0];
    auto mockMutableSem = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSem);
    semCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSem->semWait);
    EXPECT_EQ(completionGpuVa, semCmd->getSemaphoreGraphicsAddress());

    for (auto &mutableStoreRegMem : signalEvents[0]->getStoreRegMemList()) {
        auto mockMutableStoreRegMem = static_cast<MockMutableStoreRegisterMemHw<FamilyType> *>(mutableStoreRegMem);
        auto storeRegMemCmd = reinterpret_cast<MI_STORE_REGISTER_MEM *>(mockMutableStoreRegMem->storeRegMem);
        if (storeRegMemCmd->getMemoryAddress() == baseGpuVa) {
            srmBaseCmd = storeRegMemCmd;
        }
        if (storeRegMemCmd->getMemoryAddress() == completionGpuVa) {
            srmComplCmd = storeRegMemCmd;
        }
    }
    ASSERT_NE(nullptr, srmBaseCmd);
    ASSERT_NE(nullptr, srmComplCmd);

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    baseGpuVa = newEvent->getGpuAddress(this->device);
    completionGpuVa = newEvent->getCompletionFieldGpuAddress(this->device);

    EXPECT_EQ(completionGpuVa, sdiCmd->getAddress());
    EXPECT_EQ(completionGpuVa, semCmd->getSemaphoreGraphicsAddress());
    EXPECT_EQ(completionGpuVa, srmComplCmd->getMemoryAddress());
    EXPECT_EQ(baseGpuVa, srmBaseCmd->getMemoryAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitCbEventBelongingToCmdListWhenMutateIntoEventFromDifferentCmdListThenDataIsUpdatedAndCommandResotred) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool qwordInUse = mutableCommandList->isQwordInOrderCounter();

    uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto signalEvent = createTestEvent(false, false, false, false);
    auto signalEventHandle = signalEvent->toHandle();
    auto event = createTestEvent(true, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = createMutableCmdList();
    // attach event 2 to the external command list
    auto result = externalCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // attach event 1 to the command list
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT | ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, signalEventHandle, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    size_t expectedLriSize = qwordInUse ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;
    if (qwordInUse) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitAddress = newEvent->getInOrderExecInfo()->getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());

    if (qwordInUse) {
        constexpr uint32_t firstRegister = 0x2600;
        constexpr uint32_t secondRegister = 0x2604;

        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitCbEventBelongingToCmdListWhenMutateIntoEventFromSameCmdListThenDataIsUpdatedAndNoopRemain) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool qwordInUse = mutableCommandList->isQwordInOrderCounter();

    uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(true, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    // attach events 1 & 2 to the command list
    auto result = mutableCommandList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    size_t expectedLriSize = qwordInUse ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;
    if (qwordInUse) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));
    if (qwordInUse) {
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitCbEventBelongingToDifferentCmdListWhenMutateIntoEventOwnedToCurrentCmdListThenDataIsUpdatedAndCommandNooped) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool qwordInUse = mutableCommandList->isQwordInOrderCounter();

    uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(true, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = createMutableCmdList();
    // attach event 1 to the external command list
    auto result = externalCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // attach event 2 to the command list
    result = mutableCommandList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    size_t expectedLriSize = qwordInUse ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getInOrderExecInfo()->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;

    if (qwordInUse) {
        constexpr uint32_t firstRegister = 0x2600;
        constexpr uint32_t secondRegister = 0x2604;

        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    if (qwordInUse) {
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitCbEventBelongingToDifferentCmdListWhenNoopedAndMutatedBackThenDataIsUpdatedAndCommandNoopedAndRestored) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool qwordInUse = mutableCommandList->isQwordInOrderCounter();

    uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(true, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopHandle = nullptr;

    auto externalCmdList = createMutableCmdList();
    // attach event 1 to the external command list
    auto result = externalCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    size_t expectedLriSize = qwordInUse ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getInOrderExecInfo()->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;

    constexpr uint32_t firstRegister = 0x2600;
    constexpr uint32_t secondRegister = 0x2604;

    if (qwordInUse) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &noopHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));
    if (qwordInUse) {
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());
    if (qwordInUse) {
        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getInOrderExecInfo()->getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();

    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());
    if (qwordInUse) {
        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitRegularEventWhenWhenNoopedAndMutatedIntoDifferentEventThenDataIsUpdatedAndCommandNoopedAndRestoredIntoDifferentCompletion) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopHandle = nullptr;

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &whiteBoxAllocations = static_cast<L0::ult::WhiteBoxMutableResidencyAllocations &>(mutableCommandList->mutableAllocations);
    auto eventAllocation = event->getAllocation(this->device);
    auto eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                          whiteBoxAllocations.addedAllocations.end(),
                                          [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                              return ref.allocation == eventAllocation;
                                          });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &noopHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_EQ(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    eventAllocation = newEvent->getAllocation(this->device);
    eventAllocationIt = std::find_if(whiteBoxAllocations.addedAllocations.begin(),
                                     whiteBoxAllocations.addedAllocations.end(),
                                     [&eventAllocation](const L0::MCL::AllocationReference &ref) {
                                         return ref.allocation == eventAllocation;
                                     });
    EXPECT_NE(whiteBoxAllocations.addedAllocations.end(), eventAllocationIt);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, semWaitCmd->getSemaphoreGraphicsAddress());
}

} // namespace ult
} // namespace L0

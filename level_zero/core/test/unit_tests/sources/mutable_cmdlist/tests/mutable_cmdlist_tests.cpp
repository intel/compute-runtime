/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_load_register_imm_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_pipe_control_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_semaphore_wait_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_store_data_imm_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_store_register_mem_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_variable.h"

namespace L0 {
namespace ult {

using MutableCommandListTest = Test<MutableCommandListFixture<false, -1>>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenInvalidProductWhenCreatingCommandListThenNoObjectCreated) {
    ze_result_t returnValue;

    auto mcl = MutableCommandList::create(NEO::maxProductEnumValue, device, this->engineGroupType, 0, returnValue, false);
    EXPECT_EQ(nullptr, mcl);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, returnValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenEstimatedNumberOfCommandsWhenCreatingCommandListThenEstimateIsPropagatedToCommandContainer) {
    ze_result_t returnValue;

    constexpr uint32_t estimatedNumberOfCommands = 8u;
    auto mcl = MutableCommandList::create(productFamily, device, this->engineGroupType, 0, returnValue, false, estimatedNumberOfCommands);
    ASSERT_NE(nullptr, mcl);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(estimatedNumberOfCommands, mcl->getCmdContainer().getEstimatedNumberOfCommands());

    mcl->destroy();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenCallingIsMutableExpThenTrueIsReturned) {
    ze_bool_t isMutable = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, mutableCommandList->getBase()->isMutableExp(&isMutable));
    EXPECT_TRUE(static_cast<bool>(isMutable));
}

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

    ASSERT_NE(0u, mutableCommandList->kernelMutations.size());
    ASSERT_NE(0u, mutableCommandList->eventMutations.size());

    ze_mutable_command_exp_flags_t expectedFlags = ZE_MUTABLE_COMMAND_EXP_FLAG_FORCE_UINT32 & (~ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION);
    EXPECT_EQ(expectedFlags, mutableCommandList->nextMutationFlags);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenCommandListResetThenZeroContainers) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);

    ASSERT_NE(0u, mutableCommandList->kernelMutations.size());
    ASSERT_NE(0u, mutableCommandList->eventMutations.size());

    ze_mutable_command_exp_flags_t expectedFlags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    EXPECT_EQ(expectedFlags, mutableCommandList->nextMutationFlags);

    result = mutableCommandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, mutableCommandList->nextCommandId);
    EXPECT_FALSE(mutableCommandList->nextAppendKernelMutable);
    EXPECT_EQ(0u, mutableCommandList->kernelMutations.size());
    EXPECT_EQ(0u, mutableCommandList->eventMutations.size());
    EXPECT_EQ(0u, mutableCommandList->nextMutationFlags);
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
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

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
    auto kernelDispatch = mutableCommandList->dispatches[0].get();

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(mutableCommandList->updatedCommandList);

    mutableCommandList->toggleCommandListUpdated();
    EXPECT_TRUE(mutableCommandList->updatedCommandList);

    auto &bufferVarMutDesc = mutation.variables.kernelArguments[0];
    auto &bufferInternalDesc = bufferVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, bufferInternalDesc.type);
    auto gpuVaPatchFullAddress = reinterpret_cast<void *>(bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto &valueVarMutDesc = mutation.variables.kernelArguments[1];
    auto &valueInternalDesc = valueVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::value, valueInternalDesc.type);
    auto immediatePatchFullAddress = reinterpret_cast<void *>(valueVarMutDesc.kernelArgumentVariable->getValueUsages().statelessWithoutOffset[0]);
    memcpy(&valueVariablePatchValue, immediatePatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(value1, valueVariablePatchValue);

    auto &slmVarMutDesc = mutation.variables.kernelArguments[2];
    auto &slmInternalDesc = slmVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::slmBuffer, slmInternalDesc.type);

    auto &slm2VarMutDesc = mutation.variables.kernelArguments[3];
    auto &slm2InternalDesc = slm2VarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::slmBuffer, slm2InternalDesc.type);
    auto slmPatchFullAddress = reinterpret_cast<void *>(slm2VarMutDesc.kernelArgumentVariable->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&slmVariablePatchValue, slmPatchFullAddress, sizeof(uint32_t));
    EXPECT_EQ(static_cast<uint32_t>(slm1arg1), slmVariablePatchValue);

    EXPECT_EQ(alignUp(slm1arg1 + slm1arg2, MemoryConstants::kiloByte), static_cast<size_t>(kernelDispatch->slmTotalSizePerThreadGroup));

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

    EXPECT_EQ(alignUp(slm2arg1 + slm2arg2, MemoryConstants::kiloByte), static_cast<size_t>(kernelDispatch->slmTotalSizePerThreadGroup));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenAppendingKernelWithImageArgumentThenNoVariableCreated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    // set kernel arg 0 => image, buffer
    resizeKernelArg(2);
    NEO::ArgDescriptor kernelArgImage = {NEO::ArgDescriptor::argTImage};
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = kernelArgImage;

    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[1].getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrConstant;

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // only buffer created
    ASSERT_EQ(2u, mutation.variables.kernelArguments.size());
    // at index 1
    EXPECT_EQ(nullptr, mutation.variables.kernelArguments[0].kernelArgumentVariable);
    EXPECT_NE(nullptr, mutation.variables.kernelArguments[1].kernelArgumentVariable);

    void *buffer = reinterpret_cast<void *>(0x12345678);

    ze_mutable_kernel_argument_exp_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &buffer;

    // cannot mutate when variable is not created
    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenAppendingKernelWithOnlySlmArgumentThenSlmVariableCreated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    // set kernel arg 0 => slm
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::slmBuffer, kernelAllMask);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(1u, mutation.variables.kernelArguments.size());
    EXPECT_EQ(0u, mutation.variables.kernelArguments[0].argIndex);
    EXPECT_EQ(L0::MCL::VariableType::slmBuffer, mutation.variables.kernelArguments[0].kernelArgumentVariable->getType());
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
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

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
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0].getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrUnknown;
    prepareKernelArg(1, L0::MCL::VariableType::buffer, kernelAllMask);
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[1].getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrUnknown;

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

    auto &buffer1VarMutDesc = mutation.variables.kernelArguments[0];
    auto &buffer1InternalDesc = buffer1VarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, buffer1InternalDesc.type);
    auto gpuVa1PatchFullAddress = reinterpret_cast<void *>(buffer1VarMutDesc.kernelArgumentVariable->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVa1PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto &buffer2VarMutDesc = mutation.variables.kernelArguments[1];
    auto &buffer2InternalDesc = buffer2VarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, buffer2InternalDesc.type);
    auto gpuVa2PatchFullAddress = reinterpret_cast<void *>(buffer2VarMutDesc.kernelArgumentVariable->getBufferUsages().statelessWithoutOffset[0]);
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
            givenMutableCommandListAndKernelBufferNullArgumentsWhenAppendingKernelAndMutatingIntoNullThenAllocIdCountFromManagerDoesNotChange) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    uint64_t usmPatchAddressValue = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    void *nullSurface = nullptr;

    // set kernel arg 0 => buffer1
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), nullSurface);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &buffer0VarMutDesc = mutation.variables.kernelArguments[0];
    auto &buffer0InternalDesc = buffer0VarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, buffer0InternalDesc.type);
    auto gpuVa0PatchFullAddress = reinterpret_cast<void *>(buffer0VarMutDesc.kernelArgumentVariable->getBufferUsages().statelessWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVa0PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(0u, usmPatchAddressValue);
    EXPECT_EQ(undefined<uint32_t>, buffer0InternalDesc.allocIdMemoryManagerCounter);

    // update manager counter
    void *usm1 = allocateUsm(4096);
    ASSERT_NE(nullptr, usm1);

    ze_mutable_kernel_argument_exp_desc_t kernelBuffer1Arg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};

    kernelBuffer1Arg.argIndex = 0;
    kernelBuffer1Arg.argSize = sizeof(void *);
    kernelBuffer1Arg.commandId = commandId;
    kernelBuffer1Arg.pArgValue = nullSurface;
    mutableCommandsDesc.pNext = &kernelBuffer1Arg;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVa0PatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(0u, usmPatchAddressValue);
    EXPECT_EQ(undefined<uint32_t>, buffer0InternalDesc.allocIdMemoryManagerCounter);
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
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

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

    auto &bufferVarMutDesc = mutation.variables.kernelArguments[0];
    auto &bufferInternalDesc = bufferVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, bufferInternalDesc.type);
    ASSERT_NE(0u, bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset.size());
    auto gpuVaPatchFullAddress = reinterpret_cast<void *>(bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto &immediateVarMutDesc = mutation.variables.kernelArguments[1];
    auto &immediateInternalDesc = immediateVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::value, immediateInternalDesc.type);
    ASSERT_NE(0u, immediateVarMutDesc.kernelArgumentVariable->getValueUsages().commandBufferWithoutOffset.size());
    auto immediatePatchFullAddress = reinterpret_cast<void *>(immediateVarMutDesc.kernelArgumentVariable->getValueUsages().commandBufferWithoutOffset[0]);
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

    kernelBufferArg.pNext = nullptr;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndSystemSharedAllocationWhenMutatingIntoBufferThenPerformMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    void *usmSystem = nullptr, *usmAllocatedHost = nullptr;

    auto result = this->context->allocHostMem(&hostDesc, 4096, 4096, &usmAllocatedHost);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, usmAllocatedHost);

    usmSystem = reinterpret_cast<void *>(0x1000);

    // set kernel arg 0 => buffer
    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.passInlineData = 1;
    this->crossThreadOffset = 8;
    this->nextArgOffset = 16;
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), &usmSystem);

    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint64_t usmPatchAddressValue = 0;

    auto &bufferVarMutDesc = mutation.variables.kernelArguments[0];
    auto &bufferInternalDesc = bufferVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, bufferInternalDesc.type);
    ASSERT_NE(0u, bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset.size());
    auto gpuVaPatchFullAddress = reinterpret_cast<void *>(bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usmSystem), usmPatchAddressValue);
    // no graphics allocation present
    EXPECT_EQ(nullptr, bufferInternalDesc.bufferAlloc);

    ze_mutable_kernel_argument_exp_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};

    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = &usmAllocatedHost;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usmAllocatedHost), usmPatchAddressValue);

    // graphics allocation is present
    EXPECT_NE(nullptr, bufferInternalDesc.bufferAlloc);

    result = this->context->freeMem(usmAllocatedHost);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndHostPoolWhenMutationHostBufferOfSameAddressButDifferentIdThenPerformMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    debugManager.flags.EnableUsmAllocationPoolManager.set(0);
    debugManager.flags.EnableHostUsmAllocationPool.set(1);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.passInlineData = 1;

    uint64_t usmPatchAddressValue = 0;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    void *usm1 = nullptr, *usm2 = nullptr;

    result = this->context->allocHostMem(&hostDesc, 4096, 4096, &usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, usm1);

    result = this->context->allocHostMem(&hostDesc, 4096, 4096, &usm2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, usm2);

    // set kernel arg 0 => buffer
    this->crossThreadOffset = 8;
    this->nextArgOffset = 16;
    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernelAllMask);

    result = kernel->setArgBuffer(0, sizeof(void *), &usm1);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &bufferVarMutDesc = mutation.variables.kernelArguments[0];
    auto &bufferInternalDesc = bufferVarMutDesc.kernelArgumentVariable->getDesc();
    EXPECT_EQ(L0::MCL::VariableType::buffer, bufferInternalDesc.type);
    ASSERT_NE(0u, bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset.size());
    auto gpuVaPatchFullAddress = reinterpret_cast<void *>(bufferVarMutDesc.kernelArgumentVariable->getBufferUsages().commandBufferWithoutOffset[0]);
    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm1), usmPatchAddressValue);

    auto usm1AllocId = bufferInternalDesc.allocId;

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

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);

    auto usm2AllocId = bufferInternalDesc.allocId;
    EXPECT_EQ(usm1AllocId, usm2AllocId);

    auto oldUsm2 = usm2;

    result = this->context->freeMem(usm2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->context->allocHostMem(&hostDesc, 4096, 4096, &usm2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(oldUsm2, usm2);

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    memcpy(&usmPatchAddressValue, gpuVaPatchFullAddress, sizeof(uint64_t));
    EXPECT_EQ(reinterpret_cast<uint64_t>(usm2), usmPatchAddressValue);

    result = this->context->freeMem(usm2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = this->context->freeMem(usm1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenAppendingKernelWithPointerPrivateQualifierThenMutationVariableNotCreated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(commandId, mutableCommandList->nextCommandId);
    EXPECT_TRUE(mutableCommandList->nextAppendKernelMutable);
    auto &mutation = mutableCommandList->kernelMutations[commandId - 1];

    resizeKernelArg(1);
    prepareKernelArg(0, L0::MCL::VariableType::buffer, kernel1Bit);
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0].getTraits().addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::AddrPrivate;

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(1u, mutation.variables.kernelArguments.size());
    EXPECT_EQ(nullptr, mutation.variables.kernelArguments[0].kernelArgumentVariable);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoKernelArgumentsFlagSelectedWhenAppendingKernelAndMutatingArgumentsThenErrorIsReturned) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_kernel_argument_exp_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};

    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = nullptr;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoKernelArgumentAtGivenIndexWhenAppendingKernelAndMutatingArgumentsThenErrorIsReturned) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_kernel_argument_exp_desc_t kernelBufferArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};

    mutableCommandsDesc.pNext = &kernelBufferArg;

    kernelBufferArg.argIndex = 0;
    kernelBufferArg.argSize = sizeof(void *);
    kernelBufferArg.commandId = commandId;
    kernelBufferArg.pArgValue = nullptr;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenKernelWithoutLocalIDsIsDispatchedToMutateGroupCountThenUpdatePayloadUponMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    mockKernelImmData->kernelDescriptor->kernelAttributes.numLocalIdChannels = 0;
    dispatchTraits.globalWorkSize[0] = this->crossThreadOffset + 3 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[1] = this->crossThreadOffset + 4 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[2] = this->crossThreadOffset + 5 * sizeof(uint32_t);
    dispatchTraits.numWorkGroups[0] = dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t);
    dispatchTraits.numWorkGroups[1] = dispatchTraits.globalWorkSize[0] + 4 * sizeof(uint32_t);
    dispatchTraits.numWorkGroups[2] = dispatchTraits.globalWorkSize[0] + 5 * sizeof(uint32_t);
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
    auto varDispatchGc = groupCountVar->getDispatches()[0];

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

    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkSize[0]));
    uint32_t *numWorkGroupsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.numWorkGroups[0]));
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
            givenMutableCommandListAndNoGroupCountFlagSelectedWhenAppendingKernelAndMutatingGroupCountThenErrorIsReturned) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_group_count_exp_desc_t groupCountDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};

    mutableCommandsDesc.pNext = &groupCountDesc;

    ze_group_count_t mutatedGroupCount = {8, 2, 2};

    groupCountDesc.commandId = commandId;
    groupCountDesc.pGroupCount = &mutatedGroupCount;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenSingleDimensionImplicitArgsWhenKernelDispatchMutatesGroupCountThenOnlySingleOffsetsAreMutated) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    dispatchTraits.enqueuedLocalWorkSize[0] = this->crossThreadOffset;
    dispatchTraits.enqueuedLocalWorkSize[1] = undefined<CrossThreadDataOffset>;
    dispatchTraits.enqueuedLocalWorkSize[2] = undefined<CrossThreadDataOffset>;
    dispatchTraits.globalWorkSize[0] = dispatchTraits.enqueuedLocalWorkSize[0] + 2 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[1] = undefined<CrossThreadDataOffset>;
    dispatchTraits.globalWorkSize[2] = undefined<CrossThreadDataOffset>;
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
    auto varDispatchGs = groupSizeVar->getDispatches()[0];

    auto crossThreadDataSize = varDispatchGs->getIndirectData()->getCrossThreadDataSize();
    EXPECT_EQ(crossThreadInitSize, crossThreadDataSize);
    auto &offsets = varDispatchGs->getIndirectDataOffsets();

    ze_mutable_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};

    mutableCommandsDesc.pNext = &groupSizeDesc;

    uint32_t mutatedGroupSizeX = 4;
    uint32_t mutatedGroupSizeY = 9;
    uint32_t mutatedGroupSizeZ = 5;

    groupSizeDesc.commandId = commandId;
    groupSizeDesc.groupSizeX = mutatedGroupSizeX;
    groupSizeDesc.groupSizeY = mutatedGroupSizeY;
    groupSizeDesc.groupSizeZ = mutatedGroupSizeZ;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *payloadBase = mutableCommandList->getBase()->getCmdContainer().getIndirectHeap(NEO::IndirectHeapType::indirectObject)->getCpuBase();

    uint32_t *lwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.enqLocalWorkSize[0]));
    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkSize[0]));
    uint32_t *workDimBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.workDimensions));

    EXPECT_EQ(this->testGroupCount.groupCountX * mutatedGroupSizeX, gwsBuffer[0]);
    EXPECT_EQ(mutatedGroupSizeX, lwsBuffer[0]);
    EXPECT_EQ(3u, *workDimBuffer);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenKernelDispatchIsSelectedToMutateGroupSizeThenUpdatePayloadUponMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    dispatchTraits.enqueuedLocalWorkSize[0] = this->crossThreadOffset;
    dispatchTraits.enqueuedLocalWorkSize[1] = this->crossThreadOffset + sizeof(uint32_t);
    dispatchTraits.enqueuedLocalWorkSize[2] = this->crossThreadOffset + 2 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[0] = dispatchTraits.enqueuedLocalWorkSize[0] + 3 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[1] = dispatchTraits.enqueuedLocalWorkSize[1] + 3 * sizeof(uint32_t);
    dispatchTraits.globalWorkSize[2] = dispatchTraits.enqueuedLocalWorkSize[2] + 3 * sizeof(uint32_t);
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
    auto varDispatchGs = groupSizeVar->getDispatches()[0];

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

    uint32_t *lwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.enqLocalWorkSize[0]));
    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkSize[0]));
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
            givenMutableCommandListAndNoGroupSizeFlagSelectedWhenAppendingKernelAndMutatingGroupSizeThenErrorIsReturned) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_group_size_exp_desc_t groupSizeDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};

    mutableCommandsDesc.pNext = &groupSizeDesc;

    groupSizeDesc.commandId = commandId;
    groupSizeDesc.groupSizeX = 1;
    groupSizeDesc.groupSizeY = 1;
    groupSizeDesc.groupSizeZ = 2;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListWhenKernelDispatchIsSelectedToMutateGlobalOffsetThenUpdatePayloadUponMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    dispatchTraits.globalWorkOffset[0] = this->crossThreadOffset;
    dispatchTraits.globalWorkOffset[1] = this->crossThreadOffset + sizeof(uint32_t);
    dispatchTraits.globalWorkOffset[2] = this->crossThreadOffset + 2 * sizeof(uint32_t);

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto globalOffsets = getVariableList(commandId, L0::MCL::VariableType::globalOffset, nullptr);
    ASSERT_EQ(1u, globalOffsets.size());
    auto globalOffsetVar = globalOffsets[0];
    auto varDispatchGo = globalOffsetVar->getDispatches()[0];

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

    uint32_t *globalOffsetBuffer = reinterpret_cast<uint32_t *>(ptrOffset(payloadBase, offsets.globalWorkOffset[0]));

    EXPECT_EQ(mutatedGlobalOffsetX, globalOffsetBuffer[0]);
    EXPECT_EQ(mutatedGlobalOffsetY, globalOffsetBuffer[1]);
    EXPECT_EQ(mutatedGlobalOffsetZ, globalOffsetBuffer[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoGlobalOffsetFlagSelectedWhenAppendingKernelAndMutatingGlobalOffsetThenErrorIsReturned) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_mutable_global_offset_exp_desc_t globalOffsetDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXP_DESC};

    mutableCommandsDesc.pNext = &globalOffsetDesc;

    globalOffsetDesc.commandId = commandId;
    globalOffsetDesc.offsetX = 1;
    globalOffsetDesc.offsetY = 2;
    globalOffsetDesc.offsetZ = 3;

    result = mutableCommandList->updateMutableCommandsExp(&mutableCommandsDesc);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndInlineKernelAppendedWhenKernelDispatchIsSelectedToMutateThenUpdateInlineUponMutation) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE | ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.passInlineData = 1;
    this->crossThreadOffset = 8;

    auto &dispatchTraits = mockKernelImmData->kernelDescriptor->payloadMappings.dispatchTraits;
    fillOffsets(dispatchTraits.enqueuedLocalWorkSize, this->crossThreadOffset, 3);
    fillOffsets(dispatchTraits.globalWorkSize, dispatchTraits.enqueuedLocalWorkSize[0] + 3 * sizeof(uint32_t), 3);
    fillOffsets(dispatchTraits.numWorkGroups, dispatchTraits.globalWorkSize[0] + 3 * sizeof(uint32_t), 3);
    fillOffsets(dispatchTraits.globalWorkOffset, dispatchTraits.numWorkGroups[0] + 3 * sizeof(uint32_t), 3);
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
    auto varDispatchGc = groupCountVar->getDispatches()[0];

    auto groupSizes = getVariableList(commandId, L0::MCL::VariableType::groupSize, nullptr);
    ASSERT_EQ(1u, groupSizes.size());
    auto groupSizeVar = groupSizes[0];
    auto varDispatchGs = groupSizeVar->getDispatches()[0];

    auto globalOffsets = getVariableList(commandId, L0::MCL::VariableType::globalOffset, nullptr);
    ASSERT_EQ(1u, globalOffsets.size());
    auto globalOffsetVar = globalOffsets[0];
    auto varDispatchGo = globalOffsetVar->getDispatches()[0];

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
    if (offsets.enqLocalWorkSize[0] < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.enqLocalWorkSize[0];
    } else {
        currentBase = payloadBase;
        offset = offsets.enqLocalWorkSize[0] - mutableCommandList->inlineDataSize;
    }
    uint32_t *lwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.globalWorkSize[0] < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.globalWorkSize[0];
    } else {
        currentBase = payloadBase;
        offset = offsets.globalWorkSize[0] - mutableCommandList->inlineDataSize;
    }
    uint32_t *gwsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.numWorkGroups[0] < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.numWorkGroups[0];
    } else {
        currentBase = payloadBase;
        offset = offsets.numWorkGroups[0] - mutableCommandList->inlineDataSize;
    }
    uint32_t *numWorkGroupsBuffer = reinterpret_cast<uint32_t *>(ptrOffset(currentBase, offset));

    if (offsets.globalWorkOffset[0] < mutableCommandList->inlineDataSize) {
        currentBase = inlineBase;
        offset = offsets.globalWorkOffset[0];
    } else {
        currentBase = payloadBase;
        offset = offsets.globalWorkOffset[0] - mutableCommandList->inlineDataSize;
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

    auto originalEvent = createTestEvent(false, false, false, false, false);
    auto originalEventAddress = originalEvent->getGpuAddress(this->device);

    auto mutatedEvent = createTestEvent(false, false, false, false, false);
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

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, this->eventHandles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    walkerPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getWalkerActivePostSyncAddress(walkerCmd);
    EXPECT_EQ(newEventAddress, walkerPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoSignalEventFlagSelectedWhenAppendingKernelAndMutatingSignalEventThenErrorIsReturned) {
    auto originalEvent = createTestEvent(false, false, false, false, false);
    auto mutatedEvent = createTestEvent(false, false, false, false, false);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, originalEvent->toHandle(), 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->updateMutableCommandSignalEventExp(commandId, mutatedEvent->toHandle());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListOnDcFlushPlatformAndSignalEventWithSignalScopeSelectedWhenAppendingWithSignalEventThenMutationIsPerformedOnDcFlushCommand) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &productHelper = this->neoDevice->getProductHelper();
    if (productHelper.isL3FlushAfterPostSyncSupported()) {
        GTEST_SKIP();
    }

    auto mockBaseCmdList = static_cast<L0::ult::MockCommandList *>(this->mutableCommandList.get()->base);
    mockBaseCmdList->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto originalEvent = createTestEvent(false, true, false, false, false);
    auto originalEventAddress = originalEvent->getGpuAddress(this->device);

    auto mutatedEvent = createTestEvent(false, true, false, false, false);
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

    auto &productHelper = this->neoDevice->getProductHelper();
    if (productHelper.isL3FlushAfterPostSyncSupported()) {
        GTEST_SKIP();
    }
    auto mockBaseCmdList = static_cast<L0::ult::MockCommandList *>(this->mutableCommandList.get()->base);
    mockBaseCmdList->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto originalEvent = createTestEvent(false, true, true, false, false);
    auto originalEventAddress = originalEvent->getGpuAddress(this->device);

    auto mutatedEvent = createTestEvent(false, true, true, false, false);
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

    auto signalEvent = createTestEvent(false, false, false, false, false);
    auto signalEventHandle = signalEvent->toHandle();
    auto event = createTestEvent(false, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false, false);
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
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

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
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithWaitRegularTimestampEventWhenMutateIntoEventThenDataIsUpdatedAndCommandChanged) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = createTestEvent(false, false, true, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, true, false, false);
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
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithWaitRegularEventAndPrefetchEnabledWhenMutatedIntoDifferentEventThenDataIsUpdated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    debugManager.flags.EnableMemoryPrefetch.set(1);

    auto event = createTestEvent(false, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
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

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithWaitRegularEventWhenNoopAndMutateIntoEventThenDataIsUpdatedAndCommandNoopedAndRestored) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t semWaitNoop[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(false, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false, false);
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
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

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
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenAsyncMutationAndKernelWithWaitRegularEventWhenMutateIntoOtherEventThenDataIsUpdatedAndAsyncPatchlistDispatched) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    constexpr size_t semWaitSize = sizeof(MI_SEMAPHORE_WAIT);
    alignas(uint32_t) uint8_t semWaitData[semWaitSize] = {0};
    uint32_t *semWaitPtr = reinterpret_cast<uint32_t *>(semWaitData);

    auto event = createTestEvent(false, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    mutableCommandList->getBase()->setupPatchPreambleEnabled(true);

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
    EXPECT_TRUE(waitEventVar->getDesc().asyncMutation);
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto expectedAsyncPatchListGpuDst = mutableSemWait->getGpuDestinationAddress();
    auto expectedAsyncPatchListHostSrc = mutableSemWait->getCommandView();

    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(expectedAsyncPatchListHostSrc);
    auto waitAddress = event->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress, UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    auto &asyncPatchContainer = mutableCommandList->getBase()->getAsyncPatchContainer();
    ASSERT_EQ(1u, asyncPatchContainer.size());
    EXPECT_EQ(expectedAsyncPatchListGpuDst, asyncPatchContainer[0].gpuDestinationAddress);
    EXPECT_EQ(expectedAsyncPatchListHostSrc, asyncPatchContainer[0].hostSourceAddress);

    size_t expectedPatchSize = NEO::EncodeDataMemory<FamilyType>::getCommandSizeForEncode(sizeof(MI_SEMAPHORE_WAIT));
    EXPECT_EQ(expectedPatchSize, mutableCommandList->getBase()->getAsyncPatchlistPatchSize());

    ze_command_queue_desc_t queueDesc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    WhiteBox<L0::CommandQueue> *commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                                                 device,
                                                                                 neoDevice->getDefaultEngine().commandStreamReceiver,
                                                                                 &queueDesc,
                                                                                 false,
                                                                                 false,
                                                                                 false,
                                                                                 result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto mutableCommandListHandle = mutableCommandList->toHandle();

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();
    L0::CommandListExecutionInternalOptions internalOptions = {};
    result = commandQueue->executeCommandLists(1, &mutableCommandListHandle, nullptr, internalOptions);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), usedSpaceBefore), usedSpaceAfter - usedSpaceBefore));

    auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(0u, sdiCmds.size());

    size_t patchedSize = 0;
    for (auto &sdiCmds : sdiCmds) {
        auto sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(*sdiCmds);
        if (sdiCmd->getAddress() == expectedAsyncPatchListGpuDst) {
            if (sdiCmd->getStoreQword()) {
                expectedAsyncPatchListGpuDst += sizeof(uint64_t);
                patchedSize += sizeof(uint64_t);
                *semWaitPtr = sdiCmd->getDataDword0();
                semWaitPtr++;
                *semWaitPtr = sdiCmd->getDataDword1();
                semWaitPtr++;
            } else {
                expectedAsyncPatchListGpuDst += sizeof(uint32_t);
                patchedSize += sizeof(uint32_t);
                *semWaitPtr = sdiCmd->getDataDword0();
                semWaitPtr++;
            }
            if (patchedSize >= semWaitSize) {
                break;
            }
        }
    }
    semWaitPtr = reinterpret_cast<uint32_t *>(semWaitData);
    semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitPtr);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(waitAddress, UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    EXPECT_EQ(0u, mutableCommandList->getBase()->getAsyncPatchlistPatchSize());

    commandQueue->destroy();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenKernelWithTwoWaitRegularEventWhenNoopFirstAndMutateSecondThenDataIsUpdatedAndCommandsAreNoopedAndUpdated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t semWaitNoop[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(false, false, true, false, false);
    auto eventHandle = event->toHandle();
    auto event2 = createTestEvent(false, false, true, false, false);
    auto eventHandle2 = event2->toHandle();
    auto newEvent = createTestEvent(false, false, true, false, false);
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
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    auto mutableSemWait2 = waitEvent2Var->getSemWaitList()[0];
    auto mockMutableSemWait2 = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait2);
    auto semWait2Cmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait2->semWait);

    auto waitAddress2 = event2->getCompletionFieldGpuAddress(this->device);
    EXPECT_EQ(waitAddress2, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWait2Cmd));

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
    EXPECT_EQ(waitAddress2, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWait2Cmd));

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
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    // semaphore wait command for mutated event remains the same
    EXPECT_EQ(waitAddress2, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWait2Cmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListAndNoWaitEventFlagSelectedWhenAppendingKernelAndMutatingWaitEventThenErrorIsReturned) {
    auto originalEvent = createTestEvent(false, false, false, false, false);
    auto originalHandle = originalEvent->toHandle();
    auto mutatedEvent = createTestEvent(false, false, false, false, false);
    auto mutatedHandle = mutatedEvent->toHandle();

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &originalHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &mutatedHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenNotSupportedKernelLaunchModeWhenMutationPointActiveThenErrorCodeReturned) {
    mutableCommandIdDesc.flags = 0;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    this->testLaunchParams.isBuiltInKernel = true;
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    this->testLaunchParams.isBuiltInKernel = false;
    this->testLaunchParams.isIndirect = true;
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenNotSupportedKernelLaunchModeWhenMutationPointNotActiveThenSuccessCodeReturned) {
    this->testLaunchParams.isBuiltInKernel = true;
    auto result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    this->testLaunchParams.isBuiltInKernel = false;
    this->testLaunchParams.isIndirect = true;
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenNotSupportedKernelFlagsWhenAppendingKernelThenErrorCodeReturned) {
    mutableCommandIdDesc.flags = 0;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs = true;

    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE, result);

    // immutable kernels (without get next command id) are allowed to be appended, as mcl does not support implicit args, so the kernel will not be mutated
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using MutableCommandListInOrderTest = Test<MutableCommandListFixture<true, -1>>;
using MutableCommandListInOrderSem64Test = Test<MutableCommandListFixture<true, 1>>;
using MutableCommandListInOrderNoSem64Test = Test<MutableCommandListFixture<true, 0>>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenCounterBasedEventWhenAppendLaunchKernelThenGetDeviceCounterAllocIsAddedForResidency) {
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    createTestEvent(true, false, false, false, false);
    ze_event_handle_t hEvent = this->eventHandles[0];

    MockGraphicsAllocation counterDeviceAlloc(this->device->getRootDeviceIndex(), nullptr, 0x1);

    this->events[0]->getInOrderExecEventHelper().assignData(1, 0, 1, 1, &counterDeviceAlloc, &counterDeviceAlloc, 1, 0, nullptr, 0, 0, false, true);

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

    createTestEvent(true, false, false, false, false);
    ze_event_handle_t hEvent = this->eventHandles[0];

    uint32_t peerDeviceIndex = this->device->getRootDeviceIndex() + 1;
    MockGraphicsAllocation peerCounterDeviceAlloc(peerDeviceIndex, reinterpret_cast<void *>(0x1234), 0x0u);

    this->events[0]->getInOrderExecEventHelper().assignData(1, 0, 1, 1, &peerCounterDeviceAlloc, &peerCounterDeviceAlloc, 1, 0, nullptr, 0, 0, false, true);

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

    createTestEvent(true, false, false, false, false);
    auto newEvent = createTestEvent(true, false, false, false, false);

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

    EXPECT_EQ(mockBaseCmdList->inOrderExecInfo->getBaseDeviceAddress(), newEvent->getInOrderExecEventHelper().getBaseDeviceAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListWhenAppendingKernelWithSignalEventCbTimestampAndMutateItThenExpectPatchNewEventAddress) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(true, false, true, false, false);
    auto newEvent = createTestEvent(true, false, true, false, false);

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

    EXPECT_EQ(mockBaseCmdListHw->inOrderExecInfo->getBaseDeviceAddress(), newEvent->getInOrderExecEventHelper().getBaseDeviceAddress());

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

    auto event = createTestEvent(true, true, true, false, false);
    auto newEvent = createTestEvent(true, true, true, false, false);

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

    EXPECT_EQ(mockBaseCmdListHw->inOrderExecInfo->getBaseDeviceAddress(), newEvent->getInOrderExecEventHelper().getBaseDeviceAddress());

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

    auto event = createTestEvent(true, true, false, false, false);
    auto newEvent = createTestEvent(true, true, false, false, false);

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

    auto event = createTestEvent(false, false, false, false, false);
    auto newEvent = createTestEvent(false, false, false, false, false);

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
        EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
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
        EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
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

    auto event = createTestEvent(false, false, true, false, false);
    auto newEvent = createTestEvent(false, false, true, false, false);

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
        EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
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
        EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListOnDcFlushPlatformWhenAppendingKernelWithSignalEventRegularSignalScopeAndMutateItThenExpectPatchNewEventAddress) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto &productHelper = this->neoDevice->getProductHelper();
    if (productHelper.isL3FlushAfterPostSyncSupported()) {
        GTEST_SKIP();
    }

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);
    mockBaseCmdListHw->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(false, true, false, false, false);
    auto newEvent = createTestEvent(false, true, false, false, false);

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
    EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));

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
    EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
    postSyncAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);
    EXPECT_EQ(baseGpuVa, postSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenInOrderMutableCmdListOnDcFlushPlatformWhenAppendingKernelWithSignalEventRegularSignalScopeTimestampAndMutateItThenExpectPatchNewEventAddress) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto &productHelper = this->neoDevice->getProductHelper();
    if (productHelper.isL3FlushAfterPostSyncSupported()) {
        GTEST_SKIP();
    }

    auto mockBaseCmdListHw = static_cast<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(this->mutableCommandList.get()->base);
    mockBaseCmdListHw->dcFlushSupport = true;

    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    auto event = createTestEvent(false, true, true, false, false);
    auto newEvent = createTestEvent(false, true, true, false, false);

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
    EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));

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
    EXPECT_EQ(completionGpuVa, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semCmd));
    EXPECT_EQ(completionGpuVa, srmComplCmd->getMemoryAddress());
    EXPECT_EQ(baseGpuVa, srmBaseCmd->getMemoryAddress());
}

template <typename FamilyType>
void MutableCommandListFixtureInit::waitCbEventBelongToCurrentMutateToDifferent() {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto signalEvent = this->createTestEvent(false, false, false, false, false);
    auto signalEventHandle = signalEvent->toHandle();
    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = this->createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = this->createMutableCmdList();
    // attach event 2 to the external command list
    auto result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // attach event 1 to the command list
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT | ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event and signal event 0
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, signalEventHandle, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    const size_t expectedLriSize = this->lriRequired ? 2 : 0;

    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;
    if (expectedLriSize > 0) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = this->mutableCommandList->updateMutableCommandWaitEventsExp(this->commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitAddress = newEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (expectedLriSize > 0) {
        constexpr uint32_t firstRegister = 0x2600;
        constexpr uint32_t secondRegister = 0x2604;

        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenSemaphore64OffAndKernelWithWaitCbEventBelongingToCmdListWhenMutateIntoEventFromDifferentCmdListThenDataIsUpdatedAndCommandResotred) {
    waitCbEventBelongToCurrentMutateToDifferent<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderSem64Test,
            givenSemaphore64OnAndKernelWithWaitCbEventBelongingToCmdListWhenMutateIntoEventFromDifferentCmdListThenDataIsUpdatedAndCommandResotred) {
    waitCbEventBelongToCurrentMutateToDifferent<FamilyType>();
}

template <typename FamilyType>
void MutableCommandListFixtureInit::waitCbEventBelongToCurrentMutateToCurrent() {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = this->createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    // attach events 1 & 2 to the command list
    auto result = this->mutableCommandList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = this->mutableCommandList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    const size_t expectedLriSize = this->lriRequired ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;
    if (expectedLriSize > 0) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = this->mutableCommandList->updateMutableCommandWaitEventsExp(this->commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));
    if (expectedLriSize > 0) {
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenSemaphore64OffAndKernelWithWaitCbEventBelongingToCmdListWhenMutateIntoEventFromSameCmdListThenDataIsUpdatedAndNoopRemain) {
    waitCbEventBelongToCurrentMutateToCurrent<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderSem64Test,
            givenSemaphore64OnAndKernelWithWaitCbEventBelongingToCmdListWhenMutateIntoEventFromSameCmdListThenDataIsUpdatedAndNoopRemain) {
    waitCbEventBelongToCurrentMutateToCurrent<FamilyType>();
}

template <typename FamilyType>
void MutableCommandListFixtureInit::waitCbEventBelongToDifferentMutateToCurrent() {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = this->createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = this->createMutableCmdList();
    // attach event 1 to the external command list
    auto result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // attach event 2 to the command list
    result = this->mutableCommandList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    const size_t expectedLriSize = this->lriRequired ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;

    if (expectedLriSize > 0) {
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

    result = this->mutableCommandList->updateMutableCommandWaitEventsExp(this->commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));

    if (expectedLriSize > 0) {
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenSemaphore64OffAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenMutateIntoEventOwnedToCurrentCmdListThenDataIsUpdatedAndCommandNooped) {
    waitCbEventBelongToDifferentMutateToCurrent<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderSem64Test,
            givenSemaphore64OnAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenMutateIntoEventOwnedToCurrentCmdListThenDataIsUpdatedAndCommandNooped) {
    waitCbEventBelongToDifferentMutateToCurrent<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitCbTimestampEventBelongingToDifferentCmdListWhenMutateIntoDifferentEventThenDataIsUpdated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = createTestEvent(true, false, true, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, true, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = createMutableCmdList();
    // attach event 1 to the external command list
    auto result = externalCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    // attach event 2 to the external command list
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

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    uint64_t waitAddress = 0;
    if (mutableCommandList->getBase()->isHeaplessModeEnabled()) {
        waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    } else {
        waitAddress = event->getCompletionFieldGpuAddress(this->device);
    }
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    if (mutableCommandList->getBase()->isHeaplessModeEnabled()) {
        waitAddress = newEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    } else {
        waitAddress = newEvent->getCompletionFieldGpuAddress(this->device);
    }
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

template <typename FamilyType>
void MutableCommandListFixtureInit::waitCbEventBelongToDifferentNoopMutateBack() {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t lriNoopSpace[sizeof(MI_LOAD_REGISTER_IMM)] = {0};
    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = this->createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopHandle = nullptr;

    auto externalCmdList = this->createMutableCmdList();
    // attach event 1 to the external command list
    auto result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    const size_t expectedLriSize = this->lriRequired ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;

    constexpr uint32_t firstRegister = 0x2600;
    constexpr uint32_t secondRegister = 0x2604;

    if (expectedLriSize > 0) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }

    result = this->mutableCommandList->updateMutableCommandWaitEventsExp(this->commandId, 1, &noopHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0, memcmp(semWaitCmd, semWaitNoopSpace, sizeof(MI_SEMAPHORE_WAIT)));
    if (expectedLriSize > 0) {
        EXPECT_EQ(0, memcmp(lriCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(lriUpperCmd, lriNoopSpace, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = this->mutableCommandList->updateMutableCommandWaitEventsExp(this->commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
    if (expectedLriSize > 0) {
        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }

    result = this->mutableCommandList->updateMutableCommandWaitEventsExp(this->commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (expectedLriSize > 0) {
        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenSemaphore64OffAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenNoopedAndMutatedBackThenDataIsUpdatedAndCommandNoopedAndRestored) {
    waitCbEventBelongToDifferentNoopMutateBack<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderSem64Test,
            givenSemaphore64OnAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenNoopedAndMutatedBackThenDataIsUpdatedAndCommandNoopedAndRestored) {
    waitCbEventBelongToDifferentNoopMutateBack<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitCbEventExternalBelongingToDifferentCmdListWhenAssigningCbEventToThirdCmdListAndMutateWaitEventThenPerformMutationCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    alignas(uint32_t) uint8_t noopSemWait[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    constexpr bool isExternalFlag = true;
    auto event = createTestEvent(true, false, false, false, isExternalFlag);
    auto eventHandle = event->toHandle();

    auto externalCmdList = createMutableCmdList();
    auto thirdCmdList = createMutableCmdList();
    // attach event 1 to the external command list
    auto result = externalCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(2u, waitEventVar->getSemWaitList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    EXPECT_EQ(0, memcmp(mockMutableSemWait->semWait, noopSemWait, sizeof(MI_SEMAPHORE_WAIT)));

    mutableSemWait = waitEventVar->getSemWaitList()[1];
    mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    // use cb event from third command list as wait event - will be attached to other in order exec info
    result = thirdCmdList->appendLaunchKernel(kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = thirdCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutate same event handle - should update the wait event to point to the new in order exec info
    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();

    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenKernelWithWaitRegularEventWhenWhenNoopedAndMutatedIntoDifferentEventThenDataIsUpdatedAndCommandNoopedAndRestoredIntoDifferentCompletion) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    alignas(uint32_t) uint8_t semWaitNoopSpace[sizeof(MI_SEMAPHORE_WAIT)] = {0};

    auto event = createTestEvent(false, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(false, false, false, false, false);
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
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

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
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
}

template <typename FamilyType>
void MutableCommandListFixtureInit::waitCbEventBelongToDifferentMutateToDifferent() {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = this->createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = this->createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto externalCmdList = this->createMutableCmdList();
    // attach both events to the external command list
    auto result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, eventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->appendLaunchKernel(this->kernel2->toHandle(), this->testGroupCount, newEventHandle, 0, nullptr, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = externalCmdList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // mutation point
    this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = this->mutableCommandList->appendLaunchKernel(this->kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    const size_t expectedLriSize = this->lriRequired ? 2 : 0;
    ASSERT_EQ(expectedLriSize, waitEventVar->getLoadRegImmList().size());

    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    auto waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    auto waitValue = static_cast<uint32_t>(event->getInOrderExecEventHelper().getEventData()->counterValue);

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriUpperCmd = nullptr;

    if (expectedLriSize > 0) {
        constexpr uint32_t firstRegister = 0x2600;
        constexpr uint32_t secondRegister = 0x2604;

        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(getLowPart(waitValue), lriCmd->getDataDword());

        mutableLri = waitEventVar->getLoadRegImmList()[1];
        mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriUpperCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);

        EXPECT_EQ(firstRegister, lriCmd->getRegisterOffset());
        EXPECT_EQ(secondRegister, lriUpperCmd->getRegisterOffset());
    } else {
        EXPECT_EQ(waitValue, static_cast<uint32_t>(NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd)));
    }

    // mutate to event 2
    result = this->mutableCommandList->updateMutableCommandWaitEventsExp(this->commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    waitAddress = newEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    waitValue = static_cast<uint32_t>(newEvent->getInOrderExecEventHelper().getEventData()->counterValue);

    if (expectedLriSize > 0) {
        EXPECT_EQ(getLowPart(waitValue), lriCmd->getDataDword());
    } else {
        EXPECT_EQ(waitValue, static_cast<uint32_t>(NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd)));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderNoSem64Test,
            givenSemaphore64OffAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenMutatedIntoCbEventBelongingToDifferentThenUpdateAddressAndValue) {
    waitCbEventBelongToDifferentMutateToDifferent<FamilyType>();
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderSem64Test,
            givenSemaphore64OnAndKernelWithWaitCbEventBelongingToDifferentCmdListWhenMutatedIntoCbEventBelongingToDifferentThenUpdateAddressAndValue) {
    waitCbEventBelongToDifferentMutateToDifferent<FamilyType>();
}

template <typename FamilyType>
void MutableCommandListFixtureInit::mutableWaitEventsOnAppendOperations(
    MutableEventOnAppendOperationCallback callbackInit,
    bool doNotSelectWaitEvents,
    bool createCbEvent,
    bool doNotGetNextCommandId) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = this->createTestEvent(createCbEvent, false, false, false, false);
    auto eventHandle = event->toHandle();

    MutableWaitEventsOnAppendOperationsData callbackData = {};

    if (createCbEvent) {
        callbackData.signalEvent = eventHandle;
    } else {
        callbackData.waitEvents = &eventHandle;
        callbackData.numWaitEvents = 1;
    }

    if (doNotSelectWaitEvents) {
        this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;
    } else {
        this->mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    if (doNotGetNextCommandId == false) {
        result = this->mutableCommandList->getNextCommandId(&this->mutableCommandIdDesc, 0, nullptr, &this->commandId);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }

    // create resources and call the append
    (this->*callbackInit)(&callbackData);
    if (createCbEvent) {
        if (callbackData.cbEventAsWaitEvent) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, callbackData.result);
        } else {
            EXPECT_NE(ZE_RESULT_SUCCESS, callbackData.result);
        }
    } else {
        EXPECT_EQ(ZE_RESULT_SUCCESS, callbackData.result);

        if (doNotSelectWaitEvents || doNotGetNextCommandId) {
            EXPECT_EQ(nullptr, callbackData.outWaitCmds);
        } else {
            EXPECT_NE(nullptr, callbackData.outWaitCmds);
            EXPECT_TRUE(callbackData.skipAddingWaitEventsToResidency);
        }
    }

    result = this->mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    if (doNotGetNextCommandId) {
        EXPECT_EQ(0u, mutableCommandList->eventMutations.size());
    } else {
        auto waitEvents = this->getVariableList(this->commandId, L0::MCL::VariableType::waitEvent, nullptr);
        if (doNotSelectWaitEvents || createCbEvent) {
            EXPECT_EQ(0u, waitEvents.size());
        } else {
            ASSERT_EQ(1u, waitEvents.size());
            auto waitEventVar = waitEvents[0];
            auto mutableSemWait = waitEventVar->getSemWaitList()[0];
            auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
            auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
            auto waitAddress = event->getGpuAddress(this->device);

            EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
        }
    }

    if (callbackData.srcImageHandle) {
        auto srcImage = L0::Image::fromHandle(callbackData.srcImageHandle);
        srcImage->destroy();
    }
    if (callbackData.dstImageHandle) {
        auto dstImage = L0::Image::fromHandle(callbackData.dstImageHandle);
        dstImage->destroy();
    }

    result = this->mutableCommandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendBarrierThenExpectCreateWaitEventVariable) {

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendBarrierCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendBarrierCallback, true, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendBarrierCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendBarrierCallback, false, false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryRangesBarrierThenExpectCreateWaitEventVariable) {

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendRangesBarrierCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendRangesBarrierCallback, true, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendRangesBarrierCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendRangesBarrierCallback, false, false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryCopyThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::copyBufferToBufferMiddle, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryCopyRegionThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::copyBufferToBufferMiddle, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyRegionCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyRegionCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyRegionCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryCopyWithParametersThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::copyBufferToBufferMiddle, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyWithParametersCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyWithParametersCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyWithParametersCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryCopyFromContextThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::copyBufferToBufferMiddle, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyFromContextCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyFromContextCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryCopyFromContextCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryFillThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::fillBufferImmediate, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryFillWithParametersThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::fillBufferImmediate, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillWithParametersCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillWithParametersCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendMemoryFillWithParametersCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyFromMemoryThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyFromMemoryExtThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryExtCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryExtCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyFromMemoryExtCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyToMemoryThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyToMemoryExtThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryExtCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryExtCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyToMemoryExtCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendMemoryImageCopyRegionThenExpectCreateWaitEventVariable) {
    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltIn::copyImageRegion, getDefaultBuiltInMode());
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
        auto isa = kernelImmutableData->getIsaGraphicsAllocation();
        isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyRegionCallback,
                                                        false, false, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyRegionCallback,
                                                        false, true, false);

        mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendImageCopyRegionCallback,
                                                        false, false, true);

        isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendWaitOnEventsThenExpectCreateWaitEventVariable) {
    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWaitOnEventsCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWaitOnEventsCallback, true, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWaitOnEventsCallback, true, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWaitOnEventsCallback, false, false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendWriteGlobalTimestampThenExpectCreateWaitEventVariable) {
    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWriteGlobalTimestampCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWriteGlobalTimestampCallback, true, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWriteGlobalTimestampCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendWriteGlobalTimestampCallback, false, false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendQueryKernelTimestampsThenExpectCreateWaitEventVariable) {
    auto kernel = device->getBuiltinFunctionsLib()->getFunction(BufferBuiltIn::queryKernelTimestamps, getDefaultBuiltInMode());
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);

    auto kernelImmutableData = mockBuiltinKernel->getImmutableData();
    auto isa = kernelImmutableData->getIsaGraphicsAllocation();
    isa->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(0x100000), isa->getGpuAddress());

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendQueryKernelTimestampsCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendQueryKernelTimestampsCallback, false, false, true);

    isa->setCpuPtrAndGpuAddress(nullptr, isa->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListTest,
            givenMutableCommandListSelectedWaitEventsWhenCallAppendHostFunctionThenExpectCreateWaitEventVariable) {
    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendHostFunctionCallback, false, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendHostFunctionCallback, true, false, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendHostFunctionCallback, false, true, false);

    mutableWaitEventsOnAppendOperations<FamilyType>(&MutableCommandListFixtureInit::mutableWaitEventsOnAppendHostFunctionCallback, false, false, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenExternalCbEventWithPatchPreambleWhenAppendingKernelWithEventAndMutatingThenNewPatchPreambleWaitIsSet) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MI_SEMAPHORE_WAIT templateSemWait;

    alignas(uint32_t) uint8_t noopSemWait[sizeof(MI_SEMAPHORE_WAIT)] = {0};
    alignas(uint32_t) uint8_t noopLri[sizeof(MI_LOAD_REGISTER_IMM)] = {0};

    auto event = createTestEvent(true, false, false, false, true);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false, true);
    auto newEventHandle = newEvent->toHandle();
    ze_event_handle_t noopHandle = nullptr;

    auto otherCmdlist = createMutableCmdList();
    // attach wait events to other command list
    L0::CmdListWaitEventParameters waitEventParams = {};
    otherCmdlist->appendBarrier(eventHandle, 0, nullptr, waitEventParams);
    otherCmdlist->appendBarrier(newEventHandle, 0, nullptr, waitEventParams);
    otherCmdlist->close();

    // assign them counters
    uint64_t counter = 0x123;
    auto counterLow = getLowPart(counter);
    uint64_t deviceGpuAddress = 0xAB000;
    MockGraphicsAllocation counterAllocation(nullptr, deviceGpuAddress, sizeof(uint64_t));
    event->getInOrderExecEventHelper().assignPatchPreambleData(counter, nullptr, 0, nullptr, deviceGpuAddress, &counterAllocation);

    constexpr bool registerPollMode = false;
    constexpr bool waitMode = true;
    constexpr bool switchOnUnsuccessful = false;

    auto waitValue = this->lriRequired ? 0 : counter;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&templateSemWait,
                                                             deviceGpuAddress,
                                                             waitValue,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                             registerPollMode, waitMode, this->qwordInUse, this->lriRequired, switchOnUnsuccessful, this->sem64bSupport);

    uint64_t newCounter = 0x456;
    auto newCounterLow = getLowPart(newCounter);
    uint64_t newDeviceGpuAddress = 0xCD000;
    MockGraphicsAllocation newCounterAllocation(nullptr, newDeviceGpuAddress, sizeof(uint64_t));
    newEvent->getInOrderExecEventHelper().assignPatchPreambleData(newCounter, nullptr, 0, nullptr, newDeviceGpuAddress, &newCounterAllocation);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(isAllocationInMutableResidency(mutableCommandList.get(), &counterAllocation));

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(2u, waitEventVar->getSemWaitList().size());

    // patch preamble mutable sem wait
    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(deviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
    EXPECT_EQ(0, memcmp(&templateSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    if (this->lriRequired) {
        ASSERT_EQ(4u, waitEventVar->getLoadRegImmList().size());
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(counterLow, lriCmd->getDataDword());
    } else {
        EXPECT_EQ(counter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &noopHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(isAllocationInMutableResidency(mutableCommandList.get(), &counterAllocation));

    EXPECT_EQ(0, memcmp(semWaitCmd, noopSemWait, sizeof(MI_SEMAPHORE_WAIT)));
    if (this->lriRequired) {
        EXPECT_EQ(0, memcmp(lriCmd, noopLri, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(isAllocationInMutableResidency(mutableCommandList.get(), &newCounterAllocation));

    semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmd);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(newDeviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (this->lriRequired) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriCmd);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(newCounterLow, lriCmd->getDataDword());
    } else {
        EXPECT_EQ(newCounter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
    }

    waitValue = this->lriRequired ? 0 : newCounter;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&templateSemWait,
                                                             newDeviceGpuAddress,
                                                             waitValue,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                             registerPollMode, waitMode, this->qwordInUse, this->lriRequired, switchOnUnsuccessful, this->sem64bSupport);
    EXPECT_EQ(0, memcmp(&templateSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenExternalCbEventWithPatchPreambleCounterExceeding32BitBoundryWhenAppendingKernelWithWaitEventAndMutatingThenNewPatchPreambleWaitIsSetAndDeviceCounterUsesCorrectValues) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MI_SEMAPHORE_WAIT templateSemWait;

    auto event = createTestEvent(true, false, false, false, true);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false, true);
    auto newEventHandle = newEvent->toHandle();

    auto otherCmdlist = createMutableCmdList();
    // attach wait events to other command list
    L0::CmdListWaitEventParameters waitEventParams = {};
    otherCmdlist->appendBarrier(eventHandle, 0, nullptr, waitEventParams);
    otherCmdlist->appendBarrier(newEventHandle, 0, nullptr, waitEventParams);
    otherCmdlist->close();

    // assign them counters
    uint32_t counterLow = 2;
    uint64_t counter = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1 + counterLow;
    auto lriCounterLow = getLowPart(counter);
    auto lriCounterHigh = getHighPart(counter);
    uint64_t deviceGpuAddress = 0xAB000;
    MockGraphicsAllocation counterAllocation(nullptr, deviceGpuAddress, sizeof(uint64_t));
    event->getInOrderExecEventHelper().assignPatchPreambleData(counter, nullptr, 0, nullptr, deviceGpuAddress, &counterAllocation);

    constexpr bool registerPollMode = false;
    constexpr bool waitMode = true;
    constexpr bool switchOnUnsuccessful = false;

    // when qword not in use, then use lower 32b, when in use, check if lri is required, if so, use 0, else use full 64b counter
    auto waitValue = this->qwordInUse == false ? counterLow : this->lriRequired ? 0
                                                                                : counter;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&templateSemWait,
                                                             deviceGpuAddress,
                                                             waitValue,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                             registerPollMode, waitMode, this->qwordInUse, this->lriRequired, switchOnUnsuccessful, this->sem64bSupport);

    uint32_t newCounterLow = 4;
    uint64_t newCounter = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1 + newCounterLow;
    auto newLriCounterLow = getLowPart(newCounter);
    auto newLriCounterHigh = getHighPart(newCounter);
    uint64_t newDeviceGpuAddress = 0xCD000;
    MockGraphicsAllocation newCounterAllocation(nullptr, newDeviceGpuAddress, sizeof(uint64_t));
    newEvent->getInOrderExecEventHelper().assignPatchPreambleData(newCounter, nullptr, 0, nullptr, newDeviceGpuAddress, &newCounterAllocation);

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(isAllocationInMutableResidency(mutableCommandList.get(), &counterAllocation));

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];
    ASSERT_EQ(2u, waitEventVar->getSemWaitList().size());

    // patch preamble mutable sem wait
    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    auto semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(deviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));
    EXPECT_EQ(0, memcmp(&templateSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    if (this->lriRequired) {
        ASSERT_EQ(4u, waitEventVar->getLoadRegImmList().size());
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(lriCounterLow, lriCmd->getDataDword());

        auto mutableLriHigh = waitEventVar->getLoadRegImmList()[1];
        auto mockMutableLriHigh = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLriHigh);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLriHigh->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(lriCounterHigh, lriCmd->getDataDword());

        EXPECT_EQ(0u, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
    } else {
        if (this->qwordInUse) {
            EXPECT_EQ(counter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
        } else {
            EXPECT_EQ(counterLow, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
        }
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(isAllocationInMutableResidency(mutableCommandList.get(), &newCounterAllocation));

    semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmd);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(newDeviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (this->lriRequired) {
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(newLriCounterLow, lriCmd->getDataDword());

        auto mutableLriHigh = waitEventVar->getLoadRegImmList()[1];
        auto mockMutableLriHigh = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLriHigh);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLriHigh->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);
        EXPECT_EQ(newLriCounterHigh, lriCmd->getDataDword());

        EXPECT_EQ(0u, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
    } else {
        if (this->qwordInUse) {
            EXPECT_EQ(newCounter, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
        } else {
            EXPECT_EQ(newCounterLow, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semWaitCmd));
        }
    }

    waitValue = this->qwordInUse == false ? newCounterLow : this->lriRequired ? 0
                                                                              : newCounter;
    NEO::EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&templateSemWait,
                                                             newDeviceGpuAddress,
                                                             waitValue,
                                                             COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                             registerPollMode, waitMode, this->qwordInUse, this->lriRequired, switchOnUnsuccessful, this->sem64bSupport);
    EXPECT_EQ(0, memcmp(&templateSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenGraphInstantiationTargetWhenSwitchingCounterBasedEventThenRestrictionIsTracked) {
    auto *event = createTestEvent(true, true, false, false, false);
    ASSERT_TRUE(event->isCounterBasedExplicitlyEnabled());
    ASSERT_FALSE(event->isExternalEvent());
    ASSERT_FALSE(event->getIsSignalledAsGraphInternalEvent());

    mutableCommandList->getBase()->setIsGraphInstantiationTarget(true);
    mutableCommandList->switchCounterBasedEvents(0, 0, event);
    EXPECT_TRUE(event->getIsSignalledAsGraphInternalEvent());

    mutableCommandList->getBase()->setIsGraphInstantiationTarget(false);
    mutableCommandList->switchCounterBasedEvents(0, 0, event);
    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenGraphInstantiationTargetWhenSwitchingExternalCounterBasedEventThenRestrictionIsNotApplied) {
    auto *event = createTestEvent(true, true, false, false, true);
    ASSERT_TRUE(event->isExternalEvent());

    mutableCommandList->getBase()->setIsGraphInstantiationTarget(true);
    mutableCommandList->switchCounterBasedEvents(0, 0, event);
    EXPECT_FALSE(event->getIsSignalledAsGraphInternalEvent());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenUnassignedCbEventWhenAppendingKernelWithUnassignedEventAndMutatingIntoAssignedThenCommandIsNoopedAndAfterMutationRestored) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MI_SEMAPHORE_WAIT noopSemWait;
    memset(&noopSemWait, 0, sizeof(MI_SEMAPHORE_WAIT));

    MI_LOAD_REGISTER_IMM noopLri;
    memset(&noopLri, 0, sizeof(MI_LOAD_REGISTER_IMM));

    auto event = createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto otherCmdlist = createMutableCmdList();
    // attach new wait event to other command list
    L0::CmdListWaitEventParameters waitEventParams = {};
    otherCmdlist->appendBarrier(newEventHandle, 0, nullptr, waitEventParams);
    otherCmdlist->close();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];

    EXPECT_TRUE(waitEventVar->getDesc().eventValue.noopState);

    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    // sem wait is nooped
    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    MI_SEMAPHORE_WAIT *semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    EXPECT_EQ(0, memcmp(&noopSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriHighCmd = nullptr;
    if (this->lriRequired) {
        ASSERT_EQ(2u, waitEventVar->getLoadRegImmList().size());
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        EXPECT_EQ(0, memcmp(&noopLri, lriCmd, sizeof(MI_LOAD_REGISTER_IMM)));

        auto mutableLriHigh = waitEventVar->getLoadRegImmList()[1];
        auto mockMutableLriHigh = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLriHigh);
        lriHighCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLriHigh->loadRegImm);
        EXPECT_EQ(0, memcmp(&noopLri, lriHighCmd, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(waitEventVar->getDesc().eventValue.noopState);

    semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmd);
    ASSERT_NE(nullptr, semWaitCmd);

    auto waitAddress = newEvent->getInOrderExecEventHelper().getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (this->lriRequired) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriCmd);
        ASSERT_NE(nullptr, lriCmd);

        lriHighCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriHighCmd);
        ASSERT_NE(nullptr, lriHighCmd);
    }

    // mutate back into noop state
    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(waitEventVar->getDesc().eventValue.noopState);

    EXPECT_EQ(0, memcmp(&noopSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));
    if (this->lriRequired) {
        EXPECT_EQ(0, memcmp(&noopLri, lriCmd, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(&noopLri, lriHighCmd, sizeof(MI_LOAD_REGISTER_IMM)));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenAssignedCbEventWhenAppendingKernelWithAssignedEventAndMutatingIntoUnassignedThenCommandIsProgramedAndAfterMutationNooped) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MI_SEMAPHORE_WAIT noopSemWait;
    memset(&noopSemWait, 0, sizeof(MI_SEMAPHORE_WAIT));

    MI_LOAD_REGISTER_IMM noopLri;
    memset(&noopLri, 0, sizeof(MI_LOAD_REGISTER_IMM));

    auto event = createTestEvent(true, false, false, false, false);
    auto eventHandle = event->toHandle();
    auto newEvent = createTestEvent(true, false, false, false, false);
    auto newEventHandle = newEvent->toHandle();

    auto otherCmdlist = createMutableCmdList();
    // attach appending wait event to other command list
    L0::CmdListWaitEventParameters waitEventParams = {};
    otherCmdlist->appendBarrier(eventHandle, 0, nullptr, waitEventParams);
    otherCmdlist->close();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];

    EXPECT_FALSE(waitEventVar->getDesc().eventValue.noopState);

    ASSERT_EQ(1u, waitEventVar->getSemWaitList().size());
    // sem wait is nooped
    auto mutableSemWait = waitEventVar->getSemWaitList()[0];
    auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
    MI_SEMAPHORE_WAIT *semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
    ASSERT_NE(nullptr, semWaitCmd);

    auto waitAddress = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    MI_LOAD_REGISTER_IMM *lriHighCmd = nullptr;
    if (this->lriRequired) {
        ASSERT_EQ(2u, waitEventVar->getLoadRegImmList().size());
        auto mutableLri = waitEventVar->getLoadRegImmList()[0];
        auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
        ASSERT_NE(nullptr, lriCmd);

        auto mutableLriHigh = waitEventVar->getLoadRegImmList()[1];
        auto mockMutableLriHigh = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLriHigh);
        lriHighCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(mockMutableLriHigh->loadRegImm);
        ASSERT_NE(nullptr, lriHighCmd);
    }

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &newEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(waitEventVar->getDesc().eventValue.noopState);

    EXPECT_EQ(0, memcmp(&noopSemWait, semWaitCmd, sizeof(MI_SEMAPHORE_WAIT)));
    if (this->lriRequired) {
        EXPECT_EQ(0, memcmp(&noopLri, lriCmd, sizeof(MI_LOAD_REGISTER_IMM)));
        EXPECT_EQ(0, memcmp(&noopLri, lriHighCmd, sizeof(MI_LOAD_REGISTER_IMM)));
    }

    // mutate back into noop state
    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(waitEventVar->getDesc().eventValue.noopState);

    semWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmd);
    ASSERT_NE(nullptr, semWaitCmd);
    EXPECT_EQ(waitAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmd));

    if (this->lriRequired) {
        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriCmd);
        ASSERT_NE(nullptr, lriCmd);
        lriHighCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriHighCmd);
        ASSERT_NE(nullptr, lriHighCmd);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableCommandListInOrderTest,
            givenUnassignedExternalCbEventAndAppendingKernelWithUnassignedEventWhenAssigningExternalAndMutationRefreshThenCommandIsNoopedAndAfterMutationRestored) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    MI_SEMAPHORE_WAIT noopSemWait;
    memset(&noopSemWait, 0, sizeof(MI_SEMAPHORE_WAIT));

    MI_LOAD_REGISTER_IMM noopLri;
    memset(&noopLri, 0, sizeof(MI_LOAD_REGISTER_IMM));

    auto event = createTestEvent(true, false, false, false, true);
    auto eventHandle = event->toHandle();

    // mutation point
    mutableCommandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    auto result = mutableCommandList->getNextCommandId(&mutableCommandIdDesc, 0, nullptr, &commandId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // use event 1 as wait event
    result = mutableCommandList->appendLaunchKernel(kernel->toHandle(), this->testGroupCount, nullptr, 1, &eventHandle, this->testLaunchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto waitEvents = getVariableList(commandId, L0::MCL::VariableType::waitEvent, nullptr);
    ASSERT_EQ(1u, waitEvents.size());
    auto waitEventVar = waitEvents[0];

    EXPECT_TRUE(waitEventVar->getDesc().eventValue.noopState);
    EXPECT_TRUE(waitEventVar->getDesc().eventValue.patchPreambleNoopState);

    ASSERT_EQ(2u, waitEventVar->getSemWaitList().size());
    MI_SEMAPHORE_WAIT *semWaitCmdPatchPreamble = nullptr;
    MI_SEMAPHORE_WAIT *semWaitCmdEvent = nullptr;
    {
        auto mutableSemWait = waitEventVar->getSemWaitList()[0];
        auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
        semWaitCmdPatchPreamble = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
        EXPECT_EQ(0, memcmp(&noopSemWait, semWaitCmdPatchPreamble, sizeof(MI_SEMAPHORE_WAIT)));
    }
    {
        auto mutableSemWait = waitEventVar->getSemWaitList()[1];
        auto mockMutableSemWait = static_cast<MockMutableSemaphoreWaitHw<FamilyType> *>(mutableSemWait);
        semWaitCmdEvent = reinterpret_cast<MI_SEMAPHORE_WAIT *>(mockMutableSemWait->semWait);
        EXPECT_EQ(0, memcmp(&noopSemWait, semWaitCmdEvent, sizeof(MI_SEMAPHORE_WAIT)));
    }

    MI_LOAD_REGISTER_IMM *lriCmdPatchPreamble = nullptr;
    MI_LOAD_REGISTER_IMM *lriHighCmdPatchPreamble = nullptr;

    MI_LOAD_REGISTER_IMM *lriCmdEvent = nullptr;
    MI_LOAD_REGISTER_IMM *lriHighCmdEvent = nullptr;

    if (this->lriRequired) {
        ASSERT_EQ(4u, waitEventVar->getLoadRegImmList().size());
        {
            auto mutableLri = waitEventVar->getLoadRegImmList()[0];
            auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
            lriCmdPatchPreamble = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
            EXPECT_EQ(0, memcmp(&noopLri, lriCmdPatchPreamble, sizeof(MI_LOAD_REGISTER_IMM)));
        }
        {
            auto mutableLri = waitEventVar->getLoadRegImmList()[1];
            auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
            lriHighCmdPatchPreamble = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
            EXPECT_EQ(0, memcmp(&noopLri, lriHighCmdPatchPreamble, sizeof(MI_LOAD_REGISTER_IMM)));
        }
        {
            auto mutableLri = waitEventVar->getLoadRegImmList()[2];
            auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
            lriCmdEvent = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
            EXPECT_EQ(0, memcmp(&noopLri, lriCmdEvent, sizeof(MI_LOAD_REGISTER_IMM)));
        }
        {
            auto mutableLri = waitEventVar->getLoadRegImmList()[2];
            auto mockMutableLri = static_cast<MockMutableLoadRegisterImmHw<FamilyType> *>(mutableLri);
            lriHighCmdEvent = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(mockMutableLri->loadRegImm);
            EXPECT_EQ(0, memcmp(&noopLri, lriHighCmdEvent, sizeof(MI_LOAD_REGISTER_IMM)));
        }
    }

    auto otherCmdlist = createMutableCmdList();
    // attach new wait event to other command list
    L0::CmdListWaitEventParameters waitEventParams = {};
    otherCmdlist->appendBarrier(eventHandle, 0, nullptr, waitEventParams);
    otherCmdlist->close();

    uint64_t patchPreambleCounter = 4;
    uint64_t patchPreambleDeviceGpuAddress = 0xCD000;
    MockGraphicsAllocation patchPreambleDeviceAllocation(nullptr, patchPreambleDeviceGpuAddress, sizeof(uint64_t));
    event->getInOrderExecEventHelper().assignPatchPreambleData(patchPreambleCounter, nullptr, 0, nullptr, patchPreambleDeviceGpuAddress, &patchPreambleDeviceAllocation);

    result = mutableCommandList->updateMutableCommandWaitEventsExp(commandId, 1, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = mutableCommandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(waitEventVar->getDesc().eventValue.noopState);
    EXPECT_FALSE(waitEventVar->getDesc().eventValue.patchPreambleNoopState);

    semWaitCmdPatchPreamble = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmdPatchPreamble);
    EXPECT_EQ(patchPreambleDeviceGpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdPatchPreamble));

    semWaitCmdEvent = genCmdCast<MI_SEMAPHORE_WAIT *>(semWaitCmdEvent);
    ASSERT_NE(nullptr, semWaitCmdEvent);

    auto waitAddressEvent = event->getInOrderExecEventHelper().getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    EXPECT_EQ(waitAddressEvent, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semWaitCmdEvent));

    if (this->lriRequired) {
        lriCmdPatchPreamble = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriCmdPatchPreamble);
        ASSERT_NE(nullptr, lriCmdPatchPreamble);

        lriHighCmdPatchPreamble = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriHighCmdPatchPreamble);
        ASSERT_NE(nullptr, lriHighCmdPatchPreamble);

        lriCmdEvent = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriCmdEvent);
        ASSERT_NE(nullptr, lriCmdEvent);

        lriHighCmdEvent = genCmdCast<MI_LOAD_REGISTER_IMM *>(lriHighCmdEvent);
        ASSERT_NE(nullptr, lriHighCmdEvent);
    }
}

} // namespace ult
} // namespace L0

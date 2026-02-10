/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/black_box_tests/mcl_samples/zello_mcl_test_helper.h"

#include <cstring>
#include <memory>
#include <sstream>

struct TestValues {
    TestValues(bool full) : full(full) {}

    std::vector<bool> &getMutateFirst() {
        if (full) {
            return mutateFirstValuesFull;
        } else {
            return mutateFirstValuesLimited;
        }
    }

    std::vector<bool> &getMutateSecond() {
        if (full) {
            return mutateSecondValuesFull;
        } else {
            return mutateSecondValuesLimited;
        }
    }

    std::vector<bool> &getDifferentPoolFirstKernel() {
        if (full) {
            return useDifferentPoolFirstKernelValuesFull;
        } else {
            return useDifferentPoolFirstKernelValuesLimited;
        }
    }

    std::vector<bool> &getDifferentPoolSecondKernel() {
        if (full) {
            return useDifferentPoolSecondKernelValuesFull;
        } else {
            return useDifferentPoolSecondKernelValuesLimited;
        }
    }

    std::vector<uint32_t> &getWaitEventMask() {
        if (full) {
            return waitEventMaskValuesFull;
        } else {
            return waitEventMaskValuesLimited;
        }
    }

    std::vector<uint32_t> &getBaseIndexValues() {
        if (full) {
            return baseIndexValuesFull;
        } else {
            return baseIndexValuesLimited;
        }
    }

    std::vector<uint32_t> &getMutableIndexValues() {
        if (full) {
            return mutableIndexValuesFull;
        } else {
            return mutableIndexValuesLimited;
        }
    }

    std::vector<uint32_t> &getMutableGroupCountXValues() {
        if (full) {
            return groupCountXValuesFull;
        } else {
            return groupCountXValuesLimited;
        }
    }

    std::vector<uint32_t> &getMutableGroupCountYValues() {
        if (full) {
            return groupCountYValuesFull;
        } else {
            return groupCountYValuesLimited;
        }
    }

    std::vector<uint32_t> &getMutableGroupCountZValues() {
        if (full) {
            return groupCountZValuesFull;
        } else {
            return groupCountZValuesLimited;
        }
    }

    std::vector<uint32_t> &getMutableGroupSizeXValues() {
        if (full) {
            return groupSizeXValuesFull;
        } else {
            return groupSizeXValuesLimited;
        }
    }

    std::vector<uint32_t> &getMutableGroupSizeYValues() {
        if (full) {
            return groupSizeYValuesFull;
        } else {
            return groupSizeYValuesLimited;
        }
    }

    std::vector<uint32_t> &getMutableGroupSizeZValues() {
        if (full) {
            return groupSizeZValuesFull;
        } else {
            return groupSizeZValuesLimited;
        }
    }

  protected:
    std::vector<bool> mutateFirstValuesFull = {false, true};
    std::vector<bool> mutateSecondValuesFull = {false, true};
    std::vector<bool> useDifferentPoolFirstKernelValuesFull = {false, true};
    std::vector<bool> useDifferentPoolSecondKernelValuesFull = {false, true};
    std::vector<uint32_t> waitEventMaskValuesFull = {1, 2, 5, 6, 7};
    std::vector<uint32_t> baseIndexValuesFull = {1, 2, 3};
    std::vector<uint32_t> mutableIndexValuesFull = {1, 2, 3};

    std::vector<bool> mutateFirstValuesLimited = {true};
    std::vector<bool> mutateSecondValuesLimited = {true};
    std::vector<bool> useDifferentPoolFirstKernelValuesLimited = {false};
    std::vector<bool> useDifferentPoolSecondKernelValuesLimited = {true};
    std::vector<uint32_t> waitEventMaskValuesLimited = {2, 5};
    std::vector<uint32_t> baseIndexValuesLimited = {1, 2};
    std::vector<uint32_t> mutableIndexValuesLimited = {2, 3};

    std::vector<uint32_t> groupCountXValuesFull = {2, 1, 16};
    std::vector<uint32_t> groupCountYValuesFull = {8, 1, 4};
    std::vector<uint32_t> groupCountZValuesFull = {2, 4, 1};
    std::vector<uint32_t> groupCountXValuesLimited = {32};
    std::vector<uint32_t> groupCountYValuesLimited = {8};
    std::vector<uint32_t> groupCountZValuesLimited = {2};

    std::vector<uint32_t> groupSizeXValuesFull = {2, 4, 16};
    std::vector<uint32_t> groupSizeYValuesFull = {8, 1, 4};
    std::vector<uint32_t> groupSizeZValuesFull = {2, 1, 4};
    std::vector<uint32_t> groupSizeXValuesLimited = {32};
    std::vector<uint32_t> groupSizeYValuesLimited = {8};
    std::vector<uint32_t> groupSizeZValuesLimited = {2};

    bool full = false;
};

constexpr uint32_t maxEvents = 3;
using WaitEventMask = std::bitset<maxEvents>;

std::string testNameMultiExecutionMutatedCbEvents(MclTests::EventOptions eventOptions) {
    std::ostringstream testStream;

    testStream << "Mutate Signal and Wait CB Event and execute command list multiple times";
    testStream << "." << std::endl;
    MclTests::setEventTestStream(eventOptions, testStream);

    return testStream.str();
}

bool testMultiExecutionMutatedCbEvents(MclTests::ExecEnv *execEnv, ze_module_handle_t module, MclTests::EventOptions eventOptions, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    ze_command_list_handle_t cmdListAdd = nullptr, cmdListCopy = nullptr;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdListCopy, MclTests::isCbEvent(eventOptions), false);
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdListAdd, MclTests::isCbEvent(eventOptions), false);

    ze_command_list_handle_t cmdLists[] = {cmdListCopy, cmdListAdd};

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 2;

    std::vector<ze_event_handle_t> events(numEvents);
    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;

    void *stageBuffer1 = nullptr;
    void *stageBuffer2 = nullptr;

    void *dstBuffer = nullptr;

    void *patternBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &patternBuffer));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
    }
    memset(dstBuffer, 0, allocSize);

    uint64_t *zeroPtr = reinterpret_cast<uint64_t *>(patternBuffer);
    *zeroPtr = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, stageBuffer1, zeroPtr, sizeof(uint64_t), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, stageBuffer2, zeroPtr, sizeof(uint64_t), allocSize, nullptr, 0, nullptr));

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");
    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandIdCopy = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT | ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdListCopy, &commandIdDesc, &commandIdCopy));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdListCopy, copyKernel, &dispatchTraits, events[0], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListCopy));

    ElemType scalarValue = 0x1;
    groupSizeX = 32u;
    groupSizeY = 1u;
    groupSizeZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernel, groupSizeX, groupSizeY, groupSizeZ));

    // source will be mutated - use dedicated pointer
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 0, sizeof(void *), &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 1, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 2, sizeof(scalarValue), &scalarValue));

    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandIdAdd = 0;
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdListAdd, &commandIdDesc, &commandIdAdd));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdListAdd, addKernel, &dispatchTraits, nullptr, 1, &events[0]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListAdd));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 2, cmdLists, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = val1 + scalarValue;

    bool valid = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "before mutation fail" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, stageBuffer1, zeroPtr, sizeof(uint64_t), allocSize, nullptr, 0, nullptr));
    memset(dstBuffer, 0, allocSize);

    // mutate into another signal event and arguments in copy command list
    ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelDstArg.commandId = commandIdCopy;
    mutateKernelDstArg.argIndex = 1;
    mutateKernelDstArg.argSize = sizeof(void *);
    mutateKernelDstArg.pArgValue = &stageBuffer2;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelSrcArg.commandId = commandIdCopy;
    mutateKernelSrcArg.argIndex = 0;
    mutateKernelSrcArg.argSize = sizeof(void *);
    mutateKernelSrcArg.pArgValue = &srcBuffer2;
    mutateKernelSrcArg.pNext = &mutateKernelDstArg;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelSrcArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdListCopy, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdListCopy, commandIdCopy, events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListCopy));

    // mutate into another wait event and source in add command list
    mutateKernelDstArg.commandId = commandIdAdd;
    mutateKernelDstArg.argIndex = 0;
    mutateKernelDstArg.argSize = sizeof(void *);
    mutateKernelDstArg.pArgValue = &stageBuffer2;
    mutateKernelSrcArg.pNext = nullptr; // unlink dst parameter as in add kernel remains the same

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdListAdd, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdListAdd, commandIdAdd, 1, &events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListAdd));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 2, cmdLists, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = val2 + scalarValue;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation fail" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, stageBuffer2, zeroPtr, sizeof(uint64_t), allocSize, nullptr, 0, nullptr));
    memset(dstBuffer, 0, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 2, cmdLists, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation 2nd execution fail" << std::endl;
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, patternBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListCopy));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListAdd));
    execEnv->destroyKernelHandle(copyKernel);
    execEnv->destroyKernelHandle(addKernel);

    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }

    MclTests::destroyEventPool(eventPool, eventOptions);

    return valid;
}

bool testGroupSizeDimensionsTotalConstant(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t offset = 256;
    constexpr size_t steps = 3;
    constexpr size_t allocSize = elemSize * sizeof(ElemType) * offset * steps;

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_kernel_handle_t addKernelLinear = execEnv->getKernelHandle(module, "addScalarKernelLinear");

    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    void *mutatedSrcBuffer = reinterpret_cast<ElemType *>(srcBuffer) + offset;
    void *mutatedDstBuffer = reinterpret_cast<ElemType *>(dstBuffer) + offset;

    void *mutatedSrcBuffer2 = reinterpret_cast<ElemType *>(srcBuffer) + 2 * offset;
    void *mutatedDstBuffer2 = reinterpret_cast<ElemType *>(dstBuffer) + 2 * offset;

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    constexpr ElemType val3 = 14;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer)[i + offset] = val2;
        reinterpret_cast<ElemType *>(srcBuffer)[i + 2 * offset] = val3;
    }
    memset(dstBuffer, 0, allocSize);

    ElemType scalarValue = 0x2;

    uint32_t groupSizeX = elemSize;
    uint32_t groupSizeY = 1;
    uint32_t groupSizeZ = 1;
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernelLinear, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernelLinear, 0, sizeof(void *), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernelLinear, 1, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernelLinear, 2, sizeof(scalarValue), &scalarValue));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, addKernelLinear, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValueAdd = val1 + scalarValue;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer, elemSize) == false) {
            std::cerr << "before mutation - fail add operation" << std::endl;
            valid = false;
        }
    }

    ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    mutateGroupSize.commandId = commandId;
    mutateGroupSize.groupSizeX = 1;
    mutateGroupSize.groupSizeY = elemSize;
    mutateGroupSize.groupSizeZ = 1;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArgSrc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArgSrc.commandId = commandId;
    mutateKernelArgSrc.argIndex = 0;
    mutateKernelArgSrc.argSize = sizeof(void *);
    mutateKernelArgSrc.pArgValue = &mutatedSrcBuffer;
    mutateKernelArgSrc.pNext = &mutateGroupSize;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArgDst = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArgDst.commandId = commandId;
    mutateKernelArgDst.argIndex = 1;
    mutateKernelArgDst.argSize = sizeof(void *);
    mutateKernelArgDst.pArgValue = &mutatedDstBuffer;
    mutateKernelArgDst.pNext = &mutateKernelArgSrc;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArgDst;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValueAdd = val2 + scalarValue;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, mutatedDstBuffer, elemSize) == false) {
            std::cerr << "after mutation 1 - fail add operation" << std::endl;
            valid = false;
        }
    }

    mutateGroupSize.groupSizeX = 1;
    mutateGroupSize.groupSizeY = 1;
    mutateGroupSize.groupSizeZ = elemSize;

    mutateKernelArgSrc.pArgValue = &mutatedSrcBuffer2;

    mutateKernelArgDst.pArgValue = &mutatedDstBuffer2;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValueAdd = val3 + scalarValue;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, mutatedDstBuffer2, elemSize) == false) {
            std::cerr << "after mutation 2 - fail add operation" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(addKernelLinear);
    return valid;
}

bool testMutateBufferToNullAndBack(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode, bool nullPtrBeforeMutation) {
    using ElemType = uint8_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_kernel_handle_t copyKernelOptional = execEnv->getKernelHandle(module, "copyKernelOptional");

    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;
    void *nullPtrArgument = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val1 = 10;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer)[i] = val1;
    }
    memset(dstBuffer, 0, allocSize);

    uint32_t groupSizeX = 32;
    uint32_t groupSizeY = 1;
    uint32_t groupSizeZ = 1;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernelOptional, groupSizeX, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernelOptional, groupSizeX, groupSizeY, groupSizeZ));

    if (nullPtrBeforeMutation) {
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernelOptional, 0, sizeof(void *), nullPtrArgument));
    } else {
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernelOptional, 0, sizeof(void *), &srcBuffer));
    }
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernelOptional, 1, sizeof(void *), &dstBuffer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernelOptional, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = nullPtrBeforeMutation ? 0 : val1;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "before mutation" << std::endl;
            valid = false;
        }
    }

    // zero out dst buffer to later verify operation correctly sets the value
    ElemType zero = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, dstBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));

    // set the same src argument
    ze_mutable_kernel_argument_exp_desc_t mutateKernelArgSrc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArgSrc.commandId = commandId;
    mutateKernelArgSrc.argIndex = 0;
    mutateKernelArgSrc.argSize = sizeof(void *);
    if (nullPtrBeforeMutation) {
        mutateKernelArgSrc.pArgValue = &nullPtrArgument;
    } else {
        mutateKernelArgSrc.pArgValue = &srcBuffer;
    }

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArgSrc;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "same operation" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, dstBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));

    // set dst argument to the same value as before
    ze_mutable_kernel_argument_exp_desc_t mutateKernelArgDst = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArgDst.commandId = commandId;
    mutateKernelArgDst.argIndex = 1;
    mutateKernelArgDst.argSize = sizeof(void *);
    mutateKernelArgDst.pArgValue = &dstBuffer;

    // null/set source argument to opposite than before mutation
    if (nullPtrBeforeMutation) {
        mutateKernelArgSrc.pArgValue = &srcBuffer;
    } else {
        mutateKernelArgSrc.pArgValue = &nullPtrArgument;
    }
    mutateKernelArgSrc.pNext = &mutateKernelArgDst;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = nullPtrBeforeMutation ? val1 : 0;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation 1 - source set to ";
            std::cerr << (nullPtrBeforeMutation ? "memory" : "null") << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, dstBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));

    // null destination argument and restore source to original set
    if (nullPtrBeforeMutation) {
        mutateKernelArgSrc.pArgValue = &nullPtrArgument;
    } else {
        mutateKernelArgSrc.pArgValue = &srcBuffer;
    }

    // set null directly as argument value
    mutateKernelArgDst.pArgValue = nullPtrArgument;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    // expected is 0 as dst is null anyway
    expectedValue = 0;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation 2 - null destination, restore source to ";
            std::cerr << (nullPtrBeforeMutation ? "null" : "memory") << std::endl;
            valid = false;
        }
    }

    // restore destination
    mutateKernelArgDst.pArgValue = &dstBuffer;
    // set src to memory
    mutateKernelArgSrc.pArgValue = &srcBuffer;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = val1;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation 3 - restore destination, set src to memory" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernelOptional);
    return valid;
}

bool testSetNullPtrThenMutateToBuffer(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    ze_command_list_handle_t cmdList = nullptr;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    // prepare buffers
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val1 = 10;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer)[i] = val1;
    }
    memset(dstBuffer, 0, allocSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    void *nullPtrArgument = nullptr;

    // setting as nullptr
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &nullPtrArgument));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &nullPtrArgument));

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArgSrc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArgSrc.commandId = commandId;
    mutateKernelArgSrc.argIndex = 0;
    mutateKernelArgSrc.argSize = sizeof(void *);
    mutateKernelArgSrc.pArgValue = &srcBuffer;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArgDst = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArgDst.commandId = commandId;
    mutateKernelArgDst.argIndex = 1;
    mutateKernelArgDst.argSize = sizeof(void *);
    mutateKernelArgDst.pArgValue = &dstBuffer;
    mutateKernelArgDst.pNext = &mutateKernelArgSrc;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArgDst;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(val1, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation fail" << std::endl;
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

bool testMixedOffsetMemoryArgumentGlobalSize3DNonPow2(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 2048;
    constexpr size_t elemOffset = 1024;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);
    constexpr size_t processSizeInitial = 3 * 3 * 3;
    constexpr size_t processSizeMutated = 7 * 3 * 9;

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_kernel_handle_t addKernelLinear = execEnv->getKernelHandle(module, "addScalarKernelLinear");

    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    void *mutatedSrcBuffer = reinterpret_cast<ElemType *>(srcBuffer) + elemOffset;
    void *mutatedDstBuffer = reinterpret_cast<ElemType *>(dstBuffer) + elemOffset;

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    for (size_t i = 0; i < processSizeMutated; i++) {
        if (i < processSizeInitial) {
            reinterpret_cast<ElemType *>(srcBuffer)[i] = val1;
        }
        reinterpret_cast<ElemType *>(srcBuffer)[i + elemOffset] = val2;
    }
    memset(dstBuffer, 0, allocSize);

    ElemType scalarValue = 0x2;

    uint32_t groupSizeX = 3u;
    uint32_t groupSizeY = 3u;
    uint32_t groupSizeZ = 3u;
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernelLinear, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernelLinear, 0, sizeof(void *), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernelLinear, 1, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernelLinear, 2, sizeof(scalarValue), &scalarValue));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, addKernelLinear, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValueAdd = val1 + scalarValue;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer, processSizeInitial) == false) {
            std::cerr << "before mutation - fail add operation" << std::endl;
            valid = false;
        }
    }

    ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    mutateGroupSize.commandId = commandId;
    mutateGroupSize.groupSizeX = 7;
    mutateGroupSize.groupSizeY = 3;
    mutateGroupSize.groupSizeZ = 9;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArgSrc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArgSrc.commandId = commandId;
    mutateKernelArgSrc.argIndex = 0;
    mutateKernelArgSrc.argSize = sizeof(void *);
    mutateKernelArgSrc.pArgValue = &mutatedSrcBuffer;
    mutateKernelArgSrc.pNext = &mutateGroupSize;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArgDst = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArgDst.commandId = commandId;
    mutateKernelArgDst.argIndex = 1;
    mutateKernelArgDst.argSize = sizeof(void *);
    mutateKernelArgDst.pArgValue = &mutatedDstBuffer;
    mutateKernelArgDst.pNext = &mutateKernelArgSrc;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArgDst;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValueAdd = val2 + scalarValue;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, mutatedDstBuffer, processSizeMutated) == false) {
            std::cerr << "after mutation - fail add operation" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(addKernelLinear);
    return valid;
}

std::string testNameMutateSignalAndWaitEventsAndRemoveOldEvent(MclTests::EventOptions eventOptions, bool anotherPool) {
    std::ostringstream testStream;

    testStream << "Mutate Signal Event and Wait Event from ";
    if (anotherPool) {
        testStream << "another ";
    } else {
        testStream << "the same ";
    }
    testStream << "pool and remove old event case." << std::endl;
    MclTests::setEventTestStream(eventOptions, testStream);
    return testStream.str();
}

bool testMutateSignalAndWaitEventsAndRemoveOldEvent(MclTests::ExecEnv *execEnv, ze_module_handle_t module, MclTests::EventOptions eventOptions, bool anotherPool, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, MclTests::isCbEvent(eventOptions), false);

    ze_event_pool_handle_t eventPool = nullptr, eventPoolAnother = nullptr;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> events(numEvents), eventsFromAnother(numEvents);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);
    if (anotherPool) {
        MclTests::createEventPoolAndEvents(*execEnv, eventPoolAnother, numEvents, eventsFromAnother.data(), eventOptions);
    }

    ze_event_handle_t baseEvent = events[0];
    ze_event_handle_t mutableEvent = anotherPool ? eventsFromAnother[1] : events[1];

    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");
    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *stageBuffer1 = nullptr;
    void *dstBuffer1 = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));

    constexpr ElemType val1 = 10;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
    }
    memset(dstBuffer1, 0, allocSize);

    ElemType scalarValueAdd1 = 0x1;

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 1, sizeof(void *), &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 2, sizeof(scalarValueAdd1), &scalarValueAdd1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;
    uint64_t commandIdAdd = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandIdAdd));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, addKernel, &dispatchTraits, baseEvent, 0, nullptr));

    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
    uint64_t commandIdCopy = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandIdCopy));

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer1));

    dispatchTraits.groupCountX = allocSize / groupSizeX;
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, nullptr, 1, &baseEvent));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValueAdd = val1 + scalarValueAdd1;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer1, elemSize) == false) {
            std::cerr << "before mutation" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(baseEvent));
    }

    if (!MclTests::isCbEvent(eventOptions)) {
        SUCCESS_OR_TERMINATE(zeEventHostReset(baseEvent));
    }

    SUCCESS_OR_TERMINATE(zeEventDestroy(baseEvent));
    if (anotherPool) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(events[1]));
        MclTests::destroyEventPool(eventPool, eventOptions);
    }

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandIdAdd, mutableEvent));

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandIdCopy, 1, &mutableEvent));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer1, elemSize) == false) {
            std::cerr << "after mutation" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(mutableEvent));
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);
    execEnv->destroyKernelHandle(addKernel);
    if (!anotherPool) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(events[1]));
        MclTests::destroyEventPool(eventPool, eventOptions);
    }
    if (anotherPool) {
        for (auto event : eventsFromAnother) {
            SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        }
        MclTests::destroyEventPool(eventPoolAnother, eventOptions);
    }
    return valid;
}

bool testGlobalOffsetBif(MclTests::ExecEnv *execEnv, ze_module_handle_t module, uint32_t *inputOffset, bool aubMode) {
    constexpr size_t allocSize = 1024;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));
    memset(dstBuffer, 0, allocSize);
    uint64_t *outputData = reinterpret_cast<uint64_t *>(dstBuffer);

    ze_kernel_handle_t globalOffsetKernel = execEnv->getKernelHandle(module, "getGlobaOffset");

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(globalOffsetKernel, 1, 1, 1));

    uint32_t startGroupCount[] = {4, 4, 4};
    uint32_t startOffset[] = {0, 0, 0};

    ze_group_count_t offsetDispatchTraits;
    offsetDispatchTraits.groupCountX = startGroupCount[0];
    offsetDispatchTraits.groupCountY = startGroupCount[1];
    offsetDispatchTraits.groupCountZ = startGroupCount[2];

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(globalOffsetKernel, 0, sizeof(void *), &dstBuffer));

    uint8_t pattern = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, dstBuffer, &pattern, sizeof(pattern), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, globalOffsetKernel, &offsetDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        valid &= MclTests::validateGlobalOffsetData(outputData, startOffset, startGroupCount, true);
    }

    memset(dstBuffer, 0, allocSize);

    ze_mutable_global_offset_exp_desc_t mutateGlobalOffset = {ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXP_DESC};
    mutateGlobalOffset.commandId = commandId;
    mutateGlobalOffset.offsetX = inputOffset[0];
    mutateGlobalOffset.offsetY = inputOffset[1];
    mutateGlobalOffset.offsetZ = inputOffset[2];

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateGlobalOffset;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        valid &= MclTests::validateGlobalOffsetData(outputData, inputOffset, startGroupCount, false);
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(globalOffsetKernel);

    return valid;
}

bool testGlobalOffsetCopy(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 512;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);
    constexpr size_t partitionSize = allocSize / 2;

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    // prepare buffers
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val1 = 10;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer)[i] = val1;
    }
    memset(dstBuffer, 0, allocSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    size_t copiedElemSize = partitionSize / sizeof(ElemType);

    ze_group_count_t copyDispatchTraits;
    copyDispatchTraits.groupCountX = partitionSize / groupSizeX;
    copyDispatchTraits.groupCountY = 1u;
    copyDispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer));

    uint8_t pattern = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, dstBuffer, &pattern, sizeof(pattern), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &copyDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;

    ElemType expectedValue = val1;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, copiedElemSize) == false) {
            std::cerr << "before mutation fail - partition 1" << std::endl;
            valid = false;
        }

        ElemType zeroValue = 0;
        ElemType *zeroedBuffer = reinterpret_cast<ElemType *>(dstBuffer) + copiedElemSize;
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(zeroValue, zeroedBuffer, copiedElemSize) == false) {
            std::cerr << "before mutation fail - partition 2" << std::endl;
            valid = false;
        }
    }

    ze_mutable_global_offset_exp_desc_t mutateGlobalOffset = {ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXP_DESC};
    mutateGlobalOffset.commandId = commandId;
    mutateGlobalOffset.offsetX = partitionSize;
    mutateGlobalOffset.offsetY = 0;
    mutateGlobalOffset.offsetZ = 0;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateGlobalOffset;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        ElemType *copiedBuffer = reinterpret_cast<ElemType *>(dstBuffer) + copiedElemSize;
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, copiedBuffer, copiedElemSize) == false) {
            std::cerr << "after mutation fail - partition 1" << std::endl;
            valid = false;
        }

        ElemType zeroValue = 0;
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(zeroValue, dstBuffer, copiedElemSize) == false) {
            std::cerr << "after mutation fail - partition 2" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

bool testGroupSizeBif(MclTests::ExecEnv *execEnv, ze_module_handle_t module, uint32_t *groupSize, bool aubMode) {
    constexpr size_t allocSize = 256;
    void *dstBuffer = nullptr;
    void *stageBuffer = nullptr;

    uint32_t startGroupSize[] = {32, 1, 1};

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));
    memset(dstBuffer, 0, allocSize);
    uint64_t *outputData = reinterpret_cast<uint64_t *>(dstBuffer);

    ze_kernel_handle_t lwsKernel = execEnv->getKernelHandle(module, "getLocalWorkSize");

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(lwsKernel, startGroupSize[0], startGroupSize[1], startGroupSize[2]));

    ze_group_count_t lwsKernelDispatchTraits;
    lwsKernelDispatchTraits.groupCountX = 1;
    lwsKernelDispatchTraits.groupCountY = 1;
    lwsKernelDispatchTraits.groupCountZ = 1;

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(lwsKernel, 0, sizeof(void *), &stageBuffer));

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, lwsKernel, &lwsKernelDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, stageBuffer, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        valid &= MclTests::validateGroupSizeData(outputData, startGroupSize, true);
    }

    memset(dstBuffer, 0, allocSize);

    ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    mutateGroupSize.commandId = commandId;
    mutateGroupSize.groupSizeX = groupSize[0];
    mutateGroupSize.groupSizeY = groupSize[1];
    mutateGroupSize.groupSizeZ = groupSize[2];

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateGroupSize;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        valid &= MclTests::validateGroupSizeData(outputData, groupSize, false);
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(lwsKernel);

    return valid;
}

bool testGroupCountBif(MclTests::ExecEnv *execEnv, ze_module_handle_t module, uint32_t *groupCount, bool aubMode) {
    constexpr size_t allocSize = 64;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));
    memset(dstBuffer, 0, allocSize);
    uint64_t *outputData = reinterpret_cast<uint64_t *>(dstBuffer);

    ze_kernel_handle_t gwsKernel = execEnv->getKernelHandle(module, "getGlobalWorkSize");

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(gwsKernel, 1, 1, 1));

    uint32_t startGroupCount[] = {1, 1, 1};

    ze_group_count_t gwsKernelDispatchTraits;
    gwsKernelDispatchTraits.groupCountX = startGroupCount[0];
    gwsKernelDispatchTraits.groupCountY = startGroupCount[1];
    gwsKernelDispatchTraits.groupCountZ = startGroupCount[2];

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(gwsKernel, 0, sizeof(void *), &dstBuffer));

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, gwsKernel, &gwsKernelDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        valid &= MclTests::validateGroupCountData(outputData, startGroupCount, true);
    }

    memset(dstBuffer, 0, allocSize);

    ze_group_count_t newDispatchTraits = gwsKernelDispatchTraits;
    newDispatchTraits.groupCountX = groupCount[0];
    newDispatchTraits.groupCountY = groupCount[1];
    newDispatchTraits.groupCountZ = groupCount[2];

    ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
    mutateGroupCount.commandId = commandId;
    mutateGroupCount.pGroupCount = &newDispatchTraits;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateGroupCount;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        valid &= MclTests::validateGroupCountData(outputData, groupCount, false);
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(gwsKernel);

    return valid;
}

bool testGroupCountCopy(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 512;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    // prepare buffers
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val1 = 10;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer)[i] = val1;
    }
    memset(dstBuffer, 0, allocSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    size_t currentElemSize = elemSize / 2;

    ze_group_count_t copyDispatchTraits;
    copyDispatchTraits.groupCountX = (allocSize / 2) / groupSizeX;
    copyDispatchTraits.groupCountY = 1u;
    copyDispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer));

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &copyDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;

    ElemType expectedValue = val1;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, currentElemSize) == false) {
            std::cerr << "before mutation fail" << std::endl;
            valid = false;
        }

        ElemType zeroValue = 0;
        ElemType *zeroedBuffer = reinterpret_cast<ElemType *>(dstBuffer) + currentElemSize;

        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(zeroValue, zeroedBuffer, currentElemSize) == false) {
            std::cerr << "before mutation fail - zero part" << std::endl;
            valid = false;
        }
    }

    memset(dstBuffer, 0, allocSize);

    ze_group_count_t newDispatchTraits = copyDispatchTraits;
    newDispatchTraits.groupCountX *= 2;
    currentElemSize *= 2;

    ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
    mutateGroupCount.commandId = commandId;
    mutateGroupCount.pGroupCount = &newDispatchTraits;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateGroupCount;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, currentElemSize) == false) {
            std::cerr << "after mutation fail" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

bool testSignalExternalIncrementCbEvent(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    bool valid = true;

    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);
    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    constexpr uint64_t counterValue = 6;
    constexpr uint64_t incrementValue = 2;

    ze_command_list_handle_t cmdList = nullptr;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, true, false);

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    void *externalDeviceBuffer1 = nullptr;
    void *externalDeviceBuffer2 = nullptr;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, sizeof(uint64_t), 4096, execEnv->device, &externalDeviceBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, sizeof(uint64_t), 4096, execEnv->device, &externalDeviceBuffer2));

    uint64_t *counterValueBuffer1 = reinterpret_cast<uint64_t *>(externalDeviceBuffer1);
    uint64_t *counterValueBuffer2 = reinterpret_cast<uint64_t *>(externalDeviceBuffer2);

    constexpr uint64_t zero = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, externalDeviceBuffer1, &zero, sizeof(uint64_t), sizeof(uint64_t), nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, externalDeviceBuffer2, &zero, sizeof(uint64_t), sizeof(uint64_t), nullptr, 0, nullptr));

    zex_counter_based_event_external_storage_properties_t firstExternalStorageProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES};
    firstExternalStorageProperties.deviceAddress = counterValueBuffer1;
    firstExternalStorageProperties.incrementValue = incrementValue;
    firstExternalStorageProperties.completionValue = counterValue;
    zex_counter_based_event_desc_t firstEventDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    firstEventDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;
    firstEventDesc.pNext = &firstExternalStorageProperties;

    zex_counter_based_event_external_storage_properties_t secondExternalStorageProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_STORAGE_ALLOC_PROPERTIES};
    secondExternalStorageProperties.deviceAddress = counterValueBuffer2;
    secondExternalStorageProperties.incrementValue = incrementValue;
    secondExternalStorageProperties.completionValue = counterValue;
    zex_counter_based_event_desc_t secondEventDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    secondEventDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;
    secondEventDesc.pNext = &secondExternalStorageProperties;

    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;
    void *tmpBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 4096, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 4096, &dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, sizeof(uint64_t), 4096, &tmpBuffer));

    uint64_t *tmpBufferPtr = reinterpret_cast<uint64_t *>(tmpBuffer);

    ze_event_handle_t firstExternalEvent = nullptr;
    ze_event_handle_t secondExternalEvent = nullptr;
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::zexCounterBasedEventCreate2Func(execEnv->context, execEnv->device, &firstEventDesc, &firstExternalEvent));
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::zexCounterBasedEventCreate2Func(execEnv->context, execEnv->device, &secondEventDesc, &secondExternalEvent));

    uint32_t groupSize[3] = {32, 1, 1};

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSize[0], &groupSize[1], &groupSize[2]));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSize[0], groupSize[1], groupSize[2]));

    ze_group_count_t copyDispatchTraits;
    copyDispatchTraits.groupCountX = allocSize / groupSize[0];
    copyDispatchTraits.groupCountY = 1u;
    copyDispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer));

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;
    uint64_t commandId = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &copyDispatchTraits, firstExternalEvent, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeContextMakeMemoryResident(execEnv->context, execEnv->device, externalDeviceBuffer1, sizeof(uint64_t)));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (zeEventHostSynchronize(firstExternalEvent, 1) != ZE_RESULT_NOT_READY) {
            std::cerr << "first event signaled too early 1/3" << std::endl;
            valid = false;
        }

        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(execEnv->immCmdList, tmpBuffer, counterValueBuffer1, sizeof(uint64_t), nullptr, 0, nullptr));

        if (*tmpBufferPtr != incrementValue) {
            std::cerr << "first counter value (" << *tmpBufferPtr << ") is not correct 1/3" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (zeEventHostSynchronize(firstExternalEvent, 1) != ZE_RESULT_NOT_READY) {
            std::cerr << "first event signaled too early 2/3" << std::endl;
            valid = false;
        }

        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(execEnv->immCmdList, tmpBuffer, counterValueBuffer1, sizeof(uint64_t), nullptr, 0, nullptr));

        if (*tmpBufferPtr != (incrementValue * 2)) {
            std::cerr << "first counter value (" << *tmpBufferPtr << ") is not correct 2/3" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(firstExternalEvent, 1));

    if (!aubMode) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(execEnv->immCmdList, tmpBuffer, counterValueBuffer1, sizeof(uint64_t), nullptr, 0, nullptr));

        if (*tmpBufferPtr != (incrementValue * 3)) {
            std::cerr << "first counter value (" << *counterValueBuffer1 << ") is not correct 3/3" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandId, secondExternalEvent));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeContextMakeMemoryResident(execEnv->context, execEnv->device, externalDeviceBuffer2, sizeof(uint64_t)));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (zeEventHostSynchronize(secondExternalEvent, 1) != ZE_RESULT_NOT_READY) {
            std::cerr << "second event signaled too early 1/3" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(execEnv->immCmdList, tmpBuffer, counterValueBuffer2, sizeof(uint64_t), nullptr, 0, nullptr));

        if (*tmpBufferPtr != incrementValue) {
            std::cerr << "second counter value (" << *tmpBufferPtr << ") is not correct 1/3" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (zeEventHostSynchronize(secondExternalEvent, 1) != ZE_RESULT_NOT_READY) {
            std::cerr << "second event signaled too early 2/3" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(execEnv->immCmdList, tmpBuffer, counterValueBuffer2, sizeof(uint64_t), nullptr, 0, nullptr));

        if (*tmpBufferPtr != (incrementValue * 2)) {
            std::cerr << "second counter value (" << *tmpBufferPtr << ") is not correct 2/3" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(secondExternalEvent, 1));

    if (!aubMode) {

        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(execEnv->immCmdList, tmpBuffer, counterValueBuffer2, sizeof(uint64_t), nullptr, 0, nullptr));

        if (*tmpBufferPtr != (incrementValue * 3)) {
            std::cerr << "second counter value (" << *tmpBufferPtr << ") is not correct 3/3" << std::endl;
            valid = false;
        }

        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(execEnv->immCmdList, tmpBuffer, counterValueBuffer1, sizeof(uint64_t), nullptr, 0, nullptr));

        if (*tmpBufferPtr != (incrementValue * 3)) {
            std::cerr << "first counter value (" << *tmpBufferPtr << ") is not after mutation" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeEventDestroy(firstExternalEvent));
    SUCCESS_OR_TERMINATE(zeEventDestroy(secondExternalEvent));

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, externalDeviceBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, externalDeviceBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, tmpBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));

    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

bool testExternalMemoryCbWaitEvent(MclTests::ExecEnv *execEnv, ze_module_handle_t module,
                                   bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");
    ze_kernel_handle_t signalKernel = execEnv->getKernelHandle(module, "signalKernel");

    ze_command_list_handle_t cmdList1 = nullptr, cmdList2 = nullptr, dstCmdList = nullptr;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList1, false, false);
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList2, false, false);
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, dstCmdList, true, false);

    ze_event_pool_handle_t eventPoolInternal = nullptr;
    uint32_t numEvents = 2;

    std::vector<ze_event_handle_t> eventsInternal(numEvents);
    MclTests::createEventPoolAndEvents(*execEnv, eventPoolInternal, numEvents, eventsInternal.data(), MclTests::EventOptions::none);

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;

    void *stageBuffer1 = nullptr;
    void *stageBuffer2 = nullptr;

    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
    }
    memset(dstBuffer, 0, allocSize);

    // external memory buffers
    constexpr size_t externalEventSize = 4096;
    constexpr size_t externalSecondEventOffset = 1024;
    void *externalHostBuffer = nullptr;
    void *externalDeviceBuffer = nullptr;
    void *externalCounterValueBuffer = nullptr;
    uint64_t firstEventCompletionValue = 10;
    uint64_t secondEventCompletionValue = 20;

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, externalEventSize, 1, &externalHostBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, externalEventSize, 1, execEnv->device, &externalDeviceBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, externalEventSize, 1, &externalCounterValueBuffer));

    uint64_t firstExternalEventHostMemory = reinterpret_cast<uintptr_t>(externalHostBuffer);
    uint64_t firstExternalEventDeviceMemory = reinterpret_cast<uintptr_t>(externalDeviceBuffer);
    uint64_t firstExternalEventCounterValueMemory = reinterpret_cast<uintptr_t>(externalCounterValueBuffer);

    uint64_t secondExternalEventHostMemory = reinterpret_cast<uintptr_t>(externalHostBuffer) + externalSecondEventOffset;
    uint64_t secondExternalEventDeviceMemory = reinterpret_cast<uintptr_t>(externalDeviceBuffer) + externalSecondEventOffset;
    uint64_t secondExternalEventCounterValueMemory = reinterpret_cast<uintptr_t>(externalCounterValueBuffer) + externalSecondEventOffset;

    uint64_t *firstExternalEventHostMemoryPtr = reinterpret_cast<uint64_t *>(firstExternalEventHostMemory);
    uint64_t *firstExternalEventDeviceMemoryPtr = reinterpret_cast<uint64_t *>(firstExternalEventDeviceMemory);

    uint64_t *secondExternalEventHostMemoryPtr = reinterpret_cast<uint64_t *>(secondExternalEventHostMemory);
    uint64_t *secondExternalEventDeviceMemoryPtr = reinterpret_cast<uint64_t *>(secondExternalEventDeviceMemory);

    ze_event_handle_t firstExternalEvent = nullptr;
    ze_event_handle_t secondExternalEvent = nullptr;

    zex_counter_based_event_external_sync_alloc_properties_t firstExternalSyncAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES};
    firstExternalSyncAllocProperties.completionValue = firstEventCompletionValue;
    firstExternalSyncAllocProperties.deviceAddress = firstExternalEventDeviceMemoryPtr;
    firstExternalSyncAllocProperties.hostAddress = firstExternalEventHostMemoryPtr;
    zex_counter_based_event_desc_t firstEventDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    firstEventDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;
    firstEventDesc.pNext = &firstExternalSyncAllocProperties;

    zex_counter_based_event_external_sync_alloc_properties_t secondExternalSyncAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES};
    secondExternalSyncAllocProperties.completionValue = secondEventCompletionValue;
    secondExternalSyncAllocProperties.deviceAddress = secondExternalEventDeviceMemoryPtr;
    secondExternalSyncAllocProperties.hostAddress = secondExternalEventHostMemoryPtr;
    zex_counter_based_event_desc_t secondEventDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    secondEventDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;
    secondEventDesc.pNext = &secondExternalSyncAllocProperties;

    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::zexCounterBasedEventCreate2Func(execEnv->context, execEnv->device, &firstEventDesc, &firstExternalEvent));
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::zexCounterBasedEventCreate2Func(execEnv->context, execEnv->device, &secondEventDesc, &secondExternalEvent));

    ze_command_list_handle_t baseCmdListExecList[] = {cmdList1, dstCmdList};
    ze_command_list_handle_t mutableCmdListExecList[] = {cmdList2, dstCmdList};

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    ze_group_count_t copyDispatchTraits;
    copyDispatchTraits.groupCountX = allocSize / groupSizeX;
    copyDispatchTraits.groupCountY = 1u;
    copyDispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(signalKernel, 1, 1, 1));

    ze_group_count_t signalDispatchTraits;
    signalDispatchTraits.groupCountX = 1u;
    signalDispatchTraits.groupCountY = 1u;
    signalDispatchTraits.groupCountZ = 1u;

    // cmdlist 1
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer1));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList1, copyKernel, &copyDispatchTraits, eventsInternal[0], 0, nullptr));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(signalKernel, 0, sizeof(uint64_t), &firstExternalEventDeviceMemory));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(signalKernel, 1, sizeof(uint64_t), &firstExternalEventCounterValueMemory));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList1, signalKernel, &signalDispatchTraits, nullptr, 1, &eventsInternal[0]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList1));

    // cmdlist 2
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer2));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList2, copyKernel, &copyDispatchTraits, eventsInternal[1], 0, nullptr));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(signalKernel, 0, sizeof(uint64_t), &secondExternalEventDeviceMemory));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(signalKernel, 1, sizeof(uint64_t), &secondExternalEventCounterValueMemory));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList2, signalKernel, &signalDispatchTraits, nullptr, 1, &eventsInternal[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList2));

    // mutable cmdlist
    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(dstCmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(dstCmdList, copyKernel, &copyDispatchTraits, nullptr, 1, &firstExternalEvent));
    SUCCESS_OR_TERMINATE(zeCommandListClose(dstCmdList));

    // get external memory counter values
    uint64_t address = 0;
    uint64_t value = 0;
    SUCCESS_OR_TERMINATE(execEnv->zexEventGetDeviceAddressFunc(firstExternalEvent, &value, &address));
    *reinterpret_cast<uint64_t *>(firstExternalEventCounterValueMemory) = value;

    SUCCESS_OR_TERMINATE(execEnv->zexEventGetDeviceAddressFunc(secondExternalEvent, &value, &address));
    *reinterpret_cast<uint64_t *>(secondExternalEventCounterValueMemory) = value;

    SUCCESS_OR_TERMINATE(zeContextMakeMemoryResident(execEnv->context, execEnv->device, externalHostBuffer, externalEventSize));
    SUCCESS_OR_TERMINATE(zeContextMakeMemoryResident(execEnv->context, execEnv->device, externalDeviceBuffer, externalEventSize));
    SUCCESS_OR_TERMINATE(zeContextMakeMemoryResident(execEnv->context, execEnv->device, externalCounterValueBuffer, externalEventSize));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 2, baseCmdListExecList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = val1;

    bool valid = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "before mutation fail" << std::endl;
            valid = false;
        }
    }

    memset(dstBuffer, 0, allocSize);

    // change argument and event
    ze_mutable_kernel_argument_exp_desc_t mutateKernelArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArg.commandId = commandId;
    mutateKernelArg.argIndex = 0;
    mutateKernelArg.argSize = sizeof(void *);
    mutateKernelArg.pArgValue = &stageBuffer2;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(dstCmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(dstCmdList, commandId, 1, &secondExternalEvent));
    SUCCESS_OR_TERMINATE(zeCommandListClose(dstCmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 2, mutableCmdListExecList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = val2;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation fail" << std::endl;
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeContextEvictMemory(execEnv->context, execEnv->device, externalHostBuffer, externalEventSize));
    SUCCESS_OR_TERMINATE(zeContextEvictMemory(execEnv->context, execEnv->device, externalDeviceBuffer, externalEventSize));
    SUCCESS_OR_TERMINATE(zeContextEvictMemory(execEnv->context, execEnv->device, externalCounterValueBuffer, externalEventSize));

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, externalHostBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, externalDeviceBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, externalCounterValueBuffer));

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(dstCmdList));
    execEnv->destroyKernelHandle(copyKernel);
    execEnv->destroyKernelHandle(signalKernel);
    for (auto event : eventsInternal) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPoolInternal));

    SUCCESS_OR_TERMINATE(zeEventDestroy(firstExternalEvent));
    SUCCESS_OR_TERMINATE(zeEventDestroy(secondExternalEvent));

    return valid;
}

std::string testNameMixedWaitEvent(MclTests::EventOptions eventOptions, uint32_t sourceIndex, bool srcCmdListInOrder) {
    std::ostringstream testStream;

    testStream << "Mutate Mixed Wait Event case on In Order Command List";
    testStream << "." << std::endl;
    testStream << "Source command list is";
    if (!srcCmdListInOrder) {
        testStream << " not";
    }
    testStream << " in order type." << std::endl;
    MclTests::setEventTestStream(eventOptions, testStream);
    testStream << "." << std::endl;
    testStream << "Source index " << sourceIndex;

    return testStream.str();
}

bool testMixedWaitEvent(MclTests::ExecEnv *execEnv, ze_module_handle_t module,
                        MclTests::EventOptions eventOptions,
                        uint32_t sourceIndex, bool srcCmdListInOrder,
                        bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    ze_command_list_handle_t cmdList1 = nullptr, cmdList2 = nullptr, dstCmdList = nullptr;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList1, srcCmdListInOrder, false);
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList2, srcCmdListInOrder, false);
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, dstCmdList, true, false);

    ze_command_list_handle_t baseCmdList = nullptr;
    ze_command_list_handle_t mutableCmdList = nullptr;

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 2;

    std::vector<ze_event_handle_t> events(numEvents);
    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    ze_event_handle_t baseEvent = nullptr;
    ze_event_handle_t mutableEvent = nullptr;

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;

    void *stageBuffer1 = nullptr;
    void *stageBuffer2 = nullptr;

    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
    }
    memset(dstBuffer, 0, allocSize);

    ElemType baseValue = 0;
    ElemType mutableValue = 0;

    void *baseBuffer = nullptr;
    void *mutateBuffer = nullptr;

    if (sourceIndex == 1) {
        baseCmdList = cmdList1;
        baseEvent = events[0];
        baseBuffer = stageBuffer1;
        baseValue = val1;

        mutableCmdList = cmdList2;
        mutableEvent = events[1];
        mutateBuffer = stageBuffer2;
        mutableValue = val2;
    } else if (sourceIndex == 2) {
        baseCmdList = cmdList2;
        baseEvent = events[1];
        baseBuffer = stageBuffer2;
        baseValue = val2;

        mutableCmdList = cmdList1;
        mutableEvent = events[0];
        mutateBuffer = stageBuffer1;
        mutableValue = val1;
    }

    ze_command_list_handle_t baseCmdListExecList[] = {baseCmdList, dstCmdList};
    ze_command_list_handle_t mutableCmdListExecList[] = {mutableCmdList, dstCmdList};

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    // cmdlist 1
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer1));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList1, copyKernel, &dispatchTraits, events[0], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList1));

    // cmdlist 3
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer2));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList2, copyKernel, &dispatchTraits, events[1], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList2));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &baseBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer));

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(dstCmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(dstCmdList, copyKernel, &dispatchTraits, nullptr, 1, &baseEvent));
    SUCCESS_OR_TERMINATE(zeCommandListClose(dstCmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 2, baseCmdListExecList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = baseValue;

    bool valid = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "before mutation fail" << std::endl;
            valid = false;
        }
    }

    memset(dstBuffer, 0, allocSize);

    // mutate into another wait event and source
    ze_mutable_kernel_argument_exp_desc_t mutateKernelArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArg.commandId = commandId;
    mutateKernelArg.argIndex = 0;
    mutateKernelArg.argSize = sizeof(void *);
    mutateKernelArg.pArgValue = &mutateBuffer;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(dstCmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(dstCmdList, commandId, 1, &mutableEvent));
    SUCCESS_OR_TERMINATE(zeCommandListClose(dstCmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 2, mutableCmdListExecList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = mutableValue;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation fail" << std::endl;
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(dstCmdList));
    execEnv->destroyKernelHandle(copyKernel);
    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    return valid;
}

std::string testNameCbWaitEventMutateNoopMutateBack(MclTests::EventOptions eventOptions, uint32_t baseIndex, uint32_t mutateIndex) {
    std::ostringstream testStream;

    testStream << "Mutate Noop Mutate Back CB Wait Event case";
    testStream << "." << std::endl;
    MclTests::setEventTestStream(eventOptions, testStream);
    testStream << "." << std::endl;
    testStream << "Base index " << baseIndex << " mutable index " << mutateIndex;

    return testStream.str();
}

bool testCbWaitEventMutateNoopMutateBack(MclTests::ExecEnv *execEnv, ze_module_handle_t module,
                                         MclTests::EventOptions eventOptions,
                                         uint32_t baseIndex, uint32_t mutateIndex,
                                         bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    ze_command_list_handle_t cmdList1 = nullptr, cmdList2 = nullptr, cmdList3 = nullptr;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList1, MclTests::isCbEvent(eventOptions), false);
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList2, MclTests::isCbEvent(eventOptions), false);
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList3, MclTests::isCbEvent(eventOptions), false);

    uint32_t baseCmdListCount = 0;
    std::vector<ze_command_list_handle_t> baseCmdLists(2);

    uint32_t mutableCmdListCount = 0;
    std::vector<ze_command_list_handle_t> mutableCmdLists(2);

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 3;

    std::vector<ze_event_handle_t> events(numEvents);
    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    ze_event_handle_t baseEvent = nullptr;
    ze_event_handle_t mutableEvent = nullptr;
    ze_event_handle_t nullHandleEvent = nullptr;

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    void *srcBuffer3 = nullptr;

    void *stageBuffer1 = nullptr;
    void *stageBuffer2 = nullptr;
    void *stageBuffer3 = nullptr;

    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer3));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &stageBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &stageBuffer3));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    constexpr ElemType val3 = 14;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
        reinterpret_cast<ElemType *>(srcBuffer3)[i] = val3;
    }
    memset(stageBuffer1, 0, allocSize);
    memset(stageBuffer2, 0, allocSize);
    memset(stageBuffer3, 0, allocSize);
    memset(dstBuffer, 0, allocSize);

    uint32_t baseValue = 0;
    uint32_t mutableValue = 0;

    void *baseBuffer = nullptr;
    void *mutateBuffer = nullptr;

    if (baseIndex == 1) {
        baseCmdListCount = 1;
        baseCmdLists[0] = cmdList1;
        baseEvent = events[0];
        baseBuffer = stageBuffer1;
        baseValue = val1;
    } else if (baseIndex == 2) {
        baseCmdListCount = 2;
        baseCmdLists[0] = cmdList2;
        baseCmdLists[1] = cmdList1;
        baseEvent = events[1];
        baseBuffer = stageBuffer2;
        baseValue = val2;
    } else {
        baseCmdListCount = 2;
        baseCmdLists[0] = cmdList3;
        baseCmdLists[1] = cmdList1;
        baseEvent = events[2];
        baseBuffer = stageBuffer3;
        baseValue = val3;
    }

    if (mutateIndex == 1) {
        mutableCmdListCount = 1;
        mutableCmdLists[0] = cmdList1;
        mutableEvent = events[0];
        mutateBuffer = stageBuffer1;
        mutableValue = val1;
    } else if (mutateIndex == 2) {
        mutableCmdListCount = 2;
        mutableCmdLists[0] = cmdList2;
        mutableCmdLists[1] = cmdList1;
        mutableEvent = events[1];
        mutateBuffer = stageBuffer2;
        mutableValue = val2;
    } else {
        mutableCmdListCount = 2;
        mutableCmdLists[0] = cmdList3;
        mutableCmdLists[1] = cmdList1;
        mutableEvent = events[2];
        mutateBuffer = stageBuffer3;
        mutableValue = val3;
    }

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");
    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    // cmdlist 2
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer2));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList2, copyKernel, &dispatchTraits, events[1], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList2));

    // cmdlist 3
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer3));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer3));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList3, copyKernel, &dispatchTraits, events[2], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList3));

    // cmdlist 1

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer1));

    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList1, copyKernel, &dispatchTraits, events[0], 0, nullptr));

    ElemType scalarValue = 0x1;
    groupSizeX = 32u;
    groupSizeY = 1u;
    groupSizeZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernel, groupSizeX, groupSizeY, groupSizeZ));

    // source will be mutated - use dedicated pointer
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 0, sizeof(void *), &baseBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 1, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 2, sizeof(scalarValue), &scalarValue));

    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList1, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList1, addKernel, &dispatchTraits, nullptr, 1, &baseEvent));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList1));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, baseCmdListCount, baseCmdLists.data(), nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = baseValue + scalarValue;

    bool valid = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "before mutation fail" << std::endl;
            valid = false;
        }
    }
    memset(stageBuffer1, 0, allocSize);
    memset(stageBuffer2, 0, allocSize);
    memset(stageBuffer3, 0, allocSize);
    memset(dstBuffer, 0, allocSize);

    // mutate into another wait event and source
    ze_mutable_kernel_argument_exp_desc_t mutateKernelArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArg.commandId = commandId;
    mutateKernelArg.argIndex = 0;
    mutateKernelArg.argSize = sizeof(void *);
    mutateKernelArg.pArgValue = &mutateBuffer;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList1, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList1, commandId, 1, &mutableEvent));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList1));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, mutableCmdListCount, mutableCmdLists.data(), nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = mutableValue + scalarValue;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation fail" << std::endl;
            valid = false;
        }
    }
    memset(stageBuffer1, 0, allocSize);
    memset(stageBuffer2, 0, allocSize);
    memset(stageBuffer3, 0, allocSize);
    memset(dstBuffer, 0, allocSize);

    // mutate into null event, synchronize by queue
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList1, commandId, 1, &nullHandleEvent));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList1));

    if (mutateIndex == 2) {
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList2, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    } else if (mutateIndex == 3) {
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList3, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    }
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList1, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after noop mutation fail" << std::endl;
            valid = false;
        }
    }
    memset(stageBuffer1, 0, allocSize);
    memset(stageBuffer2, 0, allocSize);
    memset(stageBuffer3, 0, allocSize);
    memset(dstBuffer, 0, allocSize);

    // mutate back into original event and source
    mutateKernelArg.pArgValue = &baseBuffer;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList1, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList1, commandId, 1, &baseEvent));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList1));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, baseCmdListCount, baseCmdLists.data(), nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = baseValue + scalarValue;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after back mutation fail" << std::endl;
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer3));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer3));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList3));
    execEnv->destroyKernelHandle(copyKernel);
    execEnv->destroyKernelHandle(addKernel);
    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    return valid;
}

std::string testNameMutateNoopWaitEvent(MclTests::EventOptions eventOptions, WaitEventMask &bitMask) {
    std::ostringstream testStream;

    testStream << "Mutate Noop Wait Event case. ";
    MclTests::setEventTestStream(eventOptions, testStream);
    testStream << "." << std::endl;
    testStream << "Mutated events count " << bitMask.count() << " event to noop mutate " << bitMask;

    return testStream.str();
}

bool testMutateNoopWaitEvent(MclTests::ExecEnv *execEnv, ze_module_handle_t module,
                             MclTests::EventOptions eventOptions, WaitEventMask &bitMask,
                             bool aubMode) {
    constexpr size_t allocSize = 64;

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, MclTests::isCbEvent(eventOptions), false);

    ze_event_pool_handle_t eventPool = nullptr, eventPool2 = nullptr;
    uint32_t numEvents = 3;

    std::vector<ze_event_handle_t> events(numEvents), events2(numEvents), mutableEvents(numEvents);
    std::vector<uint32_t> poolIndices(numEvents);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);
    MclTests::createEventPoolAndEvents(*execEnv, eventPool2, numEvents, events2.data(), eventOptions);

    for (uint32_t i = 0; i < numEvents; i++) {
        mutableEvents[i] = events[i];
    }

    uint32_t testMutableEventsCount = 0;
    if (bitMask.test(0)) {
        poolIndices[testMutableEventsCount] = 0;
        ++testMutableEventsCount;
    }
    if (bitMask.test(1)) {
        poolIndices[testMutableEventsCount] = 1;
        ++testMutableEventsCount;
    }
    if (bitMask.test(2)) {
        poolIndices[testMutableEventsCount] = 2;
        ++testMutableEventsCount;
    }

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *dstBuffer1 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));

    constexpr uint8_t val1 = 0x10;

    memset(srcBuffer1, val1, allocSize);

    memset(dstBuffer1, 0, allocSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;

    uint64_t commandId1 = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId1));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, nullptr, numEvents, mutableEvents.data()));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));

    for (uint32_t i = 0; i < numEvents; i++) {
        SUCCESS_OR_TERMINATE(zeEventHostSignal(mutableEvents[i]));
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool validate = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: before mutation fail" << std::endl;
            validate = false;
        }
    }

    for (uint32_t i = 0; i < numEvents; i++) {
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[i]));
    }

    memset(dstBuffer1, 0, allocSize);

    // zero selected events to noop mutation
    for (uint32_t i = 0; i < testMutableEventsCount; i++) {
        auto poolIndex = poolIndices[i];
        mutableEvents[poolIndex] = nullptr;
    }
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandId1, numEvents, mutableEvents.data()));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));

    for (uint32_t i = 0; i < numEvents; i++) {
        if (mutableEvents[i] != nullptr) {
            SUCCESS_OR_TERMINATE(zeEventHostSignal(mutableEvents[i]));
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: after noop mutation fail" << std::endl;
            validate = false;
        }
    }

    // bring back selected events to event mutation from another pool
    for (uint32_t i = 0; i < numEvents; i++) {
        if (mutableEvents[i] != nullptr) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(mutableEvents[i]));
        }
    }

    memset(dstBuffer1, 0, allocSize);

    for (uint32_t i = 0; i < testMutableEventsCount; i++) {
        auto poolIndex = poolIndices[i];
        mutableEvents[poolIndex] = events2[poolIndex];
    }
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandId1, numEvents, mutableEvents.data()));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));

    for (uint32_t i = 0; i < numEvents; i++) {
        if (mutableEvents[i] != nullptr) {
            SUCCESS_OR_TERMINATE(zeEventHostSignal(mutableEvents[i]));
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: after second pool mutation fail" << std::endl;
            validate = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);
    for (auto event : events2) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool2, eventOptions);

    return validate;
}

std::string testNameMutateWaitEvent(bool mutateFirst, bool mutateSecond, bool useDifferentEventPoolFirst, bool useDifferentEventPoolSecond, MclTests::EventOptions eventOptions, WaitEventMask &eventMask) {
    std::ostringstream testStream;

    testStream << "Mutate Wait Event case";

    testStream << std::endl;
    testStream << (mutateFirst ? "mutate first" : "immutable first");
    if (mutateFirst) {
        if (useDifferentEventPoolFirst) {
            testStream << " from different pool";
        } else {
            testStream << " from same pool";
        }
    }

    testStream << std::endl;
    testStream << (mutateSecond ? "mutate second" : "immutable second");
    if (mutateSecond) {
        if (useDifferentEventPoolSecond) {
            testStream << " from different pool";
        } else {
            testStream << " from same pool";
        }
    }
    testStream << "." << std::endl;

    MclTests::setEventTestStream(eventOptions, testStream);
    testStream << "." << std::endl;
    testStream << "Mutated events count " << eventMask.count() << " event to mutate " << eventMask;

    return testStream.str();
}

bool testMutateWaitEvent(MclTests::ExecEnv *execEnv, ze_module_handle_t module,
                         bool mutateFirstAppend, bool mutateSecondAppend, bool useDifferentEventPoolFirst, bool useDifferentEventPoolSecond,
                         MclTests::EventOptions eventOptions, WaitEventMask &eventMask, bool aubMode) {
    constexpr size_t allocSize = 64;

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, MclTests::isCbEvent(eventOptions), false);

    ze_event_pool_handle_t eventPool = nullptr, eventPoolAlternative = nullptr;
    uint32_t numEvents = 12;
    uint32_t numEventsAlternative = 6;
    uint32_t numMutableEvents = numEvents / 2;

    std::vector<ze_event_handle_t> events(numEvents), eventsAlternative(numEventsAlternative);
    std::vector<ze_event_handle_t> mutableEventsFirstAppend(numMutableEvents), mutableEventsSecondAppend(numMutableEvents);
    std::vector<uint32_t> poolIndices(numMutableEvents);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    bool differentEventPool = (mutateFirstAppend && useDifferentEventPoolFirst) || (mutateSecondAppend && useDifferentEventPoolSecond);
    if (differentEventPool) {
        MclTests::createEventPoolAndEvents(*execEnv, eventPoolAlternative, numEventsAlternative, eventsAlternative.data(), eventOptions);
    }

    uint32_t testMutableEventsCount = 0;
    if (eventMask.test(0)) {
        mutableEventsFirstAppend[testMutableEventsCount] = events[0];
        mutableEventsSecondAppend[testMutableEventsCount] = events[6];
        poolIndices[testMutableEventsCount] = 0;
        ++testMutableEventsCount;
    }
    if (eventMask.test(1)) {
        mutableEventsFirstAppend[testMutableEventsCount] = events[1];
        mutableEventsSecondAppend[testMutableEventsCount] = events[7];
        poolIndices[testMutableEventsCount] = 1;
        ++testMutableEventsCount;
    }
    if (eventMask.test(2)) {
        mutableEventsFirstAppend[testMutableEventsCount] = events[2];
        mutableEventsSecondAppend[testMutableEventsCount] = events[8];
        poolIndices[testMutableEventsCount] = 2;
        ++testMutableEventsCount;
    }

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer2));

    constexpr uint8_t val1 = 0x10;
    constexpr uint8_t val2 = 0x12;

    memset(srcBuffer1, val1, allocSize);
    memset(srcBuffer2, val2, allocSize);

    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;

    uint64_t commandId1 = 0;
    if (mutateFirstAppend) {
        SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId1));
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, nullptr, testMutableEventsCount, mutableEventsFirstAppend.data()));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer2));

    uint64_t commandId2 = 0;
    if (mutateSecondAppend) {
        SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId2));
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, nullptr, testMutableEventsCount, mutableEventsSecondAppend.data()));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));

    for (uint32_t i = 0; i < testMutableEventsCount; i++) {
        SUCCESS_OR_TERMINATE(zeEventHostSignal(mutableEventsFirstAppend[i]));
        SUCCESS_OR_TERMINATE(zeEventHostSignal(mutableEventsSecondAppend[i]));
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool validate = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: before mutation fail" << std::endl;
            validate = false;
        }
        if (LevelZeroBlackBoxTests::validate(srcBuffer2, dstBuffer2, allocSize) == false) {
            std::cerr << "dst2 <-> src2: before mutation fail" << std::endl;
            validate = false;
        }
    }

    for (uint32_t i = 0; i < testMutableEventsCount; i++) {
        SUCCESS_OR_TERMINATE(zeEventHostReset(mutableEventsFirstAppend[i]));
        SUCCESS_OR_TERMINATE(zeEventHostReset(mutableEventsSecondAppend[i]));
    }

    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    if (mutateFirstAppend) {
        for (uint32_t i = 0; i < testMutableEventsCount; i++) {
            auto poolIndex = poolIndices[i];
            if (useDifferentEventPoolFirst) {
                mutableEventsFirstAppend[i] = eventsAlternative[0 + poolIndex];
            } else {
                mutableEventsFirstAppend[i] = events[3 + poolIndex];
            }
        }
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandId1, testMutableEventsCount, mutableEventsFirstAppend.data()));
    }

    if (mutateSecondAppend) {
        for (uint32_t i = 0; i < testMutableEventsCount; i++) {
            auto poolIndex = poolIndices[i];
            if (useDifferentEventPoolSecond) {
                mutableEventsSecondAppend[i] = eventsAlternative[3 + poolIndex];
            } else {
                mutableEventsSecondAppend[i] = events[9 + poolIndex];
            }
        }
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandId2, testMutableEventsCount, mutableEventsSecondAppend.data()));
    }

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));

    for (uint32_t i = 0; i < testMutableEventsCount; i++) {
        SUCCESS_OR_TERMINATE(zeEventHostSignal(mutableEventsFirstAppend[i]));
        SUCCESS_OR_TERMINATE(zeEventHostSignal(mutableEventsSecondAppend[i]));
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: after mutation fail" << std::endl;
            validate = false;
        }
        if (LevelZeroBlackBoxTests::validate(srcBuffer2, dstBuffer2, allocSize) == false) {
            std::cerr << "dst2 <-> src2: after mutation fail" << std::endl;
            validate = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);
    if (eventPoolAlternative != nullptr) {
        for (auto event : eventsAlternative) {
            SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        }
        MclTests::destroyEventPool(eventPoolAlternative, eventOptions);
    }

    return validate;
}

std::string testNameMutateSignalEventMutateOriginalBack(MclTests::EventOptions eventOptions) {
    std::ostringstream testStream;
    testStream << "Mutate Original Signal Event back case ";

    MclTests::setEventTestStream(eventOptions, testStream);
    return testStream.str();
}

bool testMutateSignalEventMutateOriginalBack(MclTests::ExecEnv *execEnv, ze_module_handle_t module, MclTests::EventOptions eventOptions, bool aubMode) {
    constexpr size_t allocSize = 128;

    ze_command_list_handle_t cmdList;
    bool inOrderCmdList = MclTests::isCbEvent(eventOptions);
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, inOrderCmdList, false);
    bool cbEvents = inOrderCmdList;

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> events(numEvents);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *dstBuffer1 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));

    constexpr uint8_t val1 = 0x10;

    memset(srcBuffer1, val1, allocSize);
    memset(dstBuffer1, 0, allocSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    uint64_t commandId1 = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId1));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, events[0], 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool validate = true;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: before mutation fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[0]));
    }

    if (!cbEvents) {
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[0]));
    }
    memset(dstBuffer1, 0, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandId1, events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: after mutation fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[1]));
    }

    if (!cbEvents) {
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[1]));
    }
    memset(dstBuffer1, 0, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandId1, events[0]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: after bringing original event mutation fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[0]));
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    return validate;
}

std::string testNameMutateSignalEvent(bool mutateFirst, bool mutateSecond, bool useDifferentEventPoolFirst, bool useDifferentEventPoolSecond, MclTests::EventOptions eventOptions) {
    std::ostringstream testStream;

    testStream << "Mutate Signal Event case";

    testStream << std::endl;
    testStream << (mutateFirst ? "mutate first" : "immutable first");
    if (mutateFirst) {
        if (useDifferentEventPoolFirst) {
            testStream << " from different pool";
        } else {
            testStream << " from same pool";
        }
    }

    testStream << std::endl;
    testStream << (mutateSecond ? "mutate second" : "immutable second");
    if (mutateSecond) {
        if (useDifferentEventPoolSecond) {
            testStream << " from different pool";
        } else {
            testStream << " from same pool";
        }
    }
    testStream << "." << std::endl;

    MclTests::setEventTestStream(eventOptions, testStream);
    return testStream.str();
}

bool testMutateSignalEvent(MclTests::ExecEnv *execEnv, ze_module_handle_t module,
                           bool mutateFirst, bool mutateSecond, bool useDifferentEventPoolFirst, bool useDifferentEventPoolSecond, MclTests::EventOptions eventOptions, bool aubMode) {
    constexpr size_t allocSize = 128;

    ze_command_list_handle_t cmdList;
    bool inOrderCmdList = MclTests::isCbEvent(eventOptions);
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, inOrderCmdList, false);
    bool cbEvents = inOrderCmdList;

    ze_event_pool_handle_t eventPool = nullptr, eventPoolAlternative = nullptr;
    uint32_t numEvents = 4;
    uint32_t numEventsAlternative = 2;
    std::vector<ze_event_handle_t> events(numEvents), eventsAlternative(numEventsAlternative);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);
    bool differentEventPool = (mutateFirst && useDifferentEventPoolFirst) || (mutateSecond && useDifferentEventPoolSecond);
    if (differentEventPool) {
        MclTests::createEventPoolAndEvents(*execEnv, eventPoolAlternative, numEventsAlternative, eventsAlternative.data(), eventOptions);
    }

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer2));

    constexpr uint8_t val1 = 0x10;
    constexpr uint8_t val2 = 0x12;

    memset(srcBuffer1, val1, allocSize);
    memset(srcBuffer2, val2, allocSize);

    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    uint64_t commandId1 = 0;
    if (mutateFirst) {
        SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId1));
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, events[0], 0, nullptr));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer2));

    uint64_t commandId2 = 0;
    if (mutateSecond) {
        SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId2));
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, events[1], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool validate = true;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: before mutation fail" << std::endl;
            validate = false;
        }
        if (LevelZeroBlackBoxTests::validate(srcBuffer2, dstBuffer2, allocSize) == false) {
            std::cerr << "dst2 <-> src2: before mutation fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[0]));
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[1]));
    }

    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    ze_event_handle_t nextEvent1 = events[0];
    if (mutateFirst) {
        nextEvent1 = useDifferentEventPoolFirst ? eventsAlternative[0] : events[2];
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandId1, nextEvent1));
    } else if (!cbEvents) {
        SUCCESS_OR_TERMINATE(zeEventHostReset(nextEvent1));
    }
    ze_event_handle_t nextEvent2 = events[1];
    if (mutateSecond) {
        nextEvent2 = useDifferentEventPoolSecond ? eventsAlternative[1] : events[3];
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandId2, nextEvent2));
    } else if (!cbEvents) {
        SUCCESS_OR_TERMINATE(zeEventHostReset(nextEvent2));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: after mutation fail" << std::endl;
            validate = false;
        }
        if (LevelZeroBlackBoxTests::validate(srcBuffer2, dstBuffer2, allocSize) == false) {
            std::cerr << "dst2 <-> src2: after mutation fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(nextEvent1));
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(nextEvent2));
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);
    if (eventPoolAlternative != nullptr) {
        for (auto event : eventsAlternative) {
            SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        }
        MclTests::destroyEventPool(eventPoolAlternative, eventOptions);
    }

    return validate;
}

std::string testNameMutateSignalEventOnInOrderCommandList(MclTests::EventOptions eventOptions) {
    std::ostringstream testStream;
    testStream << "Mutate Signal Event case on In Order Command List" << std::endl;
    MclTests::setEventTestStream(eventOptions, testStream);
    return testStream.str();
}

bool testMutateSignalEventOnInOrderCommandList(MclTests::ExecEnv *execEnv, ze_module_handle_t module, MclTests::EventOptions eventOptions, bool aubMode) {
    constexpr size_t allocSize = 128;

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, true, false);

    ze_event_pool_handle_t eventPool = nullptr, eventPool2 = nullptr;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> events(numEvents), events2(numEvents);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);
    MclTests::createEventPoolAndEvents(*execEnv, eventPool2, numEvents, events2.data(), eventOptions);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *dstBuffer1 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));

    constexpr uint8_t val1 = 0x10;

    memset(srcBuffer1, val1, allocSize);
    memset(dstBuffer1, 0, allocSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;

    uint64_t commandId1 = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId1));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, events[0], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool validate = true;
    ze_kernel_timestamp_result_t events0Timestamp{};
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: before mutation fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[0]));
        if (isTimestampEvent(eventOptions)) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[0], &events0Timestamp));
        }
    }
    memset(dstBuffer1, 0, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandId1, events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ze_kernel_timestamp_result_t events1Timestamp{};
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: after mutation 1 fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[1]));
        if (isTimestampEvent(eventOptions)) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[1], &events1Timestamp));
        }
    }

    memset(dstBuffer1, 0, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandId1, events2[0]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ze_kernel_timestamp_result_t events20Timestamp{};
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: after mutation 2 fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events2[0]));
        if (isTimestampEvent(eventOptions)) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events2[0], &events20Timestamp));
        }
    }
    memset(dstBuffer1, 0, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandId1, events2[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ze_kernel_timestamp_result_t events21Timestamp{};
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: after mutation 3 fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events2[1]));
        if (isTimestampEvent(eventOptions)) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events2[1], &events21Timestamp));
        }
    }

    if (!aubMode) {
        if (isTimestampEvent(eventOptions)) {
            if (events21Timestamp.context.kernelEnd < events20Timestamp.context.kernelEnd) {
                std::cerr << "events2[1] smaller than events2[0]" << std::endl;
                validate = false;
            }
            if (events20Timestamp.context.kernelEnd < events1Timestamp.context.kernelEnd) {
                std::cerr << "events2[0] smaller than events[1]" << std::endl;
                validate = false;
            }
            if (events1Timestamp.context.kernelEnd < events0Timestamp.context.kernelEnd) {
                std::cerr << "events[1] smaller than events[0]" << std::endl;
                validate = false;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);
    for (auto event : events2) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool2, eventOptions);

    return validate;
}

bool testMutateMemoryArgument(MclTests::ExecEnv *execEnv, ze_module_handle_t module, ze_command_list_handle_t &sharedCmdList, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, sharedCmdList, false, false);

    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
    }
    memset(dstBuffer, 0, allocSize);

    ElemType scalarValue = 0x1;

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 1, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 2, sizeof(scalarValue), &scalarValue));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(sharedCmdList, &commandIdDesc, &commandId));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(sharedCmdList, addKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(sharedCmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &sharedCmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = val1 + scalarValue;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "before mutation fail" << std::endl;
            valid = false;
        }
    }

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArg.commandId = commandId;
    mutateKernelArg.argIndex = 0;
    mutateKernelArg.argSize = sizeof(void *);
    mutateKernelArg.pArgValue = &srcBuffer2;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(sharedCmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(sharedCmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &sharedCmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = val2 + scalarValue;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation fail" << std::endl;
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    execEnv->destroyKernelHandle(addKernel);
    return valid;
}

bool testResetCmdlistAndMutateScalarArgument(MclTests::ExecEnv *execEnv, ze_module_handle_t module, ze_command_list_handle_t &sharedCmdList, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    SUCCESS_OR_TERMINATE(zeCommandListReset(sharedCmdList));

    ze_kernel_handle_t mulKernel = execEnv->getKernelHandle(module, "mulScalarKernel");

    // prepare buffers
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val = 5;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer)[i] = val;
    }
    memset(dstBuffer, 0, allocSize);

    ElemType scalarValue = 0x2;

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(mulKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(mulKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 0, sizeof(void *), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 1, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 2, sizeof(scalarValue), &scalarValue));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(sharedCmdList, &commandIdDesc, &commandId));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(sharedCmdList, mulKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(sharedCmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &sharedCmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = val * scalarValue;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "before mutation fail" << std::endl;
            valid = false;
        }
    }

    scalarValue = 0x3;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArg.commandId = commandId;
    mutateKernelArg.argIndex = 2;
    mutateKernelArg.argSize = sizeof(scalarValue);
    mutateKernelArg.pArgValue = &scalarValue;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(sharedCmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(sharedCmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &sharedCmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = val * scalarValue;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation fail" << std::endl;
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(sharedCmdList));
    execEnv->destroyKernelHandle(mulKernel);
    return valid;
}

std::string testNameMutateComplexStructurePassedAsImmediate(bool conditionalAdd, bool conditionalMul, uint32_t variant, bool includeAdd) {
    std::ostringstream testStream;

    testStream << "Mutate Complex Structure Passed As Immediate. Struct variant: " << variant << ", include add: " << std::boolalpha << includeAdd;
    testStream << std::endl;

    auto streamCaseString = [&testStream](bool conditional, bool mul) {
        const std::string opStr = mul ? "mul" : "add";
        if (conditional) {
            testStream << "Execute ";
        } else {
            testStream << "Noop ";
        }
        testStream << opStr;
        testStream << " before mutation. Opposite after mutation.";
    };

    streamCaseString(conditionalMul, true);
    if (includeAdd) {
        testStream << std::endl;
        streamCaseString(conditionalAdd, false);
    }

    return testStream.str();
}

using ComplexStructureType = uint64_t;

struct ConditionalCalculations1 {
    char conditionMul;
    ComplexStructureType valueMul;
    char conditionAdd;
    ComplexStructureType valueAdd;
};

struct ConditionalCalculations2 {
    ComplexStructureType valueMul;
    ComplexStructureType valueAdd;
    char conditionMul;
    char conditionAdd;
};

struct ConditionalCalculations3 {
    ComplexStructureType valueMul;
    char conditionMul;
    ComplexStructureType valueAdd;
    char conditionAdd;
};

struct ConditionalCalculations4 {
    ComplexStructureType valueMul;
    char conditionMul;
    char conditionAdd;
    ComplexStructureType valueAdd;
};

struct ConditionalMulCalculations1 {
    char conditionMul;
    ComplexStructureType valueMul;
};

struct ConditionalMulCalculations2 {
    ComplexStructureType valueMul;
    char conditionMul;
};

template <typename ConditionalStruct, uint32_t variantT, bool includeAddT>
bool testMutateComplexStructurePassedAsImmediate(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool conditionalAdd, bool conditionalMul, bool aubMode) {
    using ElemType = ComplexStructureType;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_kernel_handle_t computeKernel;
    {
        auto makeKernelNameMutateComplexStructure = [](uint32_t variant, bool includeAdd) {
            std::ostringstream kernelName;
            kernelName << "computeScalar";
            if (!includeAdd) {
                kernelName << "Mul";
            }
            kernelName << "Kernel";
            kernelName << variant;
            return kernelName.str();
        };

        std::string kernelName = makeKernelNameMutateComplexStructure(variantT, includeAddT);
        computeKernel = execEnv->getKernelHandle(module, kernelName);
    }

    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val1 = 10;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer)[i] = val1;
    }
    memset(dstBuffer, 0, allocSize);

    ElemType valueAdd = 15;
    ElemType valueMul = 4;

    ConditionalStruct input = {};

    input.conditionMul = conditionalMul;
    input.valueMul = valueMul;
    if constexpr (includeAddT) {
        input.conditionAdd = conditionalAdd;
        input.valueAdd = valueAdd;
    }

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(computeKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(computeKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(computeKernel, 0, sizeof(void *), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(computeKernel, 1, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(computeKernel, 2, sizeof(input), &input));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, computeKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = val1;
    if (conditionalMul) {
        expectedValue *= valueMul;
    }
    if constexpr (includeAddT) {
        if (conditionalAdd) {
            expectedValue += valueAdd;
        }
    }

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "before mutation" << std::endl;
            valid = false;
        }
    }
    memset(dstBuffer, 0, allocSize);

    valueMul = 8;
    conditionalMul = !conditionalMul;

    input.conditionMul = conditionalMul;
    input.valueMul = valueMul;

    if constexpr (includeAddT) {
        valueAdd = 20;
        conditionalAdd = !conditionalAdd;

        input.conditionAdd = conditionalAdd;
        input.valueAdd = valueAdd;
    }

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArg.commandId = commandId;
    mutateKernelArg.argIndex = 2;
    mutateKernelArg.argSize = sizeof(input);
    mutateKernelArg.pArgValue = &input;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = val1;
    if (conditionalMul) {
        expectedValue *= valueMul;
    }
    if constexpr (includeAddT) {
        if (conditionalAdd) {
            expectedValue += valueAdd;
        }
    }

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation" << std::endl;
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(computeKernel);
    return valid;
}

bool testMutateAfterFreedMemoryArgument(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");

    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
    }
    memset(dstBuffer, 0, allocSize);

    ElemType scalarValue = 0x1;

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 1, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 2, sizeof(scalarValue), &scalarValue));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, addKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValueAdd = val1 + scalarValue;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer, elemSize) == false) {
            std::cerr << "before mutation - fail add operation" << std::endl;
            valid = false;
        }
    }

    memset(dstBuffer, 0, allocSize);
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArg.commandId = commandId;
    mutateKernelArg.argIndex = 0;
    mutateKernelArg.argSize = sizeof(void *);
    mutateKernelArg.pArgValue = &srcBuffer2;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValueAdd = val2 + scalarValue;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer, elemSize) == false) {
            std::cerr << "after mutation - fail add operation" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(addKernel);
    return valid;
}

bool testOffsetMemoryArgument(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 2048;
    constexpr size_t elemOffset = 1024;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);
    constexpr size_t processSize = 64;

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");

    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    void *mutatedSrcBuffer = reinterpret_cast<ElemType *>(srcBuffer) + elemOffset;
    void *mutatedDstBuffer = reinterpret_cast<ElemType *>(dstBuffer) + elemOffset;

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    for (size_t i = 0; i < processSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer)[i + elemOffset] = val2;
    }
    memset(dstBuffer, 0, allocSize);

    ElemType scalarValue = 0x2;

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addKernel, processSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 0, sizeof(void *), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 1, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 2, sizeof(scalarValue), &scalarValue));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = processSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, addKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValueAdd = val1 + scalarValue;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer, processSize) == false) {
            std::cerr << "before mutation - fail add operation" << std::endl;
            valid = false;
        }
    }

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArgSrc = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArgSrc.commandId = commandId;
    mutateKernelArgSrc.argIndex = 0;
    mutateKernelArgSrc.argSize = sizeof(void *);
    mutateKernelArgSrc.pArgValue = &mutatedSrcBuffer;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArgDst = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArgDst.commandId = commandId;
    mutateKernelArgDst.argIndex = 1;
    mutateKernelArgDst.argSize = sizeof(void *);
    mutateKernelArgDst.pArgValue = &mutatedDstBuffer;
    mutateKernelArgDst.pNext = &mutateKernelArgSrc;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArgDst;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValueAdd = val2 + scalarValue;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, mutatedDstBuffer, processSize) == false) {
            std::cerr << "after mutation - fail add operation" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(addKernel);
    return valid;
}

bool testMutateMultipleMemoryArguments(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool firstImmutable, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");
    ze_kernel_handle_t mulKernel = execEnv->getKernelHandle(module, "mulScalarKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer2));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
    }
    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    ElemType scalarValue1 = 0x1;
    ElemType scalarValue2 = 0x2;

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 1, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 2, sizeof(scalarValue1), &scalarValue1));

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(mulKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(mulKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 1, sizeof(void *), &dstBuffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 2, sizeof(scalarValue2), &scalarValue2));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    // add 1st kernel optionally as immutable
    if (!firstImmutable) {
        SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, addKernel, &dispatchTraits, nullptr, 0, nullptr));

    // add 2nd kernel as mutable
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    uint64_t mulCommandId = commandId;

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, mulKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValueAdd = val1 + scalarValue1;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer1, elemSize) == false) {
            std::cerr << "before mutation - fail add operation" << std::endl;
            valid = false;
        }
    }

    ElemType expectedValueMul = val1 * scalarValue2;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueMul, dstBuffer2, elemSize) == false) {
            std::cerr << "before mutation - fail mul operation" << std::endl;
            valid = false;
        }
    }

    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    ze_mutable_kernel_argument_exp_desc_t mutateKernelArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelArg.commandId = mulCommandId;
    mutateKernelArg.argIndex = 0;
    mutateKernelArg.argSize = sizeof(void *);
    mutateKernelArg.pArgValue = &srcBuffer2;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer1, elemSize) == false) {
            std::cerr << "after mutation - fail add operation" << std::endl;
            valid = false;
        }
    }

    expectedValueMul = val2 * scalarValue2;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueMul, dstBuffer2, elemSize) == false) {
            std::cerr << "after mutation - fail mul operation" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(mulKernel);
    execEnv->destroyKernelHandle(addKernel);
    return valid;
}

std::string testNameMutateMemoryAndImmediateArgumentsSignalAndWaitEvents(MclTests::EventOptions eventOptions) {
    std::ostringstream testStream;

    testStream << "Mutate Kernel Arguments (memory and immediate), Signal Event and Wait Event case";
    testStream << "." << std::endl;
    MclTests::setEventTestStream(eventOptions, testStream);
    return testStream.str();
}

bool testMutateMemoryAndImmediateArgumentsSignalAndWaitEvents(MclTests::ExecEnv *execEnv, ze_module_handle_t module, MclTests::EventOptions eventOptions, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, MclTests::isCbEvent(eventOptions), false);

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 4;
    std::vector<ze_event_handle_t> events(numEvents);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");
    ze_kernel_handle_t mulKernel = execEnv->getKernelHandle(module, "mulScalarKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    void *stageBuffer1 = nullptr;
    void *stageBuffer2 = nullptr;
    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer2));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
    }
    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    ElemType scalarValueAdd1 = 0x1;
    ElemType scalarValueAdd2 = 0x2;
    ElemType scalarValueMul1 = 0x3;
    ElemType scalarValueMul2 = 0x4;

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 1, sizeof(void *), &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 2, sizeof(scalarValueAdd1), &scalarValueAdd1));

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(mulKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(mulKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 0, sizeof(void *), &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 1, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 2, sizeof(scalarValueMul1), &scalarValueMul1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = 0; // allow to mutatate all

    uint64_t commandIdAdd = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandIdAdd));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, addKernel, &dispatchTraits, events[0], 0, nullptr));

    uint64_t commandIdMul = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandIdMul));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, mulKernel, &dispatchTraits, events[1], 1, &events[0]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValueAdd = val1 + scalarValueAdd1;
    ElemType expectedValueMul = expectedValueAdd * scalarValueMul1;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueMul, dstBuffer1, elemSize) == false) {
            std::cerr << "before mutation" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[0]));
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[1]));
    }

    SUCCESS_OR_TERMINATE(zeEventHostReset(events[0]));
    SUCCESS_OR_TERMINATE(zeEventHostReset(events[1]));

    ze_mutable_kernel_argument_exp_desc_t mutateAddKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateAddKernelSrcArg.commandId = commandIdAdd;
    mutateAddKernelSrcArg.argIndex = 0;
    mutateAddKernelSrcArg.argSize = sizeof(void *);
    mutateAddKernelSrcArg.pArgValue = &srcBuffer2;

    ze_mutable_kernel_argument_exp_desc_t mutateAddKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateAddKernelDstArg.commandId = commandIdAdd;
    mutateAddKernelDstArg.argIndex = 1;
    mutateAddKernelDstArg.argSize = sizeof(void *);
    mutateAddKernelDstArg.pArgValue = &stageBuffer2;
    mutateAddKernelDstArg.pNext = &mutateAddKernelSrcArg;

    ze_mutable_kernel_argument_exp_desc_t mutateAddKernelScalarArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateAddKernelScalarArg.commandId = commandIdAdd;
    mutateAddKernelScalarArg.argIndex = 2;
    mutateAddKernelScalarArg.argSize = sizeof(scalarValueAdd2);
    mutateAddKernelScalarArg.pArgValue = &scalarValueAdd2;
    mutateAddKernelScalarArg.pNext = &mutateAddKernelDstArg;

    ze_mutable_kernel_argument_exp_desc_t mutateMulKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateMulKernelSrcArg.commandId = commandIdMul;
    mutateMulKernelSrcArg.argIndex = 0;
    mutateMulKernelSrcArg.argSize = sizeof(void *);
    mutateMulKernelSrcArg.pArgValue = &stageBuffer2;
    mutateMulKernelSrcArg.pNext = &mutateAddKernelScalarArg;

    ze_mutable_kernel_argument_exp_desc_t mutateMulKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateMulKernelDstArg.commandId = commandIdMul;
    mutateMulKernelDstArg.argIndex = 1;
    mutateMulKernelDstArg.argSize = sizeof(void *);
    mutateMulKernelDstArg.pArgValue = &dstBuffer2;
    mutateMulKernelDstArg.pNext = &mutateMulKernelSrcArg;

    ze_mutable_kernel_argument_exp_desc_t mutateMulKernelScalarArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateMulKernelScalarArg.commandId = commandIdMul;
    mutateMulKernelScalarArg.argIndex = 2;
    mutateMulKernelScalarArg.argSize = sizeof(scalarValueMul2);
    mutateMulKernelScalarArg.pArgValue = &scalarValueMul2;
    mutateMulKernelScalarArg.pNext = &mutateMulKernelDstArg;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateMulKernelScalarArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandIdAdd, events[2]));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandIdMul, events[3]));

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandIdMul, 1, &events[2]));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValueAdd = val2 + scalarValueAdd2;
    expectedValueMul = expectedValueAdd * scalarValueMul2;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueMul, dstBuffer2, elemSize) == false) {
            std::cerr << "after mutation" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[2]));
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[3]));
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(mulKernel);
    execEnv->destroyKernelHandle(addKernel);
    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);
    return valid;
}

std::string testNameMutateMixedSignalEventOnCommandList(bool useTimestamp, bool useFirstSignal, bool inOrder) {
    std::ostringstream testStream;
    testStream << "Append and mutate mixed signal scope Signal Events on ";
    testStream << (inOrder ? "In Order" : "Out of Order");
    testStream << " Command List " << std::endl;

    testStream << (useTimestamp ? "" : "No ");
    testStream << "Timestamp Event is used. ";
    testStream << "Signal scope event is " << (useFirstSignal ? "first" : "second") << " in sequence.";
    return testStream.str();
}

bool testMutateMixedSignalEventOnCommandList(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool useTimestamp, bool useFirstSignal, bool inOrder, bool aubMode) {
    constexpr size_t allocSize = 128;

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, inOrder, false);

    ze_event_pool_handle_t eventPoolNoSignalScope = nullptr, eventPoolSignalScope = nullptr;
    uint32_t numEvents = 4;
    std::vector<ze_event_handle_t> eventsNoSignalScope(numEvents), eventsSignalScope(numEvents);

    MclTests::EventOptions eventOptionsNoSignalScope = MclTests::EventOptions::noEvent;
    MclTests::EventOptions eventOptionsSignalScope = MclTests::EventOptions::noEvent;
    if (useTimestamp) {
        if (inOrder) {
            eventOptionsNoSignalScope = MclTests::EventOptions::cbEventTimestamp;
            eventOptionsSignalScope = MclTests::EventOptions::cbEventSignalTimestamp;
        } else {
            eventOptionsNoSignalScope = MclTests::EventOptions::timestamp;
            eventOptionsSignalScope = MclTests::EventOptions::signalEventTimestamp;
        }
    } else {
        if (inOrder) {
            eventOptionsNoSignalScope = MclTests::EventOptions::cbEvent;
            eventOptionsSignalScope = MclTests::EventOptions::cbEventSignal;
        } else {
            eventOptionsNoSignalScope = MclTests::EventOptions::none;
            eventOptionsSignalScope = MclTests::EventOptions::signalEvent;
        }
    }

    MclTests::createEventPoolAndEvents(*execEnv, eventPoolNoSignalScope, numEvents, eventsNoSignalScope.data(), eventOptionsNoSignalScope);
    MclTests::createEventPoolAndEvents(*execEnv, eventPoolSignalScope, numEvents, eventsSignalScope.data(), eventOptionsSignalScope);

    std::vector<ze_event_handle_t> &firstEvents = useFirstSignal ? eventsSignalScope : eventsNoSignalScope;
    std::vector<ze_event_handle_t> &secondEvents = useFirstSignal ? eventsNoSignalScope : eventsSignalScope;

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *stageBuffer1 = nullptr;
    void *stageBuffer2 = nullptr;
    void *stageBuffer3 = nullptr;
    void *dstBuffer1 = nullptr;

    void *zeroPatternBuffer = nullptr;
    size_t zeroPatternBufferSize = sizeof(uint32_t);

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, zeroPatternBufferSize, 1, &zeroPatternBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer3));

    constexpr uint8_t val1 = 0x10;

    memset(srcBuffer1, val1, allocSize);
    memset(dstBuffer1, 0, allocSize);
    memset(zeroPatternBuffer, 0, zeroPatternBufferSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT;
    uint32_t numWaitEvents = 0;

    ze_event_handle_t *waitEventHandle = nullptr;

    // out of order command list needs explicit wait events and mutation of those
    if (!inOrder) {
        commandIdDesc.flags |= ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;
        numWaitEvents = 1;
    }

    uint64_t commandIds[4] = {};

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandIds[0]));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, firstEvents[0], 0, nullptr));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandIds[1]));

    if (!inOrder) {
        waitEventHandle = &firstEvents[0];
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, secondEvents[0], numWaitEvents, waitEventHandle));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &stageBuffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer3));
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandIds[2]));

    if (!inOrder) {
        waitEventHandle = &secondEvents[0];
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, firstEvents[1], numWaitEvents, waitEventHandle));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &stageBuffer3));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandIds[3]));

    if (!inOrder) {
        waitEventHandle = &firstEvents[1];
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, secondEvents[1], numWaitEvents, waitEventHandle));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool validate = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: before mutation fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(firstEvents[0]));
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(secondEvents[0]));
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(firstEvents[1]));
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(secondEvents[1]));
        if (!inOrder) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(firstEvents[0]));
            SUCCESS_OR_TERMINATE(zeEventHostReset(secondEvents[0]));
            SUCCESS_OR_TERMINATE(zeEventHostReset(firstEvents[1]));
            SUCCESS_OR_TERMINATE(zeEventHostReset(secondEvents[1]));
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, dstBuffer1, zeroPatternBuffer, zeroPatternBufferSize, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, stageBuffer1, zeroPatternBuffer, zeroPatternBufferSize, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, stageBuffer2, zeroPatternBuffer, zeroPatternBufferSize, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, stageBuffer3, zeroPatternBuffer, zeroPatternBufferSize, allocSize, nullptr, 0, nullptr));

    // mutate events
    {
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandIds[0], firstEvents[2]));

        if (!inOrder) {
            SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandIds[1], 1, &firstEvents[2]));
        }
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandIds[1], secondEvents[2]));

        if (!inOrder) {
            SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandIds[2], 1, &secondEvents[2]));
        }
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandIds[2], firstEvents[3]));

        if (!inOrder) {
            SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandIds[3], 1, &firstEvents[3]));
        }
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandIds[3], secondEvents[3]));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            std::cerr << "dst1 <-> src1: after mutation fail" << std::endl;
            validate = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(firstEvents[2]));
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(secondEvents[2]));
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(firstEvents[3]));
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(secondEvents[3]));
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer3));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, zeroPatternBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    for (auto event : eventsNoSignalScope) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPoolNoSignalScope, eventOptionsNoSignalScope);
    for (auto event : eventsSignalScope) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPoolSignalScope, eventOptionsSignalScope);

    return validate;
}

bool testSimpleSlm(MclTests::ExecEnv *execEnv, bool aubMode) {
    const char *src = R"OpenCLC(
    __kernel void testSimpleSlm(__local int *lmemArg, __global int *out, uint localSize, __local int *lmemAuxArg) {
        __local int lmemInline[4];
        const int defalutValue = localSize < 4 ? 1 : 0;

        int lid = get_local_id(0);
        if (lid == 0) {
            lmemInline[0] = 0;
            lmemInline[1] = 1;
            lmemInline[2] = 2;
            lmemInline[3] = 3;

            for(uint i = 0; i < localSize; i++) {
                lmemArg[i] = i;
                lmemAuxArg[i] = defalutValue;
            }
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        uint localMemId = lid % localSize;
        out[lid] = lmemInline[lid % 4] + lmemArg[localMemId] + lmemAuxArg[localMemId];
    }

    )OpenCLC";

    ze_module_handle_t module;
    {
        std::string buildLog;
        auto spirV = LevelZeroBlackBoxTests::compileToSpirV(src, "-cl-std=CL2.0", buildLog);
        LevelZeroBlackBoxTests::printBuildLog(buildLog);
        SUCCESS_OR_TERMINATE((0 == spirV.size()));

        ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        ze_module_build_log_handle_t buildlog;
        moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = spirV.data();
        moduleDesc.inputSize = spirV.size();
        moduleDesc.pBuildFlags = MclTests::mclBuildOption.c_str();
        if (zeModuleCreate(execEnv->context, execEnv->device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
            size_t szLog = 0;
            zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

            char *strLog = (char *)malloc(szLog);
            zeModuleBuildLogGetString(buildlog, &szLog, strLog);
            LevelZeroBlackBoxTests::printBuildLog(strLog);

            free(strLog);
            SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
            std::cerr << "\nZello MCL Test Results validation FAILED. Module creation error."
                      << std::endl;
            SUCCESS_OR_TERMINATE_BOOL(false);
        }
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
    }

    using ElemType = uint32_t;
    constexpr size_t elemSize = 128;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_kernel_handle_t testKernel;
    {
        ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
        kernelDesc.pKernelName = "testSimpleSlm";
        SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &testKernel));
    }

    // prepare buffers
    void *dstBuffer1 = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));

    memset(dstBuffer1, 0, allocSize);

    uint32_t slmSize = 32;

    uint32_t groupSizeX = slmSize;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(testKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(testKernel, 0, slmSize * sizeof(ElemType), nullptr));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(testKernel, 1, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(testKernel, 2, sizeof(groupSizeX), &groupSizeX));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(testKernel, 3, slmSize * sizeof(ElemType), nullptr));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;

    uint64_t commandId = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, testKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType *values = static_cast<ElemType *>(dstBuffer1);
    if (!aubMode) {
        std::cout << "before mutation for slm size: " << slmSize;
        for (uint32_t i = 0; i < slmSize; i++) {
            if (i % 4 == 0) {
                std::cout << std::endl;
            }
            std::cout << "values[" << i << "] = " << values[i] << " ";
        }
    }

    slmSize = 64;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelSlmArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelSlmArg.commandId = commandId;
    mutateKernelSlmArg.argIndex = 0;
    mutateKernelSlmArg.argSize = slmSize * sizeof(ElemType);
    mutateKernelSlmArg.pArgValue = nullptr;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelSlmAuxArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelSlmAuxArg.commandId = commandId;
    mutateKernelSlmAuxArg.argIndex = 3;
    mutateKernelSlmAuxArg.argSize = slmSize * sizeof(ElemType);
    mutateKernelSlmAuxArg.pArgValue = nullptr;
    mutateKernelSlmAuxArg.pNext = &mutateKernelSlmArg;

    ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    mutateGroupSize.commandId = commandId;
    mutateGroupSize.groupSizeX = slmSize;
    mutateGroupSize.groupSizeY = 1;
    mutateGroupSize.groupSizeZ = 1;
    mutateGroupSize.pNext = &mutateKernelSlmAuxArg;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateGroupSize;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        std::cout << std::endl
                  << "after mutation for slm size: " << slmSize;
        for (uint32_t i = 0; i < slmSize; i++) {
            if (i % 4 == 0) {
                std::cout << std::endl;
            }
            std::cout << "values[" << i << "] = " << values[i] << " ";
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(testKernel);
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    bool valid = true;

    return valid;
}

bool testMutateKernelArgSharedMemoryAndEventSync(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 1;
    std::vector<ze_event_handle_t> events(numEvents);

    MclTests::EventOptions eventOptions = MclTests::EventOptions::signalEvent;
    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    ze_kernel_handle_t addScalarKernel = execEnv->getKernelHandle(module, "addScalarKernel");

    // prepare buffers
    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocShared(execEnv->context, &deviceDesc, &hostDesc, allocSize, 1, execEnv->device, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocShared(execEnv->context, &deviceDesc, &hostDesc, allocSize, 1, execEnv->device, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocShared(execEnv->context, &deviceDesc, &hostDesc, allocSize, 1, execEnv->device, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocShared(execEnv->context, &deviceDesc, &hostDesc, allocSize, 1, execEnv->device, &dstBuffer2));

    ElemType val1 = 5;
    ElemType val2 = 8;

    for (uint32_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
    }

    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    ElemType scalar = 4;

    uint32_t groupSizeX = elemSize;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addScalarKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addScalarKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addScalarKernel, 1, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addScalarKernel, 2, sizeof(ElemType), &scalar));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;

    uint64_t commandIdAdd = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandIdAdd));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, addScalarKernel, &dispatchTraits, events[0], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[0], std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    ElemType expectedValueAdd = val1 + scalar;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer1, elemSize) == false) {
            std::cerr << "before mutation" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[0]));
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelSrcArg.commandId = commandIdAdd;
    mutateKernelSrcArg.argIndex = 0;
    mutateKernelSrcArg.argSize = sizeof(void *);
    mutateKernelSrcArg.pArgValue = &srcBuffer2;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelDstArg.commandId = commandIdAdd;
    mutateKernelDstArg.argIndex = 1;
    mutateKernelDstArg.argSize = sizeof(void *);
    mutateKernelDstArg.pArgValue = &dstBuffer2;
    mutateKernelDstArg.pNext = &mutateKernelSrcArg;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelDstArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[0], std::numeric_limits<uint64_t>::max()));

    expectedValueAdd = val2 + scalar;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer2, elemSize) == false) {
            std::cerr << "after mutation" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(addScalarKernel);
    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    return valid;
}

bool testImmediateCmdListExecuteMcl(MclTests::ExecEnv *execEnv, bool aubMode, uint32_t appendCount) {
    const char *atomicIncSrc = LevelZeroBlackBoxTests::atomicIncSrc;

    ze_module_handle_t module;
    {
        std::string buildLog;
        auto spirV = LevelZeroBlackBoxTests::compileToSpirV(atomicIncSrc, "-cl-std=CL2.0", buildLog);
        LevelZeroBlackBoxTests::printBuildLog(buildLog);
        SUCCESS_OR_TERMINATE((0 == spirV.size()));

        ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        ze_module_build_log_handle_t buildlog;
        moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = spirV.data();
        moduleDesc.inputSize = spirV.size();
        moduleDesc.pBuildFlags = MclTests::mclBuildOption.c_str();
        if (zeModuleCreate(execEnv->context, execEnv->device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
            size_t szLog = 0;
            zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

            char *strLog = (char *)malloc(szLog);
            zeModuleBuildLogGetString(buildlog, &szLog, strLog);
            LevelZeroBlackBoxTests::printBuildLog(strLog);

            free(strLog);
            SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
            std::cerr << "\nZello MCL Test Results validation FAILED. Module creation error."
                      << std::endl;
            SUCCESS_OR_TERMINATE_BOOL(false);
        }
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
    }

    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_kernel_handle_t testKernel;
    {
        ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
        kernelDesc.pKernelName = "testKernel";
        SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &testKernel));
    }

    // prepare buffers
    void *dstBuffer1 = nullptr;
    void *outBuffer1 = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &dstBuffer1));
    ElemType zero = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, dstBuffer1, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &outBuffer1));
    memset(outBuffer1, 0, allocSize);

    uint32_t groupSizeX = 1u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(testKernel, groupSizeX, groupSizeY, groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(testKernel, 0, sizeof(void *), &dstBuffer1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ElemType appendCountT = appendCount;

    for (ElemType i = 0; i < appendCountT; i++) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, testKernel, &dispatchTraits, nullptr, 0, nullptr));
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, outBuffer1, dstBuffer1, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandListImmediateAppendCommandListsExp(execEnv->immCmdList, 1, &cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(execEnv->immCmdList, std::numeric_limits<uint64_t>::max()));

    bool valid = true;

    ElemType *values = static_cast<ElemType *>(outBuffer1);
    if (!aubMode) {
        if (*values != appendCountT) {
            std::cerr << "expected " << appendCountT << " do not match received " << *values << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, outBuffer1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(testKernel);
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    return valid;
}

bool testProfileAppendOperationsUsingTsEvents(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 128;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 3;
    std::vector<ze_event_handle_t> events(numEvents);
    std::vector<ze_kernel_timestamp_result_t> timestamps(numEvents * 2);

    MclTests::EventOptions eventOptions = MclTests::EventOptions::timestamp;
    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    ze_kernel_handle_t mulScalarKernel = execEnv->getKernelHandle(module, "mulScalarKernel");

    // prepare buffers
    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer2));

    ElemType val1 = 5;
    ElemType val2 = 8;

    for (uint32_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
    }

    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    ElemType scalar = 4;

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, events[0], 0, nullptr));

    uint32_t groupSizeX = 32;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(mulScalarKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(mulScalarKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 1, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 2, sizeof(ElemType), &scalar));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS;

    uint64_t commandIdAdd = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandIdAdd));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, mulScalarKernel, &dispatchTraits, events[1], 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, events[2], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    ElemType expectedValueMul = val1 * scalar;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueMul, dstBuffer1, elemSize) == false) {
            std::cerr << "before mutation" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[0], &timestamps[0]));
        SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[1], &timestamps[1]));
        SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[2], &timestamps[2]));
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[0]));
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[1]));
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[2]));
    }

    ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelSrcArg.commandId = commandIdAdd;
    mutateKernelSrcArg.argIndex = 0;
    mutateKernelSrcArg.argSize = sizeof(void *);
    mutateKernelSrcArg.pArgValue = &srcBuffer2;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelDstArg.commandId = commandIdAdd;
    mutateKernelDstArg.argIndex = 1;
    mutateKernelDstArg.argSize = sizeof(void *);
    mutateKernelDstArg.pArgValue = &dstBuffer2;
    mutateKernelDstArg.pNext = &mutateKernelSrcArg;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelDstArg;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[0], std::numeric_limits<uint64_t>::max()));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValueMul = val2 * scalar;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueMul, dstBuffer2, elemSize) == false) {
            std::cerr << "after mutation" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[0], &timestamps[3]));
        SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[1], &timestamps[4]));
        SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[2], &timestamps[5]));

        std::vector<bool> useGlobalValues = {false, true};
        for (auto useGlobal : useGlobalValues) {
            std::string tsType = useGlobal ? "global " : "context ";

            uint64_t precedingTs = useGlobal ? timestamps[4].global.kernelEnd : timestamps[4].context.kernelEnd;
            uint64_t succeedingTs = useGlobal ? timestamps[5].global.kernelEnd : timestamps[5].context.kernelEnd;
            if (succeedingTs < precedingTs) {
                std::cerr << tsType << "timestamps[5] smaller than timestamps[4]" << std::endl;
                std::cerr << succeedingTs << " < " << precedingTs << std::endl;
                valid = false;
            }

            precedingTs = useGlobal ? timestamps[3].global.kernelEnd : timestamps[3].context.kernelEnd;
            succeedingTs = useGlobal ? timestamps[4].global.kernelEnd : timestamps[4].context.kernelEnd;
            if (succeedingTs < precedingTs) {
                std::cerr << tsType << "timestamps[4] smaller than timestamps[3]" << std::endl;
                std::cerr << succeedingTs << " < " << precedingTs << std::endl;
                valid = false;
            }

            precedingTs = useGlobal ? timestamps[2].global.kernelEnd : timestamps[2].context.kernelEnd;
            succeedingTs = useGlobal ? timestamps[3].global.kernelEnd : timestamps[3].context.kernelEnd;
            if (succeedingTs < precedingTs) {
                std::cerr << tsType << "timestamps[3] smaller than timestamps[2]" << std::endl;
                std::cerr << succeedingTs << " < " << precedingTs << std::endl;
                valid = false;
            }

            precedingTs = useGlobal ? timestamps[1].global.kernelEnd : timestamps[1].context.kernelEnd;
            succeedingTs = useGlobal ? timestamps[2].global.kernelEnd : timestamps[2].context.kernelEnd;
            if (succeedingTs < precedingTs) {
                std::cerr << tsType << "timestamps[2] smaller than timestamps[1]" << std::endl;
                std::cerr << succeedingTs << " < " << precedingTs << std::endl;
                valid = false;
            }

            precedingTs = useGlobal ? timestamps[0].global.kernelEnd : timestamps[0].context.kernelEnd;
            succeedingTs = useGlobal ? timestamps[1].global.kernelEnd : timestamps[1].context.kernelEnd;
            if (succeedingTs < precedingTs) {
                std::cerr << tsType << "timestamps[1] smaller than timestamps[0]" << std::endl;
                std::cerr << succeedingTs << " < " << precedingTs << std::endl;
                valid = false;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(mulScalarKernel);
    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    return valid;
}

std::string testNameMutateSlmArgumentKernel(uint32_t firstGroupSize, uint32_t mutateGroupSize, uint32_t groupCount, bool kernelOneArg) {
    std::ostringstream testStream;

    std::string kernelName = kernelOneArg ? "slmKernelOneArg" : "slmKernelTwoArgs";

    testStream << "Mutate Slm Argument Kernel using kernel: " << kernelName << std::endl;

    testStream << "First Group Size: " << firstGroupSize << std::endl;
    testStream << "Mutate Group Size: " << mutateGroupSize << std::endl;
    testStream << "Group Count: " << groupCount;
    return testStream.str();
}

bool testMutateSlmArgumentKernel(MclTests::ExecEnv *execEnv, ze_module_handle_t slmModule, uint32_t firstGroupSize, uint32_t mutateGroupSize, uint32_t groupCount, bool kernelOneArg, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 128;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    std::string kernelName = kernelOneArg ? "slmKernelOneArg" : "slmKernelTwoArgs";
    ze_kernel_handle_t slmKernel = execEnv->getKernelHandle(slmModule, kernelName);

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));

    uint32_t groupSizeX = firstGroupSize;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = groupCount;
    dispatchTraits.groupCountY = 1;
    dispatchTraits.groupCountZ = 1;

    uint32_t dataSize = firstGroupSize * groupCount;

    std::vector<ElemType> verificationData;
    if (kernelOneArg) {
        MclTests::prepareOutputSlmKernelOneArg(verificationData, firstGroupSize, groupCount);
    } else {
        MclTests::prepareOutputSlmKernelTwoArgs(verificationData, firstGroupSize, groupCount);
    }

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(slmKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(slmKernel, 0, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(slmKernel, 1, firstGroupSize * sizeof(ElemType), nullptr));
    if (!kernelOneArg) {
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(slmKernel, 2, firstGroupSize * sizeof(ElemType), nullptr));
    }

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 0, nullptr, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, slmKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(verificationData.data(), dstBuffer, dataSize * sizeof(ElemType)) == false) {
            std::cerr << "before mutation - slm kernel" << std::endl;
            valid = false;
        }
    }

    ElemType zero = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, dstBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));

    dataSize = mutateGroupSize * groupCount;
    if (kernelOneArg) {
        MclTests::prepareOutputSlmKernelOneArg(verificationData, mutateGroupSize, groupCount);
    } else {
        MclTests::prepareOutputSlmKernelTwoArgs(verificationData, mutateGroupSize, groupCount);
    }

    ze_mutable_kernel_argument_exp_desc_t mutateKernelSlmReverseArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    if (!kernelOneArg) {
        mutateKernelSlmReverseArg.commandId = commandId;
        mutateKernelSlmReverseArg.argIndex = 2;
        mutateKernelSlmReverseArg.argSize = mutateGroupSize * sizeof(ElemType);
        mutateKernelSlmReverseArg.pArgValue = nullptr;
    }

    ze_mutable_kernel_argument_exp_desc_t mutateKernelSlmArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelSlmArg.commandId = commandId;
    mutateKernelSlmArg.argIndex = 1;
    mutateKernelSlmArg.argSize = mutateGroupSize * sizeof(ElemType);
    mutateKernelSlmArg.pArgValue = nullptr;
    if (!kernelOneArg) {
        mutateKernelSlmArg.pNext = &mutateKernelSlmReverseArg;
    }

    ze_mutable_group_size_exp_desc_t mutateGroupSizeDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    mutateGroupSizeDesc.commandId = commandId;
    mutateGroupSizeDesc.groupSizeX = mutateGroupSize;
    mutateGroupSizeDesc.groupSizeY = 1;
    mutateGroupSizeDesc.groupSizeZ = 1;
    mutateGroupSizeDesc.pNext = &mutateKernelSlmArg;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateGroupSizeDesc;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(verificationData.data(), dstBuffer, dataSize * sizeof(ElemType)) == false) {
            std::cerr << "after mutation - slm kernel" << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(slmKernel);

    return valid;
}

bool testSubmitGraphOnImmediate(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool immediateSubmit, bool copyOffload, uint32_t iteration, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 128;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    MclTests::EventOptions eventOptions = MclTests::EventOptions::cbEventSignal;
    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 1;
    std::vector<ze_event_handle_t> events(numEvents);
    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);
    ze_event_handle_t event = events[0];

    ze_command_queue_flags_t immediateFlags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    immediateFlags |= copyOffload ? ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT : 0;

    ze_command_list_handle_t submitCmdList = nullptr;
    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(execEnv->context, execEnv->device,
                                                           immediateFlags,
                                                           false,
                                                           false,
                                                           submitCmdList);

    constexpr bool inOrder = true;
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, inOrder, false);

    ze_kernel_handle_t mulScalarKernel = execEnv->getKernelHandle(module, "mulScalarKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;
    void *dstBuffer3 = nullptr;
    void *dstBuffer4 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer3));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer4));

    ElemType src1 = 5;
    ElemType mul1 = 8;
    ElemType mul2 = 2;
    ElemType mul3 = 7;

    for (uint32_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = src1;
    }

    uint32_t groupSizeX = 32;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(mulScalarKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(mulScalarKernel, groupSizeX, groupSizeY, groupSizeZ));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    // node 1
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 0, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 1, sizeof(void *), &dstBuffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 2, sizeof(ElemType), &mul1));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, mulScalarKernel, &dispatchTraits, nullptr, 0, nullptr));

    // node 2
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 0, sizeof(void *), &dstBuffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 1, sizeof(void *), &dstBuffer3));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 2, sizeof(ElemType), &mul2));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, mulScalarKernel, &dispatchTraits, nullptr, 0, nullptr));

    // node 3
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 0, sizeof(void *), &dstBuffer3));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 1, sizeof(void *), &dstBuffer4));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 2, sizeof(ElemType), &mul3));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, mulScalarKernel, &dispatchTraits, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    ElemType expectedValueMul = ((src1 * mul1) * mul2) * mul3;
    bool valid = true;

    for (uint32_t i = 0; i < iteration; i++) {
        // execution - copy using dispatch immediate
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(submitCmdList, dstBuffer1, srcBuffer1, allocSize, nullptr, 0, nullptr));

        if (immediateSubmit) {
            // in order command list - no need to synchronize between appends
            SUCCESS_OR_TERMINATE(zeCommandListImmediateAppendCommandListsExp(submitCmdList, 1, &cmdList, event, 0, nullptr));
            // synchronization after workload is done
            SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
        } else {
            // no implicit serialization between in order command list and queue - need to synchronize
            SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(submitCmdList, std::numeric_limits<uint64_t>::max()));
            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
            // synchronization after workload is done
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
        }

        if (!aubMode) {
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueMul, dstBuffer4, elemSize) == false) {
                std::cerr << "invalid output at iteration " << i << std::endl;
                valid &= false;
            }
        }

        memset(dstBuffer4, 0, allocSize);
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer3));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer4));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(submitCmdList));
    execEnv->destroyKernelHandle(mulScalarKernel);

    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    return valid;
}

bool testSubmitCopyCmdListOnCopyImmediateCmdList(MclTests::ExecEnv *execEnv, uint32_t copyOrdinal, bool useImmediate, uint32_t iteration, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    MclTests::EventOptions eventOptions = MclTests::EventOptions::cbEventSignal;
    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 1;
    std::vector<ze_event_handle_t> events(numEvents);
    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);
    ze_event_handle_t event = events[0];

    ze_command_list_handle_t cmdListCopy = nullptr;
    ze_command_list_handle_t submitCmdList = nullptr;
    ze_command_queue_handle_t submitQueue = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = copyOrdinal;
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    if (useImmediate) {
        LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(execEnv->context, execEnv->device,
                                                               ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
                                                               copyOrdinal,
                                                               false,
                                                               submitCmdList);
    } else {
        SUCCESS_OR_TERMINATE(zeCommandQueueCreate(execEnv->context, execEnv->device, &cmdQueueDesc, &submitQueue));
    }

    ze_command_list_desc_t descriptor = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    descriptor.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
    descriptor.commandQueueGroupOrdinal = copyOrdinal;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(execEnv->context, execEnv->device, &descriptor, &cmdListCopy));

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;
    void *dstBuffer3 = nullptr;
    void *dstBuffer4 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer3));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer4));

    ElemType src1 = 6;

    for (uint32_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = src1;
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListCopy, dstBuffer1, srcBuffer1, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListCopy, dstBuffer2, dstBuffer1, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListCopy, dstBuffer3, dstBuffer2, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListCopy, dstBuffer4, dstBuffer3, allocSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListCopy));

    bool valid = true;

    ElemType expectedValue = src1;

    for (uint32_t i = 0; i < iteration; i++) {
        if (useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandListImmediateAppendCommandListsExp(submitCmdList, 1, &cmdListCopy, event, 0, nullptr));
            SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
        } else {
            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(submitQueue, 1, &cmdListCopy, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(submitQueue, std::numeric_limits<uint64_t>::max()));
        }

        if (!aubMode) {
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer4, elemSize) == false) {
                std::cerr << "copy fail at iteration " << i << std::endl;
                valid = false;
            }
        }
        memset(dstBuffer4, 0, allocSize);
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer3));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer4));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListCopy));
    if (submitCmdList != nullptr) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(submitCmdList));
    }
    if (submitQueue != nullptr) {
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(submitQueue));
    }

    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    return valid;
}

bool testAppendRegularOnOutOfOrderImmediate(MclTests::ExecEnv *execEnv, ze_module_handle_t module, uint32_t iterations, bool useEvents, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 128;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    MclTests::EventOptions eventOptions = MclTests::EventOptions::none;
    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = iterations * 2;

    std::vector<ze_event_handle_t> events(numEvents);
    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    ze_command_list_handle_t submitCmdList = nullptr;

    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(execEnv->context, execEnv->device,
                                                           submitCmdList);

    constexpr bool inOrder = false;
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, inOrder, false);

    ze_kernel_handle_t mulScalarKernel = execEnv->getKernelHandle(module, "mulScalarKernel");

    // prepare buffers
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;
    void *deviceBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &deviceBuffer));

    ElemType src1 = 5;
    ElemType mul1 = 8;

    for (uint32_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer)[i] = src1;
    }

    ElemType expectedValueMul = src1 * mul1;

    uint32_t groupSizeX = 32;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(mulScalarKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(mulScalarKernel, groupSizeX, groupSizeY, groupSizeZ));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 0, sizeof(void *), &deviceBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 1, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulScalarKernel, 2, sizeof(ElemType), &mul1));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, mulScalarKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;

    uint8_t zero = 0;

    for (uint32_t i = 0; i < iterations; i++) {
        // execution - copy using dispatch immediate - signal event
        auto eventIdx = 2 * i;
        ze_event_handle_t copyEvent = events[eventIdx];
        ze_event_handle_t execSyncEvent = nullptr;
        // when event is used for host synchronization instead of command list synchronization
        if (useEvents) {
            execSyncEvent = events[eventIdx + 1];
        }
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(submitCmdList, deviceBuffer, srcBuffer, allocSize, copyEvent, 0, nullptr));
        // execution regular - wait for event
        SUCCESS_OR_TERMINATE(zeCommandListImmediateAppendCommandListsExp(submitCmdList, 1, &cmdList, execSyncEvent, 1, &copyEvent));
        if (useEvents) {
            SUCCESS_OR_TERMINATE(zeEventHostSynchronize(execSyncEvent, std::numeric_limits<uint64_t>::max()));
        } else {
            SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(submitCmdList, std::numeric_limits<uint64_t>::max()));
        }

        if (!aubMode) {
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueMul, dstBuffer, elemSize) == false) {
                std::cerr << "invalid output at iteration " << (i + 1) << std::endl;
                valid &= false;
            }
        }

        if (i < (iterations - 1)) {
            // reset buffers
            SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, deviceBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, dstBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, deviceBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(submitCmdList));
    execEnv->destroyKernelHandle(mulScalarKernel);

    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    return valid;
}

bool testAppendMultipleRegularOnInOrderImmediate(MclTests::ExecEnv *execEnv, ze_module_handle_t module, uint32_t regularCount, uint32_t iterations, bool inOrderImmediate, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 128;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    MclTests::EventOptions eventOptions = inOrderImmediate ? MclTests::EventOptions::cbEventSignal : MclTests::EventOptions::signalEvent;
    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_handle_t event = nullptr;

    uint32_t numEvents = 1;
    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, &event, eventOptions);

    ze_command_queue_flags_t flags = 0;
    if (inOrderImmediate) {
        flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    }

    std::vector<ze_command_list_handle_t> submitCmdLists(iterations);
    for (uint32_t i = 0; i < iterations; i++) {
        LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(execEnv->context, execEnv->device,
                                                               flags,
                                                               false,
                                                               false,
                                                               submitCmdLists[i]);
    }

    constexpr bool inOrder = false;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ElemType src1 = 5;

    std::vector<ze_command_list_handle_t> cmdLists(regularCount);
    std::vector<void *> srcBuffers(regularCount);
    std::vector<void *> dstBuffers(regularCount);

    for (uint32_t i = 0; i < regularCount; i++) {
        MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdLists[i], inOrder, false);

        SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffers[i]));
        SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffers[i]));

        void *srcBuffer = srcBuffers[i];
        for (uint32_t el = 0; el < elemSize; el++) {
            reinterpret_cast<ElemType *>(srcBuffer)[el] = src1;
        }
        void *dstBuffer = dstBuffers[i];

        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdLists[i], dstBuffer, srcBuffer, allocSize, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdLists[i], nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdLists[i]));
    }

    bool valid = true;
    for (uint32_t i = 0; i < iterations; i++) {
        uint32_t currentCmdListCount = std::min(i + 1, regularCount);

        SUCCESS_OR_TERMINATE(zeCommandListImmediateAppendCommandListsExp(submitCmdLists[i], currentCmdListCount, cmdLists.data(), event, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));

        if (!aubMode) {
            ElemType expectedValue = src1;

            for (uint32_t cmd = 0; cmd < currentCmdListCount; cmd++) {
                void *dstBuffer = dstBuffers[cmd];
                if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer, elemSize) == false) {
                    std::cerr << "invalid output at iteration " << (i + 1) << " for command list " << cmd << std::endl;
                    valid &= false;
                }
            }
        }

        if (eventOptions == MclTests::EventOptions::signalEvent) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(event));
        }
    }

    for (uint32_t i = 0; i < regularCount; i++) {
        SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffers[i]));
        SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffers[i]));
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdLists[i]));
    }
    for (uint32_t i = 0; i < iterations; i++) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(submitCmdLists[i]));
    }

    SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    MclTests::destroyEventPool(eventPool, eventOptions);

    return valid;
}

int main(int argc, char *argv[]) {
    constexpr uint32_t bitNumberTestKernelArgument = 0u;
    constexpr uint32_t bitNumberTestSignalEvent = 1u;
    constexpr uint32_t bitNumberTestWaitEvents = 2u;
    constexpr uint32_t bitNumberTestMutateKernelArgumentsAndEvents = 3u;
    constexpr uint32_t bitNumberTestSignalCbEvent = 4u;
    constexpr uint32_t bitNumberTestWaitCbEvent = 5u;
    constexpr uint32_t bitNumberTestMixedWaitEvent = 6u;
    constexpr uint32_t bitNumberTestCbWaitEventExternalMemory = 7u;
    constexpr uint32_t bitNumberTestGroupCount = 8u;
    constexpr uint32_t bitNumberTestGroupSize = 9u;
    constexpr uint32_t bitNumberTestGlobalOffset = 10u;
    constexpr uint32_t bitNumberTestMixedTests = 11u;
    constexpr uint32_t bitNumberTestMultiExecutionMutatedCbEvents = 12u;
    constexpr uint32_t bitNumberTestComplexStructurePassedAsImmediate = 13u;
    constexpr uint32_t bitNumberTestKernelSlmArgument = 14u;
    constexpr uint32_t bitNumberTestCbSignalIncrementEvent = 15u;
    constexpr uint32_t bitNumberTestExperimental = 31u;

    const std::string blackBoxName = "Zello MCL Command ID";
    constexpr uint32_t excludedTestsFromDefault = (1u << bitNumberTestExperimental) | (1u << bitNumberTestCbSignalIncrementEvent);
    constexpr uint32_t defaultTestMask = std::numeric_limits<uint32_t>::max() & ~excludedTestsFromDefault;
    LevelZeroBlackBoxTests::TestBitMask testMask = LevelZeroBlackBoxTests::getTestMask(argc, argv, defaultTestMask);
    LevelZeroBlackBoxTests::TestBitMask testSubMask = LevelZeroBlackBoxTests::getTestSubmask(argc, argv, std::numeric_limits<uint32_t>::max());
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    bool limitedValuesSet = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-l", "--limited");
    LevelZeroBlackBoxTests::getErrorMax(argc, argv);
    TestValues valuesForTests(!limitedValuesSet);

    auto env = std::unique_ptr<MclTests::ExecEnv>(MclTests::ExecEnv::create());
    env->setGlobalKernels(argc, argv);

    ze_module_handle_t module;
    env->buildMainModule(module);

    bool valid = true;
    bool caseResult = true;
    std::string caseName = "";

    if (testMask.test(bitNumberTestKernelArgument)) {
        if (testSubMask.test(0)) {
            ze_command_list_handle_t sharedCmdList;

            caseName = "Mutate Memory Kernel Argument Test";
            caseResult = testMutateMemoryArgument(env.get(), module, sharedCmdList, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestKernelArgument);

            caseName = "Reset Command List and Mutate Scalar Kernel Argument Test";
            caseResult = testResetCmdlistAndMutateScalarArgument(env.get(), module, sharedCmdList, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestKernelArgument);
        }

        if (testSubMask.test(1)) {
            caseName = "Mutate Multiple Memory Kernel Arguments - First Append Immutable Test";
            caseResult = testMutateMultipleMemoryArguments(env.get(), module, true, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestKernelArgument);

            caseName = "Mutate Multiple Memory Kernel Arguments - First Append Mutable Test";
            caseResult = testMutateMultipleMemoryArguments(env.get(), module, false, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestKernelArgument);
        }

        if (testSubMask.test(2)) {
            caseName = "Mutate Offset Memory Kernel Argument";
            caseResult = testOffsetMemoryArgument(env.get(), module, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestKernelArgument);
        }

        if (testSubMask.test(3)) {
            caseName = "Mutate Freed Memory Kernel Argument";
            caseResult = testMutateAfterFreedMemoryArgument(env.get(), module, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestKernelArgument);
        }
    }

    if (testMask.test(bitNumberTestSignalEvent)) {
        std::vector<bool> &mutateFirstValues = valuesForTests.getMutateFirst();
        std::vector<bool> &mutateSecondValues = valuesForTests.getMutateSecond();
        std::vector<bool> &useDifferentPoolFirstKernelValues = valuesForTests.getDifferentPoolFirstKernel();
        std::vector<bool> &useDifferentPoolSecondKernelValues = valuesForTests.getDifferentPoolSecondKernel();
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::none,
                                                                 MclTests::EventOptions::timestamp,
                                                                 MclTests::EventOptions::signalEvent,
                                                                 MclTests::EventOptions::signalEventTimestamp,
                                                                 MclTests::EventOptions::cbEvent,
                                                                 MclTests::EventOptions::cbEventTimestamp};
        if (testSubMask.test(0)) {
            for (auto mutateFirst : mutateFirstValues) {
                for (auto mutateSecond : mutateSecondValues) {
                    if (!mutateFirst && !mutateSecond) {
                        continue;
                    }
                    for (auto useDifferentPoolFirstKernel : useDifferentPoolFirstKernelValues) {
                        for (auto useDifferentPoolSecondKernel : useDifferentPoolSecondKernelValues) {
                            if (!mutateFirst && useDifferentPoolFirstKernel) {
                                continue;
                            }
                            if (!mutateSecond && useDifferentPoolSecondKernel) {
                                continue;
                            }
                            for (auto eventOption : eventOptionValues) {
                                caseName = testNameMutateSignalEvent(mutateFirst, mutateSecond,
                                                                     useDifferentPoolFirstKernel, useDifferentPoolSecondKernel,
                                                                     eventOption);
                                caseResult = testMutateSignalEvent(env.get(), module,
                                                                   mutateFirst, mutateSecond,
                                                                   useDifferentPoolFirstKernel, useDifferentPoolSecondKernel,
                                                                   eventOption,
                                                                   aubMode);
                                LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                                valid &= caseResult;
                                MclTests::addFailedCase(caseResult, caseName, bitNumberTestSignalEvent);
                            }
                        }
                    }
                }
            }
        }

        if (testSubMask.test(1)) {
            for (auto eventOption : eventOptionValues) {
                caseName = testNameMutateSignalEventMutateOriginalBack(eventOption);
                caseResult = testMutateSignalEventMutateOriginalBack(env.get(), module,
                                                                     eventOption,
                                                                     aubMode);
                LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                valid &= caseResult;
                MclTests::addFailedCase(caseResult, caseName, bitNumberTestSignalEvent);
            }
        }

        if (testSubMask.test(2)) {
            std::vector<bool> useTimestampValues = {false, true};
            std::vector<bool> useFirstSignalValues = {false, true};
            std::vector<bool> useInOrderValues = {false, true};
            for (auto useInOrder : useInOrderValues) {
                for (auto useTimestamp : useTimestampValues) {
                    for (auto useFirstSignal : useFirstSignalValues) {
                        caseName = testNameMutateMixedSignalEventOnCommandList(useTimestamp, useFirstSignal, useInOrder);
                        caseResult = testMutateMixedSignalEventOnCommandList(env.get(), module, useTimestamp, useFirstSignal, useInOrder, aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberTestSignalEvent);
                    }
                }
            }
        }
    }

    if (testMask.test(bitNumberTestWaitEvents)) {
        std::vector<bool> &mutateFirstValues = valuesForTests.getMutateFirst();
        std::vector<bool> &mutateSecondValues = valuesForTests.getMutateSecond();
        std::vector<bool> &useDifferentPoolFirstKernelValues = valuesForTests.getDifferentPoolFirstKernel();
        std::vector<bool> &useDifferentPoolSecondKernelValues = valuesForTests.getDifferentPoolSecondKernel();
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::none,
                                                                 MclTests::EventOptions::timestamp,
                                                                 MclTests::EventOptions::signalEvent,
                                                                 MclTests::EventOptions::signalEventTimestamp};
        std::vector<uint32_t> &eventMaskValues = valuesForTests.getWaitEventMask();
        if (testSubMask.test(0)) {
            for (auto mutateFirst : mutateFirstValues) {
                for (auto mutateSecond : mutateSecondValues) {
                    if (!mutateFirst && !mutateSecond) {
                        continue;
                    }
                    for (auto useDifferentPoolFirstKernel : useDifferentPoolFirstKernelValues) {
                        for (auto useDifferentPoolSecondKernel : useDifferentPoolSecondKernelValues) {
                            if (!mutateFirst && useDifferentPoolFirstKernel) {
                                continue;
                            }
                            if (!mutateSecond && useDifferentPoolSecondKernel) {
                                continue;
                            }
                            for (auto eventOption : eventOptionValues) {
                                for (auto eventMask : eventMaskValues) {
                                    WaitEventMask bitMask(eventMask);
                                    caseName = testNameMutateWaitEvent(mutateFirst, mutateSecond,
                                                                       useDifferentPoolFirstKernel, useDifferentPoolSecondKernel,
                                                                       eventOption, bitMask);
                                    caseResult = testMutateWaitEvent(env.get(), module, mutateFirst, mutateSecond,
                                                                     useDifferentPoolFirstKernel, useDifferentPoolSecondKernel,
                                                                     eventOption, bitMask,
                                                                     aubMode);
                                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                                    valid &= caseResult;
                                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestWaitEvents);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (testSubMask.test(1)) {
            for (auto eventOption : eventOptionValues) {
                for (auto eventMask : eventMaskValues) {
                    WaitEventMask bitMask(eventMask);
                    caseName = testNameMutateNoopWaitEvent(eventOption, bitMask);
                    caseResult = testMutateNoopWaitEvent(env.get(), module,
                                                         eventOption, bitMask,
                                                         aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestWaitEvents);
                    valid &= caseResult;
                }
            }
        }
    }

    if (testMask.test(bitNumberTestMutateKernelArgumentsAndEvents)) {
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::none,
                                                                 MclTests::EventOptions::timestamp,
                                                                 MclTests::EventOptions::signalEvent,
                                                                 MclTests::EventOptions::signalEventTimestamp};
        if (testSubMask.test(0)) {
            for (auto eventOption : eventOptionValues) {
                caseName = testNameMutateMemoryAndImmediateArgumentsSignalAndWaitEvents(eventOption);
                caseResult = testMutateMemoryAndImmediateArgumentsSignalAndWaitEvents(env.get(), module, eventOption, aubMode);
                LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                MclTests::addFailedCase(caseResult, caseName, bitNumberTestMutateKernelArgumentsAndEvents);
                valid &= caseResult;
            }
        }

        std::vector<bool> useAnotherPoolValues = {true, false};
        std::vector<MclTests::EventOptions> eventOptionCbEvents = {MclTests::EventOptions::cbEvent,
                                                                   MclTests::EventOptions::cbEventTimestamp,
                                                                   MclTests::EventOptions::cbEventSignal,
                                                                   MclTests::EventOptions::cbEventSignalTimestamp};
        eventOptionValues.insert(eventOptionValues.end(), eventOptionCbEvents.begin(), eventOptionCbEvents.end());

        if (testSubMask.test(1)) {
            for (auto eventOption : eventOptionValues) {
                for (auto useAnotherPool : useAnotherPoolValues) {
                    caseName = testNameMutateSignalAndWaitEventsAndRemoveOldEvent(eventOption, useAnotherPool);
                    caseResult = testMutateSignalAndWaitEventsAndRemoveOldEvent(env.get(), module, eventOption, useAnotherPool, aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestMutateKernelArgumentsAndEvents);
                }
            }
        }
    }

    if (testMask.test(bitNumberTestSignalCbEvent)) {
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::none,
                                                                 MclTests::EventOptions::timestamp,
                                                                 MclTests::EventOptions::signalEvent,
                                                                 MclTests::EventOptions::signalEventTimestamp,
                                                                 MclTests::EventOptions::cbEvent,
                                                                 MclTests::EventOptions::cbEventTimestamp,
                                                                 MclTests::EventOptions::cbEventSignal,
                                                                 MclTests::EventOptions::cbEventSignalTimestamp};
        for (auto eventOption : eventOptionValues) {
            caseName = testNameMutateSignalEventOnInOrderCommandList(eventOption);
            caseResult = testMutateSignalEventOnInOrderCommandList(env.get(), module, eventOption, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestSignalCbEvent);
        }
    }

    if (testMask.test(bitNumberTestWaitCbEvent)) {
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::cbEvent,
                                                                 MclTests::EventOptions::cbEventTimestamp};
        std::vector<uint32_t> &baseIndexValues = valuesForTests.getBaseIndexValues();
        std::vector<uint32_t> &mutableIndexValues = valuesForTests.getMutableIndexValues();
        for (auto baseIndex : baseIndexValues) {
            for (auto mutableIndex : mutableIndexValues) {
                if (baseIndex == mutableIndex) {
                    continue;
                }
                for (auto eventOption : eventOptionValues) {
                    caseName = testNameCbWaitEventMutateNoopMutateBack(eventOption, baseIndex, mutableIndex);
                    caseResult = testCbWaitEventMutateNoopMutateBack(env.get(), module, eventOption, baseIndex, mutableIndex, aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestWaitCbEvent);
                }
            }
        }
    }

    if (testMask.test(bitNumberTestMixedWaitEvent)) {
        std::vector<bool> srcCmdListInOrderValues = {false, true};
        std::vector<uint32_t> sourceIndexValues = {1, 2};
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::none,
                                                                 MclTests::EventOptions::timestamp,
                                                                 MclTests::EventOptions::signalEvent,
                                                                 MclTests::EventOptions::signalEventTimestamp,
                                                                 MclTests::EventOptions::cbEvent,
                                                                 MclTests::EventOptions::cbEventTimestamp,
                                                                 MclTests::EventOptions::cbEventSignal,
                                                                 MclTests::EventOptions::cbEventSignalTimestamp};

        for (auto srcCmdListInOrder : srcCmdListInOrderValues) {
            for (auto eventOption : eventOptionValues) {
                // cb events are allowed only on in-order command lists
                if (MclTests::isCbEvent(eventOption) && !srcCmdListInOrder) {
                    continue;
                }
                for (auto sourceIndex : sourceIndexValues) {
                    caseName = testNameMixedWaitEvent(eventOption, sourceIndex, srcCmdListInOrder);
                    caseResult = testMixedWaitEvent(env.get(), module, eventOption, sourceIndex, srcCmdListInOrder, aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestMixedWaitEvent);
                }
            }
        }
    }

    if (testMask.test(bitNumberTestCbWaitEventExternalMemory)) {
        if (env->getCbEventExtensionPresent()) {
            caseName = "External Memory CB wait events mutation";
            caseResult = testExternalMemoryCbWaitEvent(env.get(), module, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestCbWaitEventExternalMemory);
        } else {
            std::cerr << "CB events extension not present, skipping tests" << std::endl;
        }
    }

    if (testMask.test(bitNumberTestGroupCount)) {
        caseName = "Group count mutation - copy kernel";
        caseResult = testGroupCountCopy(env.get(), module, aubMode);
        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
        valid &= caseResult;

        std::vector<uint32_t> groupCountData(3);

        auto &groupCountXValues = valuesForTests.getMutableGroupCountXValues();
        auto &groupCountYValues = valuesForTests.getMutableGroupCountYValues();
        auto &groupCountZValues = valuesForTests.getMutableGroupCountZValues();

        for (auto groupCountX : groupCountXValues) {
            for (auto groupCountY : groupCountYValues) {
                for (auto groupCountZ : groupCountZValues) {
                    groupCountData[0] = groupCountX;
                    groupCountData[1] = groupCountY;
                    groupCountData[2] = groupCountZ;

                    std::ostringstream testStream;
                    testStream << "Group count mutation - bif kernel - group count: " << groupCountData[0] << " " << groupCountData[1] << " " << groupCountData[2];

                    caseName = testStream.str();
                    caseResult = testGroupCountBif(env.get(), module, groupCountData.data(), aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestGroupCount);
                }
            }
        }
    }

    if (testMask.test(bitNumberTestGroupSize)) {
        std::vector<uint32_t> groupSizeData(3);

        if (testSubMask.test(0)) {
            auto &groupSizeXValues = valuesForTests.getMutableGroupSizeXValues();
            auto &groupSizeYValues = valuesForTests.getMutableGroupSizeYValues();
            auto &groupSizeZValues = valuesForTests.getMutableGroupSizeZValues();

            for (auto groupSizeX : groupSizeXValues) {
                for (auto groupSizeY : groupSizeYValues) {
                    for (auto groupSizeZ : groupSizeZValues) {
                        groupSizeData[0] = groupSizeX;
                        groupSizeData[1] = groupSizeY;
                        groupSizeData[2] = groupSizeZ;

                        std::ostringstream testStream;
                        testStream << "Group size mutation - bif kernel - group size: " << groupSizeData[0] << " " << groupSizeData[1] << " " << groupSizeData[2];
                        caseName = testStream.str();

                        if (!env->verifyGroupSizeSupported(groupSizeData.data())) {
                            std::cerr << "Skipping case: " << caseName << std::endl;
                            continue;
                        }

                        caseResult = testGroupSizeBif(env.get(), module, groupSizeData.data(), aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberTestGroupSize);
                    }
                }
            }
        }

        std::vector<uint32_t> groupSizeXNonPow2 = {3, 11, 29};
        std::vector<uint32_t> groupSizeYNonPow2 = {3, 5};
        std::vector<uint32_t> groupSizeZNonPow2 = {5, 7};

        if (testSubMask.test(1)) {
            auto &groupSizeXValues = groupSizeXNonPow2;
            auto &groupSizeYValues = groupSizeYNonPow2;
            auto &groupSizeZValues = groupSizeZNonPow2;

            for (auto groupSizeX : groupSizeXValues) {
                for (auto groupSizeY : groupSizeYValues) {
                    for (auto groupSizeZ : groupSizeZValues) {
                        groupSizeData[0] = groupSizeX;
                        groupSizeData[1] = groupSizeY;
                        groupSizeData[2] = groupSizeZ;

                        std::ostringstream testStream;
                        testStream << "Group size non-pow 2 mutation - bif kernel - group size: " << groupSizeData[0] << " " << groupSizeData[1] << " " << groupSizeData[2];
                        caseName = testStream.str();

                        if (!env->verifyGroupSizeSupported(groupSizeData.data())) {
                            std::cerr << "Skipping case: " << caseName << std::endl;
                            continue;
                        }

                        caseResult = testGroupSizeBif(env.get(), module, groupSizeData.data(), aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberTestGroupSize);
                    }
                }
            }
        }
    }

    if (testMask.test(bitNumberTestGlobalOffset)) {
        caseName = "Global offset mutation - copy kernel";
        caseResult = testGlobalOffsetCopy(env.get(), module, aubMode);
        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
        valid &= caseResult;

        std::vector<uint32_t> offsetData(3);

        std::vector<uint32_t> offsetXValues = {1, 2};
        std::vector<uint32_t> offsetYValues = {1, 2, 3};
        std::vector<uint32_t> offsetZValues = {2, 3};

        for (auto offsetX : offsetXValues) {
            for (auto offsetY : offsetYValues) {
                for (auto offsetZ : offsetZValues) {
                    offsetData[0] = offsetX;
                    offsetData[1] = offsetY;
                    offsetData[2] = offsetZ;

                    std::ostringstream testStream;
                    testStream << "Global offset mutation - bif kernel - offset size: " << offsetData[0] << " " << offsetData[1] << " " << offsetData[2];

                    caseName = testStream.str();
                    caseResult = testGlobalOffsetBif(env.get(), module, offsetData.data(), aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestGlobalOffset);
                }
            }
        }
    }

    if (testMask.test(bitNumberTestMixedTests)) {
        if (testSubMask.test(0)) {
            caseName = "Mixed Offset Arguments and Global Size 3D non-power of 2 mutation";
            caseResult = testMixedOffsetMemoryArgumentGlobalSize3DNonPow2(env.get(), module, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestMixedTests);
        }

        if (testSubMask.test(1)) {
            caseName = "Set buffer argument to null and then make argument mutation";
            caseResult = testSetNullPtrThenMutateToBuffer(env.get(), module, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestMixedTests);
        }

        if (testSubMask.test(2)) {
            caseName = "Mutate group size dimensions, remain total group size constant";
            caseResult = testGroupSizeDimensionsTotalConstant(env.get(), module, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestMixedTests);
        }

        if (testSubMask.test(3)) {
            caseName = "Mutate kernel argument from buffer to null and back to buffer";
            caseResult = testMutateBufferToNullAndBack(env.get(), module, aubMode, false);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestMixedTests);

            caseName = "Mutate kernel argument from null to buffer and back to null";
            caseResult = testMutateBufferToNullAndBack(env.get(), module, aubMode, true);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestMixedTests);
        }
    }

    if (testMask.test(bitNumberTestMultiExecutionMutatedCbEvents)) {
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::cbEvent,
                                                                 MclTests::EventOptions::cbEventTimestamp,
                                                                 MclTests::EventOptions::cbEventSignal,
                                                                 MclTests::EventOptions::cbEventSignalTimestamp};
        for (auto eventOption : eventOptionValues) {
            caseName = testNameMultiExecutionMutatedCbEvents(eventOption);
            caseResult = testMultiExecutionMutatedCbEvents(env.get(), module, eventOption, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestMultiExecutionMutatedCbEvents);
        }
    }

    if (testMask.test(bitNumberTestComplexStructurePassedAsImmediate)) {
        std::vector<bool> conditionalMulValues = {true, false};
        std::vector<bool> conditionalAddValues = {true, false};
        for (auto conditionalMul : conditionalMulValues) {
            for (auto conditionalAdd : conditionalAddValues) {

                if (testSubMask.test(0)) {
                    caseName = testNameMutateComplexStructurePassedAsImmediate(conditionalAdd, conditionalMul, 1, true);
                    caseResult = testMutateComplexStructurePassedAsImmediate<ConditionalCalculations1, 1, true>(env.get(), module, conditionalAdd, conditionalMul, aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestComplexStructurePassedAsImmediate);
                }

                if (testSubMask.test(1)) {
                    caseName = testNameMutateComplexStructurePassedAsImmediate(conditionalAdd, conditionalMul, 2, true);
                    caseResult = testMutateComplexStructurePassedAsImmediate<ConditionalCalculations2, 2, true>(env.get(), module, conditionalAdd, conditionalMul, aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestComplexStructurePassedAsImmediate);
                }

                if (testSubMask.test(2)) {
                    caseName = testNameMutateComplexStructurePassedAsImmediate(conditionalAdd, conditionalMul, 3, true);
                    caseResult = testMutateComplexStructurePassedAsImmediate<ConditionalCalculations3, 3, true>(env.get(), module, conditionalAdd, conditionalMul, aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestComplexStructurePassedAsImmediate);
                }

                if (testSubMask.test(3)) {
                    caseName = testNameMutateComplexStructurePassedAsImmediate(conditionalAdd, conditionalMul, 4, true);
                    caseResult = testMutateComplexStructurePassedAsImmediate<ConditionalCalculations4, 4, true>(env.get(), module, conditionalAdd, conditionalMul, aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberTestComplexStructurePassedAsImmediate);
                }

                if (!conditionalAdd) {
                    if (testSubMask.test(4)) {
                        caseName = testNameMutateComplexStructurePassedAsImmediate(conditionalAdd, conditionalMul, 1, false);
                        caseResult = testMutateComplexStructurePassedAsImmediate<ConditionalMulCalculations1, 1, false>(env.get(), module, conditionalAdd, conditionalMul, aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberTestComplexStructurePassedAsImmediate);
                    }

                    if (testSubMask.test(5)) {
                        caseName = testNameMutateComplexStructurePassedAsImmediate(conditionalAdd, conditionalMul, 2, false);
                        caseResult = testMutateComplexStructurePassedAsImmediate<ConditionalMulCalculations2, 2, false>(env.get(), module, conditionalAdd, conditionalMul, aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberTestComplexStructurePassedAsImmediate);
                    }
                }
            }
        }
    }

    if (testMask.test(bitNumberTestKernelSlmArgument)) {
        std::vector<uint32_t> groupSizeValues = {1, 16, 32, 64};
        std::vector<uint32_t> groupCountValues = {1, 2};

        if (testSubMask.test(0)) {
            constexpr bool kernelOneArg = true;
            for (auto firstGroupSize : groupSizeValues) {
                for (auto mutateGroupSize : groupSizeValues) {
                    if (firstGroupSize == mutateGroupSize) {
                        continue;
                    }
                    for (auto groupCount : groupCountValues) {
                        caseName = testNameMutateSlmArgumentKernel(firstGroupSize, mutateGroupSize, groupCount, kernelOneArg);
                        caseResult = testMutateSlmArgumentKernel(env.get(), env->getSlmModule(), firstGroupSize, mutateGroupSize, groupCount, kernelOneArg, aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberTestKernelSlmArgument);
                    }
                }
            }
        }
        if (testSubMask.test(1)) {
            constexpr bool kernelOneArg = false;
            for (auto firstGroupSize : groupSizeValues) {
                for (auto mutateGroupSize : groupSizeValues) {
                    if (firstGroupSize == mutateGroupSize) {
                        continue;
                    }
                    for (auto groupCount : groupCountValues) {
                        caseName = testNameMutateSlmArgumentKernel(firstGroupSize, mutateGroupSize, groupCount, kernelOneArg);
                        caseResult = testMutateSlmArgumentKernel(env.get(), env->getSlmModule(), firstGroupSize, mutateGroupSize, groupCount, kernelOneArg, aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberTestKernelSlmArgument);
                    }
                }
            }
        }
    }

    if (testMask.test(bitNumberTestCbSignalIncrementEvent)) {
        if (env->getCbEventExtensionPresent()) {
            caseName = "Signal external increment CB Event";
            caseResult = testSignalExternalIncrementCbEvent(env.get(), module, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestCbSignalIncrementEvent);
        } else {
            std::cerr << "CB events extension not present, skipping tests" << std::endl;
        }
    }

    if (testMask.test(bitNumberTestExperimental)) {
        if (testSubMask.test(0)) {
            caseName = "Simple SLM kernel";
            caseResult = testSimpleSlm(env.get(), aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestExperimental);
        }

        if (testSubMask.test(1)) {
            caseName = "Sync via Event and Mutate Shared Memory Kernel Arg";
            caseResult = testMutateKernelArgSharedMemoryAndEventSync(env.get(), module, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestExperimental);
        }

        if (testSubMask.test(2)) {
            constexpr uint32_t defaultAppendCount = 4335;
            uint32_t appendCount = LevelZeroBlackBoxTests::getParamValue(argc, argv, "-ac", "-appendCount", defaultAppendCount);
            caseName = "Immediate Command List Execution of MCL";
            caseResult = testImmediateCmdListExecuteMcl(env.get(), aubMode, appendCount);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestExperimental);
        }

        if (testSubMask.test(3)) {
            caseName = "Profile append operations using timestamp events";
            caseResult = testProfileAppendOperationsUsingTsEvents(env.get(), module, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestExperimental);
        }

        if (testSubMask.test(4)) {
            auto getCaseName = [](bool immediateSubmit, bool copyOffload, uint32_t iteration) -> std::string {
                std::ostringstream caseName;
                caseName << "Graph submmission ";
                caseName << "Immediate submit: " << (immediateSubmit ? "true" : "false") << std::endl;
                caseName << "Copy offload: " << (copyOffload ? "true" : "false");
                caseName << " Iteration: " << iteration;
                caseName << ".";
                return caseName.str();
            };

            constexpr uint32_t defaultIterations = 3;
            constexpr bool defaultImmediateSubmit = true;
            constexpr bool defaultCopyOffload = true;

            uint32_t iterations = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--iterations", defaultIterations);
            bool immediateSubmit = !!LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--use_immediate", defaultImmediateSubmit);
            bool copyOffload = !!LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--copy_offload", defaultCopyOffload);

            caseName = getCaseName(immediateSubmit, copyOffload, iterations);
            caseResult = testSubmitGraphOnImmediate(env.get(), module, immediateSubmit, copyOffload, iterations, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestExperimental);
        }

        if (testSubMask.test(5)) {
            uint32_t copyOrdinal = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(env->device);
            if (copyOrdinal == LevelZeroBlackBoxTests::undefinedQueueOrdinal) {
                std::cerr << "No copy only command queue found, skipping test" << std::endl;
            } else {
                std::string baseTitle = "Submit Copy Command List on Copy Immediate Command List";
                auto getCaseName = [&baseTitle](bool useImmediate, uint32_t iteration) -> std::string {
                    std::ostringstream caseName;
                    caseName << baseTitle << std::endl;
                    caseName << "Immediate submit: " << (useImmediate ? "true" : "false");
                    caseName << " Iteration: " << iteration;
                    caseName << ".";
                    return caseName.str();
                };

                constexpr uint32_t defaultIterations = 3;
                constexpr bool defaultImmediateSubmit = true;

                uint32_t iterations = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--iterations", defaultIterations);
                bool immediateSubmit = !!LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--use_immediate", defaultImmediateSubmit);

                caseName = getCaseName(immediateSubmit, iterations);
                caseResult = testSubmitCopyCmdListOnCopyImmediateCmdList(env.get(), copyOrdinal, immediateSubmit, iterations, aubMode);
                LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                valid &= caseResult;
                MclTests::addFailedCase(caseResult, caseName, bitNumberTestExperimental);
            }
        }

        if (testSubMask.test(6)) {
            std::string baseTitle = "Append Regular Command List on Out-of-Order Immediate Command List with Event Sync";
            auto getCaseName = [&baseTitle](uint32_t iteration, bool useEvents) -> std::string {
                std::ostringstream caseName;
                caseName << baseTitle << std::endl;
                caseName << "Iterations: " << iteration;
                caseName << ".";
                caseName << "Use event for exec synchronization: " << std::boolalpha << useEvents;
                return caseName.str();
            };

            constexpr uint32_t defaultIterations = 2;
            uint32_t iterations = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--iterations", defaultIterations);
            constexpr bool defaultUseEvents = true;
            bool useEvents = static_cast<bool>(LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--use_event", static_cast<int>(defaultUseEvents)));

            caseName = getCaseName(iterations, useEvents);
            caseResult = testAppendRegularOnOutOfOrderImmediate(env.get(), module, iterations, useEvents, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestExperimental);
        }

        if (testSubMask.test(7)) {
            std::string baseTitle = "Append Multiple Regular Command List on Immediate Command List with Barrier";
            auto getCaseName = [&baseTitle](uint32_t regularCount, uint32_t iteration, bool inOrder) -> std::string {
                std::ostringstream caseName;
                caseName << baseTitle << std::endl;
                caseName << "Regular Count: " << regularCount;
                caseName << ". ";
                caseName << "Iterations: " << iteration;
                caseName << ". ";
                caseName << "In-Order: " << std::boolalpha << inOrder;
                caseName << ".";
                return caseName.str();
            };

            constexpr bool defaultInOrder = false;
            bool inOrder = static_cast<bool>(LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--in_order", static_cast<int>(defaultInOrder)));
            constexpr uint32_t defaultIterations = 2;
            uint32_t iterations = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--iterations", defaultIterations);
            constexpr uint32_t defaultRegularCount = 2;
            uint32_t regularCount = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--count", defaultRegularCount);

            caseName = getCaseName(regularCount, iterations, inOrder);
            caseResult = testAppendMultipleRegularOnInOrderImmediate(env.get(), module, regularCount, iterations, inOrder, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestExperimental);
        }
    }

    env->releaseMainModule(module);
    int mainRetCode = aubMode ? 0 : (valid ? 0 : 1);
    std::string finalStatus = (mainRetCode != 0) ? " FAILED" : " SUCCESS";
    std::cerr << blackBoxName << finalStatus << std::endl;
    MclTests::printFailedCases();
    return mainRetCode;
}

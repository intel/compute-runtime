/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/black_box_tests/mcl_samples/zello_mcl_test_helper.h"

#include <array>
#include <cstring>
#include <memory>
#include <sstream>

using KernelType = MclTests::KernelType;

std::string testNameMutateKernelsWaitSignalEvents(MclTests::EventOptions eventOptions) {
    std::ostringstream testStream;

    testStream << "Mutate kernels and wait & signal events" << std::endl;
    MclTests::setEventTestStream(eventOptions, testStream);
    testStream << " Test";

    return testStream.str();
}

bool testMutateKernelsWaitSignalEvents(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode, MclTests::EventOptions eventOptions) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    bool isCbEvent = MclTests::isCbEvent(eventOptions);
    bool isTsEvent = MclTests::isTimestampEvent(eventOptions);

    // prepare resources
    ze_command_list_handle_t copyCmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, copyCmdList, isCbEvent, false);

    ze_command_list_handle_t addCmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, addCmdList, isCbEvent, false);

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> events(numEvents);
    std::vector<ze_kernel_timestamp_result_t> eventTimestamps(numEvents + 1);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");
    ze_kernel_handle_t copyLinearKernel = execEnv->getKernelHandle(module, "copyKernelLinear");
    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");
    ze_kernel_handle_t addLinearKernel = execEnv->getKernelHandle(module, "addScalarKernelLinear");

    ze_kernel_handle_t copyKernelGroup[] = {copyKernel, copyLinearKernel};
    ze_kernel_handle_t addKernelGroup[] = {addKernel, addLinearKernel};

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;
    void *stageBuffer1 = nullptr;
    void *stageBuffer2 = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &stageBuffer2));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
    }
    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    ElemType scalarValueAdd1 = 0x1;
    ElemType scalarValueAdd2 = 0x3;

    // setup copy command list
    uint32_t copyKernelGroupSizeX = 32u;
    uint32_t copyKernelGroupSizeY = 1u;
    uint32_t copyKernelGroupSizeZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &copyKernelGroupSizeX, &copyKernelGroupSizeY, &copyKernelGroupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, copyKernelGroupSizeX, copyKernelGroupSizeY, copyKernelGroupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &stageBuffer1));

    ze_group_count_t copyKernelDispatchTraits;
    copyKernelDispatchTraits.groupCountX = allocSize / copyKernelGroupSizeX;
    copyKernelDispatchTraits.groupCountY = copyKernelGroupSizeY;
    copyKernelDispatchTraits.groupCountZ = copyKernelGroupSizeZ;

    uint64_t commandIdCopy = 0;
    ze_mutable_command_id_exp_desc_t commandIdCopyDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdCopyDesc.flags =
        ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
        ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
        ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
        ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT |
        ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(copyCmdList, &commandIdCopyDesc, 2, copyKernelGroup, &commandIdCopy));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(copyCmdList, copyKernel, &copyKernelDispatchTraits, events[0], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(copyCmdList));

    // setup add command list
    uint32_t addKernelGroupSizeX = 32u;
    uint32_t addKernelGroupSizeY = 1u;
    uint32_t addKernelGgroupSizeZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addKernel, elemSize, 1U, 1U, &addKernelGroupSizeX, &addKernelGroupSizeY, &addKernelGgroupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernel, addKernelGroupSizeX, addKernelGroupSizeY, addKernelGgroupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 0, sizeof(void *), &stageBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 1, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 2, sizeof(scalarValueAdd1), &scalarValueAdd1));

    ze_group_count_t addKernelDispatchTraits;
    addKernelDispatchTraits.groupCountX = elemSize / addKernelGroupSizeX;
    addKernelDispatchTraits.groupCountY = addKernelGroupSizeY;
    addKernelDispatchTraits.groupCountZ = addKernelGgroupSizeZ;

    uint64_t commandIdAdd = 0;
    ze_mutable_command_id_exp_desc_t commandIdAddDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdAddDesc.flags =
        ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
        ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
        ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
        ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS |
        ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;

    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(addCmdList, &commandIdAddDesc, 2, addKernelGroup, &commandIdAdd));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(addCmdList, addKernel, &addKernelDispatchTraits, nullptr, 1, &events[0]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(addCmdList));

    ze_command_list_handle_t batchCmdList[] = {copyCmdList, addCmdList};

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 2, batchCmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValueAdd = val1 + scalarValueAdd1;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer1, elemSize) == false) {
            std::cerr << "before mutation" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[0]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[0], &eventTimestamps[0]));
        }
        if (!isCbEvent) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(events[0]));
        }
    }

    // mutate copy command list into another copy kernel
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(copyCmdList, 1, &commandIdCopy, &copyLinearKernel));
    // get new kernel arguments
    {
        SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyLinearKernel, allocSize, 1U, 1U, &copyKernelGroupSizeX, &copyKernelGroupSizeY, &copyKernelGroupSizeZ));

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandIdCopy;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &srcBuffer2;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandIdCopy;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &stageBuffer2;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        copyKernelDispatchTraits.groupCountX = allocSize / copyKernelGroupSizeX;
        copyKernelDispatchTraits.groupCountY = copyKernelGroupSizeY;
        copyKernelDispatchTraits.groupCountZ = copyKernelGroupSizeZ;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandIdCopy;
        mutateGroupCount.pGroupCount = &copyKernelDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandIdCopy;
        mutateGroupSize.groupSizeX = copyKernelGroupSizeX;
        mutateGroupSize.groupSizeY = copyKernelGroupSizeY;
        mutateGroupSize.groupSizeZ = copyKernelGroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;

        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(copyCmdList, &mutateDesc));
    }
    // mutate new signal event
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(copyCmdList, commandIdCopy, events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(copyCmdList));

    // mutate add command list into another add kernel
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(addCmdList, 1, &commandIdAdd, &addLinearKernel));
    // get new kernel arguments
    {
        SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addLinearKernel, elemSize, 1U, 1U, &addKernelGroupSizeX, &addKernelGroupSizeY, &addKernelGgroupSizeZ));

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandIdAdd;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &stageBuffer2;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandIdAdd;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &dstBuffer2;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelScalarArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelScalarArg.commandId = commandIdAdd;
        mutateKernelScalarArg.argIndex = 2;
        mutateKernelScalarArg.argSize = sizeof(ElemType);
        mutateKernelScalarArg.pArgValue = &scalarValueAdd2;
        mutateKernelScalarArg.pNext = &mutateKernelDstArg;

        addKernelDispatchTraits.groupCountX = elemSize / addKernelGroupSizeX;
        addKernelDispatchTraits.groupCountY = addKernelGroupSizeY;
        addKernelDispatchTraits.groupCountZ = addKernelGgroupSizeZ;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandIdAdd;
        mutateGroupCount.pGroupCount = &addKernelDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelScalarArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandIdAdd;
        mutateGroupSize.groupSizeX = addKernelGroupSizeX;
        mutateGroupSize.groupSizeY = addKernelGroupSizeY;
        mutateGroupSize.groupSizeZ = addKernelGgroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;

        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(addCmdList, &mutateDesc));
    }
    // mutate new wait event
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(addCmdList, commandIdAdd, 1, &events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(addCmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 2, batchCmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValueAdd = val2 + scalarValueAdd2;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer2, elemSize) == false) {
            std::cerr << "after mutation" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[1]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[1], &eventTimestamps[1]));

            if (eventTimestamps[0].context.kernelEnd > eventTimestamps[1].context.kernelEnd) {
                std::cerr << "eventTimestamps[0] greater than eventTimestamps[1]" << std::endl;
                valid = false;
            }
        }
        if (!isCbEvent) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(events[1]));
        }
    }

    // reset stage and destination buffers on the GPU first - synchronous immediate for simplicity
    ElemType zero = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, stageBuffer1, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, dstBuffer1, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));

    // bring back old kernels and old arguments
    // copy command list
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(copyCmdList, 1, &commandIdCopy, &copyKernel));
    {
        SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &copyKernelGroupSizeX, &copyKernelGroupSizeY, &copyKernelGroupSizeZ));

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandIdCopy;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &srcBuffer1;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandIdCopy;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &stageBuffer1;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        copyKernelDispatchTraits.groupCountX = allocSize / copyKernelGroupSizeX;
        copyKernelDispatchTraits.groupCountY = copyKernelGroupSizeY;
        copyKernelDispatchTraits.groupCountZ = copyKernelGroupSizeZ;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandIdCopy;
        mutateGroupCount.pGroupCount = &copyKernelDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandIdCopy;
        mutateGroupSize.groupSizeX = copyKernelGroupSizeX;
        mutateGroupSize.groupSizeY = copyKernelGroupSizeY;
        mutateGroupSize.groupSizeZ = copyKernelGroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;

        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(copyCmdList, &mutateDesc));
    }
    // mutate old signal event
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(copyCmdList, commandIdCopy, events[0]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(copyCmdList));

    // add kernel
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(addCmdList, 1, &commandIdAdd, &addKernel));
    {
        SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addKernel, elemSize, 1U, 1U, &addKernelGroupSizeX, &addKernelGroupSizeY, &addKernelGgroupSizeZ));

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandIdAdd;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &stageBuffer1;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandIdAdd;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &dstBuffer1;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelScalarArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelScalarArg.commandId = commandIdAdd;
        mutateKernelScalarArg.argIndex = 2;
        mutateKernelScalarArg.argSize = sizeof(ElemType);
        mutateKernelScalarArg.pArgValue = &scalarValueAdd1;
        mutateKernelScalarArg.pNext = &mutateKernelDstArg;

        addKernelDispatchTraits.groupCountX = elemSize / addKernelGroupSizeX;
        addKernelDispatchTraits.groupCountY = addKernelGroupSizeY;
        addKernelDispatchTraits.groupCountZ = addKernelGgroupSizeZ;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandIdAdd;
        mutateGroupCount.pGroupCount = &addKernelDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelScalarArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandIdAdd;
        mutateGroupSize.groupSizeX = addKernelGroupSizeX;
        mutateGroupSize.groupSizeY = addKernelGroupSizeY;
        mutateGroupSize.groupSizeZ = addKernelGgroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;

        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(addCmdList, &mutateDesc));
    }
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(addCmdList, commandIdAdd, 1, &events[0]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(addCmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 2, batchCmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValueAdd = val1 + scalarValueAdd1;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValueAdd, dstBuffer1, elemSize) == false) {
            std::cerr << "after back mutation" << std::endl;
            valid = false;
        }
        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[0]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[0], &eventTimestamps[2]));

            if (eventTimestamps[1].context.kernelEnd > eventTimestamps[2].context.kernelEnd) {
                std::cerr << "eventTimestamps[1] greater than eventTimestamps[2]" << std::endl;
                valid = false;
            }
        }
    }

    for (auto &event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, stageBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(addCmdList));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(copyCmdList));
    execEnv->destroyKernelHandle(addLinearKernel);
    execEnv->destroyKernelHandle(addKernel);
    execEnv->destroyKernelHandle(copyLinearKernel);
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

std::string testNameMutateSignalEventArgumentsThenKernelAndKernelBackMutateIntoThirdSignalEventArguments(MclTests::EventOptions eventOptions) {
    std::ostringstream testStream;

    testStream << "Mutate signal event/arguments into Add kernel back Mul kernel into signal event/arguments" << std::endl;
    MclTests::setEventTestStream(eventOptions, testStream);
    testStream << " Test";

    return testStream.str();
}

bool testMutateSignalEventArgumentsThenKernelAndKernelBackMutateIntoThirdSignalEventArguments(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode, MclTests::EventOptions eventOptions) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    bool isCbEvent = MclTests::isCbEvent(eventOptions);
    bool isTsEvent = MclTests::isTimestampEvent(eventOptions);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, isCbEvent, false);

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 3;
    std::vector<ze_event_handle_t> signalEvents(numEvents);
    std::vector<ze_kernel_timestamp_result_t> eventTimestamps(numEvents + 1);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, signalEvents.data(), eventOptions);

    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");
    ze_kernel_handle_t mulKernel = execEnv->getKernelHandle(module, "mulScalarKernel");

    ze_kernel_handle_t kernelGroup[] = {addKernel, mulKernel};

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    void *srcBuffer3 = nullptr;
    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;
    void *dstBuffer3 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer3));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer3));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 12;
    constexpr ElemType val3 = 14;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = val1;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = val2;
        reinterpret_cast<ElemType *>(srcBuffer3)[i] = val3;
    }
    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);
    memset(dstBuffer3, 0, allocSize);

    ElemType scalarValue1 = 0x1;
    ElemType scalarValue2 = 0x2;
    ElemType scalarValue3 = 0x3;

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(mulKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(mulKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 1, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 2, sizeof(scalarValue1), &scalarValue1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandId));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, mulKernel, &dispatchTraits, signalEvents[0], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = val1 * scalarValue1;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer1, elemSize) == false) {
            std::cerr << "before mutation - fail mul operation" << std::endl;
            valid = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(signalEvents[0]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(signalEvents[0], &eventTimestamps[0]));
        }
        if (!isCbEvent) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(signalEvents[0]));
        }
    }

    // 1st mutation step - update kernel arguments and signal event with the same mul kernel
    ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelSrcArg.commandId = commandId;
    mutateKernelSrcArg.argIndex = 0;
    mutateKernelSrcArg.argSize = sizeof(void *);
    mutateKernelSrcArg.pArgValue = &srcBuffer2;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelDstArg.commandId = commandId;
    mutateKernelDstArg.argIndex = 1;
    mutateKernelDstArg.argSize = sizeof(void *);
    mutateKernelDstArg.pArgValue = &dstBuffer2;
    mutateKernelDstArg.pNext = &mutateKernelSrcArg;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelScalarArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelScalarArg.commandId = commandId;
    mutateKernelScalarArg.argIndex = 2;
    mutateKernelScalarArg.argSize = sizeof(ElemType);
    mutateKernelScalarArg.pArgValue = &scalarValue2;
    mutateKernelScalarArg.pNext = &mutateKernelDstArg;

    ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
    mutateGroupCount.commandId = commandId;
    mutateGroupCount.pGroupCount = &dispatchTraits;
    mutateGroupCount.pNext = &mutateKernelScalarArg;

    ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    mutateGroupSize.commandId = commandId;
    mutateGroupSize.groupSizeX = groupSizeX;
    mutateGroupSize.groupSizeY = groupSizeY;
    mutateGroupSize.groupSizeZ = groupSizeZ;
    mutateGroupSize.pNext = &mutateGroupCount;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateGroupSize;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandId, signalEvents[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = val2 * scalarValue2;
    void *outputBuffer = dstBuffer2;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, outputBuffer, elemSize) == false) {
            std::cerr << "after mutation of kernel arguments - fail mul operation" << std::endl;
            valid = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(signalEvents[1]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(signalEvents[1], &eventTimestamps[1]));
        }
        if (!isCbEvent) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(signalEvents[1]));
        }
    }

    // 2nd mutation step - select new kernel with the same arguments and signal event - no need to update signal event
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &addKernel));
    // new kernel - invalidated arguments and dispatch parameters - provide them again
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = val2 + scalarValue2;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, outputBuffer, elemSize) == false) {
            std::cerr << "after mutation of kernel into add - same arguments - fail add operation" << std::endl;
            valid = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(signalEvents[1]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(signalEvents[1], &eventTimestamps[2]));
        }
        if (!isCbEvent) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(signalEvents[1]));
        }
    }

    // 3rd mutation step - select mul kernel back with new arguments and signal event
    // signal can be mutated before kernel
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandId, signalEvents[2]));
    // select mul kernel back
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mulKernel));

    // select arguments and dispatch parameters for mul kernel, use new arguments for mutation
    mutateKernelSrcArg.pArgValue = &srcBuffer3;
    mutateKernelDstArg.pArgValue = &dstBuffer3;
    mutateKernelScalarArg.pArgValue = &scalarValue3;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = val3 * scalarValue3;
    outputBuffer = dstBuffer3;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, outputBuffer, elemSize) == false) {
            std::cerr << "after back mutation into mul and new kernel arguments - fail mul operation" << std::endl;
            valid = false;
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(signalEvents[2]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(signalEvents[2], &eventTimestamps[3]));

            if (eventTimestamps[0].context.kernelEnd > eventTimestamps[1].context.kernelEnd) {
                std::cerr << "eventTimestamps[0] greater than eventTimestamps[1]" << std::endl;
                valid = false;
            }
            if (eventTimestamps[1].context.kernelEnd > eventTimestamps[2].context.kernelEnd) {
                std::cerr << "eventTimestamps[1] greater than eventTimestamps[2]" << std::endl;
                valid = false;
            }
            if (eventTimestamps[2].context.kernelEnd > eventTimestamps[3].context.kernelEnd) {
                std::cerr << "eventTimestamps[2] greater than eventTimestamps[3]" << std::endl;
                valid = false;
            }
        }
    }

    for (auto &event : signalEvents) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer3));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer3));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(mulKernel);
    execEnv->destroyKernelHandle(addKernel);

    return valid;
}

std::string testNameMutateMulKernelIntoAddKernelBackMulKernel(bool useSameArguments, MclTests::EventOptions eventOptions) {
    std::ostringstream testStream;

    testStream << "Mutate Mul kernel Into Add kernel and back into Mul kernel";
    testStream << (useSameArguments ? " use same arguments" : " use different arguments") << std::endl;
    MclTests::setEventTestStream(eventOptions, testStream);
    testStream << " Test";

    return testStream.str();
}

bool testMutateMulKernelIntoAddKernelBackMulKernel(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool useSameArguments, bool aubMode, MclTests::EventOptions eventOptions) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    bool isCbEvent = MclTests::isCbEvent(eventOptions);
    bool isTsEvent = MclTests::isTimestampEvent(eventOptions);

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, isCbEvent, false);

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 1;
    ze_event_handle_t signalEvent = nullptr;

    ze_kernel_timestamp_result_t firstExecutionTimestamp = {};
    ze_kernel_timestamp_result_t secondExecutionTimestamp = {};
    ze_kernel_timestamp_result_t thirdExecutionTimestamp = {};

    if (eventOptions != MclTests::EventOptions::noEvent) {
        MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, &signalEvent, eventOptions);
    }

    ze_kernel_handle_t addKernel = execEnv->getKernelHandle(module, "addScalarKernel");
    ze_kernel_handle_t mulKernel = execEnv->getKernelHandle(module, "mulScalarKernel");

    ze_kernel_handle_t kernelGroup[] = {addKernel, mulKernel};

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

    void *mutableSrcBuffer = useSameArguments ? srcBuffer1 : srcBuffer2;
    void *mutableDstBuffer = useSameArguments ? dstBuffer1 : dstBuffer2;
    ElemType mutableScalarValue = useSameArguments ? scalarValue1 : scalarValue2;

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(mulKernel, elemSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(mulKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 1, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 2, sizeof(scalarValue1), &scalarValue1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = elemSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE | ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandId));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, mulKernel, &dispatchTraits, signalEvent, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = val1 * scalarValue1;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBuffer1, elemSize) == false) {
            std::cerr << "before mutation - fail mul operation" << std::endl;
            valid = false;
        }

        if (signalEvent != nullptr) {
            SUCCESS_OR_TERMINATE(zeEventQueryStatus(signalEvent));
            if (isTsEvent) {
                SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(signalEvent, &firstExecutionTimestamp));
            }
            if (!isCbEvent) {
                SUCCESS_OR_TERMINATE(zeEventHostReset(signalEvent));
            }
        }
    }

    // select new kernel
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &addKernel));

    // select arguments and dispatch parameters for new kernel
    ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelSrcArg.commandId = commandId;
    mutateKernelSrcArg.argIndex = 0;
    mutateKernelSrcArg.argSize = sizeof(void *);
    mutateKernelSrcArg.pArgValue = &mutableSrcBuffer;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelDstArg.commandId = commandId;
    mutateKernelDstArg.argIndex = 1;
    mutateKernelDstArg.argSize = sizeof(void *);
    mutateKernelDstArg.pArgValue = &mutableDstBuffer;
    mutateKernelDstArg.pNext = &mutateKernelSrcArg;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelScalarArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelScalarArg.commandId = commandId;
    mutateKernelScalarArg.argIndex = 2;
    mutateKernelScalarArg.argSize = sizeof(ElemType);
    mutateKernelScalarArg.pArgValue = &mutableScalarValue;
    mutateKernelScalarArg.pNext = &mutateKernelDstArg;

    ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
    mutateGroupCount.commandId = commandId;
    mutateGroupCount.pGroupCount = &dispatchTraits;
    mutateGroupCount.pNext = &mutateKernelScalarArg;

    ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    mutateGroupSize.commandId = commandId;
    mutateGroupSize.groupSizeX = groupSizeX;
    mutateGroupSize.groupSizeY = groupSizeY;
    mutateGroupSize.groupSizeZ = groupSizeZ;
    mutateGroupSize.pNext = &mutateGroupCount;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateGroupSize;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = useSameArguments ? (val1 + scalarValue1) : (val2 + scalarValue2);
    void *outputBuffer = mutableDstBuffer;

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, outputBuffer, elemSize) == false) {
            std::cerr << "after mutation - fail add operation - same arguments: " << std::boolalpha << useSameArguments << std::endl;
            valid = false;
        }

        if (signalEvent != nullptr) {
            SUCCESS_OR_TERMINATE(zeEventQueryStatus(signalEvent));
            if (isTsEvent) {
                SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(signalEvent, &secondExecutionTimestamp));
            }
            if (!isCbEvent) {
                SUCCESS_OR_TERMINATE(zeEventHostReset(signalEvent));
            }
        }
    }

    // select mul kernel back
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mulKernel));

    // select arguments and dispatch parameters for mul kernel, use the same arguments for mutation and kernels use the same layout of arguments
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    expectedValue = useSameArguments ? (val1 * scalarValue1) : (val2 * scalarValue2);

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, outputBuffer, elemSize) == false) {
            std::cerr << "after mutation - fail mul operation - same arguments: " << std::boolalpha << useSameArguments << std::endl;
            valid = false;
        }

        if (signalEvent != nullptr) {
            SUCCESS_OR_TERMINATE(zeEventQueryStatus(signalEvent));
            if (isTsEvent) {
                SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(signalEvent, &thirdExecutionTimestamp));

                if (firstExecutionTimestamp.context.kernelEnd > secondExecutionTimestamp.context.kernelEnd) {
                    std::cerr << "firstExecutionTimestamp greater than secondExecutionTimestamp" << std::endl;
                    valid = false;
                }
                if (secondExecutionTimestamp.context.kernelEnd > thirdExecutionTimestamp.context.kernelEnd) {
                    std::cerr << "secondExecutionTimestamp greater than thirdExecutionTimestamp" << std::endl;
                    valid = false;
                }
            }
        }
    }

    if (signalEvent != nullptr) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(signalEvent));
    }
    if (eventPool != nullptr) {
        MclTests::destroyEventPool(eventPool, eventOptions);
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(mulKernel);
    execEnv->destroyKernelHandle(addKernel);

    return valid;
}

std::string testNameMutateKernelsScratch(bool scratchKernelFirst) {
    std::ostringstream testStream;
    testStream << "Mutate Kernels with Scratch:" << std::endl;
    if (scratchKernelFirst) {
        testStream << "scratch kernel first, mutate into copy kernel";
    } else {
        testStream << "copy kernel first, mutate into scratch";
    }
    testStream << " Test";

    return testStream.str();
}

bool testMutateKernelsScratch(MclTests::ExecEnv *execEnv, ze_module_handle_t module, ze_kernel_handle_t scratchKernel, bool scratchKernelFirst, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    // prepare copy buffers
    void *copySrcBuffer = nullptr;
    void *copyDstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copyDstBuffer));

    constexpr ElemType val = 10;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(copySrcBuffer)[i] = val;
    }
    memset(copyDstBuffer, 0, allocSize);

    // prepare scratch buffers
    uint32_t arraySize = 0;
    uint32_t vectorSize = 0;
    uint32_t srcAdditionalMul = 0;

    uint32_t expectedMemorySize = 0;
    uint32_t srcMemorySize = 0;
    uint32_t idxMemorySize = 0;

    LevelZeroBlackBoxTests::prepareScratchTestValues(arraySize, vectorSize, expectedMemorySize, srcAdditionalMul, srcMemorySize, idxMemorySize);
    void *scratchSrcBuffer = nullptr;
    void *scratchDstBuffer = nullptr;
    void *scratchIdxBuffer = nullptr;
    void *scratchExpectedMemory = nullptr;

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, srcMemorySize, 1, &scratchSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, expectedMemorySize, 1, &scratchDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, idxMemorySize, 1, &scratchIdxBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, expectedMemorySize, 1, &scratchExpectedMemory));

    memset(scratchSrcBuffer, 0, srcMemorySize);
    memset(scratchIdxBuffer, 0, idxMemorySize);
    memset(scratchDstBuffer, 0, expectedMemorySize);
    memset(scratchExpectedMemory, 0, expectedMemorySize);

    LevelZeroBlackBoxTests::prepareScratchTestBuffers(scratchSrcBuffer, scratchIdxBuffer, scratchExpectedMemory, arraySize, vectorSize, expectedMemorySize, srcAdditionalMul);

    // prepare dispatch params of kernels
    uint32_t copyKernelGroupSizeX = 32u;
    uint32_t copyKernelGroupSizeY = 1u;
    uint32_t copyKernelGroupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &copyKernelGroupSizeX, &copyKernelGroupSizeY, &copyKernelGroupSizeZ));

    ze_group_count_t copyKernelDispatchTraits;
    copyKernelDispatchTraits.groupCountX = allocSize / copyKernelGroupSizeX;
    copyKernelDispatchTraits.groupCountY = copyKernelGroupSizeY;
    copyKernelDispatchTraits.groupCountZ = copyKernelGroupSizeZ;

    uint32_t scratchGroupSizeX = arraySize;
    uint32_t scratchGroupSizeY = 1u;
    uint32_t scratchGroupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(scratchKernel, arraySize, 1U, 1U, &scratchGroupSizeX, &scratchGroupSizeY, &scratchGroupSizeZ));

    ze_group_count_t scratchDispatchTraits;
    scratchDispatchTraits.groupCountX = 1u;
    scratchDispatchTraits.groupCountY = 1u;
    scratchDispatchTraits.groupCountZ = 1u;

    if (scratchKernelFirst) {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(scratchKernel, scratchGroupSizeX, scratchGroupSizeY, scratchGroupSizeZ));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(scratchKernel, 2, sizeof(scratchDstBuffer), &scratchDstBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(scratchKernel, 1, sizeof(scratchSrcBuffer), &scratchSrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(scratchKernel, 0, sizeof(scratchIdxBuffer), &scratchIdxBuffer));
    } else {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, copyKernelGroupSizeX, copyKernelGroupSizeY, copyKernelGroupSizeZ));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &copySrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &copyDstBuffer));
    }

    ze_kernel_handle_t firstKernel = scratchKernelFirst ? scratchKernel : copyKernel;
    ze_kernel_handle_t mutableKernel = scratchKernelFirst ? copyKernel : scratchKernel;

    ze_group_count_t *firstDispatchTraits = scratchKernelFirst ? &scratchDispatchTraits : &copyKernelDispatchTraits;
    ze_group_count_t *mutableDispatchTraits = scratchKernelFirst ? &copyKernelDispatchTraits : &scratchDispatchTraits;

    ze_kernel_handle_t kernelGroup[] = {scratchKernel, copyKernel};

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, firstKernel, firstDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (scratchKernelFirst) {
            valid = LevelZeroBlackBoxTests::validate(scratchExpectedMemory, scratchDstBuffer, expectedMemorySize);
            if (!valid) {
                std::cerr << "before mutation - fail scratch operation" << std::endl;
            }
        } else {
            auto expectedCopyValue = val;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedCopyValue, copyDstBuffer, elemSize) == false) {
                std::cerr << "before mutation - fail copy operation" << std::endl;
                valid = false;
            }
        }
    }

    // mutate kernels and replace arguments
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mutableKernel));
    if (scratchKernelFirst) {
        // change into copy kernel arguments
        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &copySrcBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &copyDstBuffer;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = mutableDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = copyKernelGroupSizeX;
        mutateGroupSize.groupSizeY = copyKernelGroupSizeY;
        mutateGroupSize.groupSizeZ = copyKernelGroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelIdxArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelIdxArg.commandId = commandId;
        mutateKernelIdxArg.argIndex = 0;
        mutateKernelIdxArg.argSize = sizeof(void *);
        mutateKernelIdxArg.pArgValue = &scratchIdxBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 1;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &scratchSrcBuffer;
        mutateKernelSrcArg.pNext = &mutateKernelIdxArg;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 2;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &scratchDstBuffer;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = mutableDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = scratchGroupSizeX;
        mutateGroupSize.groupSizeY = scratchGroupSizeY;
        mutateGroupSize.groupSizeZ = scratchGroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (scratchKernelFirst) {
            auto expectedCopyValue = val;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedCopyValue, copyDstBuffer, elemSize) == false) {
                std::cerr << "after mutation - fail copy operation" << std::endl;
                valid = false;
            }
        } else {
            valid = LevelZeroBlackBoxTests::validate(scratchExpectedMemory, scratchDstBuffer, expectedMemorySize);
            if (!valid) {
                std::cerr << "after mutation - fail scratch operation" << std::endl;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copyDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, scratchSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, scratchDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, scratchIdxBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, scratchExpectedMemory));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

std::string testNameMutateKernelsSlm(bool slmKernelFirst) {
    std::ostringstream testStream;
    testStream << "Mutate Kernels with SLM:" << std::endl;
    if (slmKernelFirst) {
        testStream << "SLM kernel first, mutate into copy kernel";
    } else {
        testStream << "copy kernel first, mutate into SLM";
    }
    testStream << " Test";

    return testStream.str();
}

bool testMutateKernelsSlm(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool slmKernelFirst, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");
    ze_kernel_handle_t addSlmKernel = execEnv->getKernelHandle(module, "addScalarSlmKernel");
    {
        ze_kernel_properties_t kernelProperties{ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
        SUCCESS_OR_TERMINATE(zeKernelGetProperties(addSlmKernel, &kernelProperties));
        std::cout << "addScalarSlmKernel SLM size = " << std::dec << kernelProperties.localMemSize << std::endl;
    }

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    // prepare buffers
    void *copySrcBuffer = nullptr;
    void *copyDstBuffer = nullptr;

    void *addSrcBuffer = nullptr;
    void *addDstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copyDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &addSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &addDstBuffer));

    constexpr ElemType val1 = 10;
    constexpr ElemType val2 = 35;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(copySrcBuffer)[i] = val1;
        reinterpret_cast<ElemType *>(addSrcBuffer)[i] = val2;
    }
    memset(copyDstBuffer, 0, allocSize);
    memset(addDstBuffer, 0, allocSize);

    ElemType addScalarValue = 42;

    // prepare dispatch params of kernels
    uint32_t copyKernelGroupSizeX = 32u;
    uint32_t copyKernelGroupSizeY = 1u;
    uint32_t copyKernelGroupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &copyKernelGroupSizeX, &copyKernelGroupSizeY, &copyKernelGroupSizeZ));

    ze_group_count_t copyKernelDispatchTraits;
    copyKernelDispatchTraits.groupCountX = allocSize / copyKernelGroupSizeX;
    copyKernelDispatchTraits.groupCountY = copyKernelGroupSizeY;
    copyKernelDispatchTraits.groupCountZ = copyKernelGroupSizeZ;

    uint32_t addGroupSizeX = elemSize;
    uint32_t addGroupSizeY = 1u;
    uint32_t addGroupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addSlmKernel, elemSize, 1U, 1U, &addGroupSizeX, &addGroupSizeY, &addGroupSizeZ));

    ze_group_count_t addDispatchTraits;
    addDispatchTraits.groupCountX = elemSize / addGroupSizeX;
    addDispatchTraits.groupCountY = addGroupSizeY;
    addDispatchTraits.groupCountZ = addGroupSizeZ;

    if (slmKernelFirst) {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addSlmKernel, addGroupSizeX, addGroupSizeY, addGroupSizeZ));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addSlmKernel, 0, sizeof(addSrcBuffer), &addSrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addSlmKernel, 1, sizeof(addDstBuffer), &addDstBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addSlmKernel, 2, sizeof(addScalarValue), &addScalarValue));
    } else {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, copyKernelGroupSizeX, copyKernelGroupSizeY, copyKernelGroupSizeZ));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &copySrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &copyDstBuffer));
    }

    ze_kernel_handle_t firstKernel = slmKernelFirst ? addSlmKernel : copyKernel;
    ze_kernel_handle_t mutableKernel = slmKernelFirst ? copyKernel : addSlmKernel;

    ze_group_count_t *firstDispatchTraits = slmKernelFirst ? &addDispatchTraits : &copyKernelDispatchTraits;
    ze_group_count_t *mutableDispatchTraits = slmKernelFirst ? &copyKernelDispatchTraits : &addDispatchTraits;

    ze_kernel_handle_t kernelGroup[] = {addSlmKernel, copyKernel};

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, firstKernel, firstDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (slmKernelFirst) {
            auto expectedAddValue = val2 + addScalarValue;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedAddValue, addDstBuffer, elemSize) == false) {
                std::cerr << "before mutation - fail add slm operation" << std::endl;
                valid = false;
            }
        } else {
            auto expectedCopyValue = val1;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedCopyValue, copyDstBuffer, elemSize) == false) {
                std::cerr << "before mutation - fail copy operation" << std::endl;
                valid = false;
            }
        }
    }

    // mutate kernels and replace arguments
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mutableKernel));
    if (slmKernelFirst) {
        // change into copy kernel arguments
        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &copySrcBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &copyDstBuffer;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = mutableDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = copyKernelGroupSizeX;
        mutateGroupSize.groupSizeY = copyKernelGroupSizeY;
        mutateGroupSize.groupSizeZ = copyKernelGroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelIdxArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelIdxArg.commandId = commandId;
        mutateKernelIdxArg.argIndex = 0;
        mutateKernelIdxArg.argSize = sizeof(void *);
        mutateKernelIdxArg.pArgValue = &addSrcBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 1;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &addDstBuffer;
        mutateKernelSrcArg.pNext = &mutateKernelIdxArg;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 2;
        mutateKernelDstArg.argSize = sizeof(addScalarValue);
        mutateKernelDstArg.pArgValue = &addScalarValue;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = mutableDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = addGroupSizeX;
        mutateGroupSize.groupSizeY = addGroupSizeY;
        mutateGroupSize.groupSizeZ = addGroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (slmKernelFirst) {
            auto expectedCopyValue = val1;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedCopyValue, copyDstBuffer, elemSize) == false) {
                std::cerr << "after mutation - fail copy operation" << std::endl;
                valid = false;
            }
        } else {
            auto expectedAddValue = val2 + addScalarValue;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedAddValue, addDstBuffer, elemSize) == false) {
                std::cerr << "after mutation - fail add slm operation" << std::endl;
                valid = false;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copyDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, addSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, addDstBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);
    execEnv->destroyKernelHandle(addSlmKernel);

    return valid;
}

std::string testNameMutateKernelsPrivateMemory(bool privateMemoryKernelFirst) {
    std::ostringstream testStream;
    testStream << "Mutate Kernels with Private Memory:" << std::endl;
    if (privateMemoryKernelFirst) {
        testStream << "private memory kernel first, mutate into copy kernel";
    } else {
        testStream << "copy kernel first, mutate into private memory";
    }
    testStream << " Test";

    return testStream.str();
}

bool testMutateKernelsPrivateMemory(MclTests::ExecEnv *execEnv, ze_module_handle_t module, ze_kernel_handle_t privateMemoryKernel, bool privateMemoryKernelFirst, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    // prepare buffers
    void *copySrcBuffer = nullptr;
    void *copyDstBuffer = nullptr;

    void *privateSrcBuffer = nullptr;
    void *privateDstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copyDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &privateSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &privateDstBuffer));

    constexpr ElemType copySrcVal = 10;
    constexpr ElemType privateSrcVal = 35;

    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(copySrcBuffer)[i] = copySrcVal;
        reinterpret_cast<ElemType *>(privateSrcBuffer)[i] = privateSrcVal;
    }
    memset(copyDstBuffer, 0, allocSize);
    memset(privateDstBuffer, 0, allocSize);

    constexpr ElemType privateKernelArraySize = 256;
    ElemType privateScalarValue = 5;

    ElemType expectedPrivateArrayOperationValue = 0;
    ElemType loopCount = privateScalarValue > privateKernelArraySize ? privateKernelArraySize : privateScalarValue;
    for (ElemType i = 0; i < loopCount; i++) {
        expectedPrivateArrayOperationValue += (privateSrcVal + privateScalarValue + i);
    }

    // prepare dispatch params of kernels
    uint32_t copyKernelGroupSizeX = 32u;
    uint32_t copyKernelGroupSizeY = 1u;
    uint32_t copyKernelGroupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &copyKernelGroupSizeX, &copyKernelGroupSizeY, &copyKernelGroupSizeZ));

    ze_group_count_t copyKernelDispatchTraits;
    copyKernelDispatchTraits.groupCountX = allocSize / copyKernelGroupSizeX;
    copyKernelDispatchTraits.groupCountY = copyKernelGroupSizeY;
    copyKernelDispatchTraits.groupCountZ = copyKernelGroupSizeZ;

    uint32_t privateGroupSizeX = elemSize;
    uint32_t privateGroupSizeY = 1u;
    uint32_t privateGroupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(privateMemoryKernel, elemSize, 1U, 1U, &privateGroupSizeX, &privateGroupSizeY, &privateGroupSizeZ));

    ze_group_count_t privateDispatchTraits;
    privateDispatchTraits.groupCountX = elemSize / privateGroupSizeX;
    privateDispatchTraits.groupCountY = privateGroupSizeY;
    privateDispatchTraits.groupCountZ = privateGroupSizeZ;

    if (privateMemoryKernelFirst) {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(privateMemoryKernel, privateGroupSizeX, privateGroupSizeY, privateGroupSizeZ));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(privateMemoryKernel, 0, sizeof(privateSrcBuffer), &privateSrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(privateMemoryKernel, 1, sizeof(privateDstBuffer), &privateDstBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(privateMemoryKernel, 2, sizeof(privateScalarValue), &privateScalarValue));
    } else {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, copyKernelGroupSizeX, copyKernelGroupSizeY, copyKernelGroupSizeZ));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &copySrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &copyDstBuffer));
    }

    ze_kernel_handle_t firstKernel = privateMemoryKernelFirst ? privateMemoryKernel : copyKernel;
    ze_kernel_handle_t mutableKernel = privateMemoryKernelFirst ? copyKernel : privateMemoryKernel;

    ze_group_count_t *firstDispatchTraits = privateMemoryKernelFirst ? &privateDispatchTraits : &copyKernelDispatchTraits;
    ze_group_count_t *mutableDispatchTraits = privateMemoryKernelFirst ? &copyKernelDispatchTraits : &privateDispatchTraits;

    ze_kernel_handle_t kernelGroup[] = {privateMemoryKernel, copyKernel};

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, firstKernel, firstDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (privateMemoryKernelFirst) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cout << "Private memory kernel output = " << expectedPrivateArrayOperationValue << std::endl;
            }
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedPrivateArrayOperationValue, privateDstBuffer, elemSize) == false) {
                std::cerr << "before mutation - fail private array operation" << std::endl;
                valid = false;
            }
        } else {
            auto expectedCopyValue = copySrcVal;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedCopyValue, copyDstBuffer, elemSize) == false) {
                std::cerr << "before mutation - fail copy operation" << std::endl;
                valid = false;
            }
        }
    }

    // mutate kernels and replace arguments
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mutableKernel));
    if (privateMemoryKernelFirst) {
        // change into copy kernel arguments
        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &copySrcBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &copyDstBuffer;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = mutableDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = copyKernelGroupSizeX;
        mutateGroupSize.groupSizeY = copyKernelGroupSizeY;
        mutateGroupSize.groupSizeZ = copyKernelGroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelIdxArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelIdxArg.commandId = commandId;
        mutateKernelIdxArg.argIndex = 0;
        mutateKernelIdxArg.argSize = sizeof(void *);
        mutateKernelIdxArg.pArgValue = &privateSrcBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 1;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &privateDstBuffer;
        mutateKernelSrcArg.pNext = &mutateKernelIdxArg;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 2;
        mutateKernelDstArg.argSize = sizeof(privateScalarValue);
        mutateKernelDstArg.pArgValue = &privateScalarValue;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = mutableDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = privateGroupSizeX;
        mutateGroupSize.groupSizeY = privateGroupSizeY;
        mutateGroupSize.groupSizeZ = privateGroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (privateMemoryKernelFirst) {
            auto expectedCopyValue = copySrcVal;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedCopyValue, copyDstBuffer, elemSize) == false) {
                std::cerr << "after mutation - fail copy operation" << std::endl;
                valid = false;
            }
        } else {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cout << "Private memory kernel output = " << expectedPrivateArrayOperationValue << std::endl;
            }
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedPrivateArrayOperationValue, privateDstBuffer, elemSize) == false) {
                std::cerr << "after mutation - fail private array operation" << std::endl;
                valid = false;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copyDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, privateSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, privateDstBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

std::string testNameMutateKernelsLocalWorkSize(bool localWorkSizeKernelFirst, uint32_t *localWorkSizeKernelGroupSize, uint32_t *addScalarLinearKernelGroupSize) {
    std::ostringstream testStream;
    testStream << "Mutate Kernels with Local Work Size check:" << std::endl;
    if (localWorkSizeKernelFirst) {
        testStream << "local work size kernel " << localWorkSizeKernelGroupSize[0]
                   << " " << localWorkSizeKernelGroupSize[1]
                   << " " << localWorkSizeKernelGroupSize[2]
                   << " mutate into linear add scalar kernel "
                   << addScalarLinearKernelGroupSize[0]
                   << " " << addScalarLinearKernelGroupSize[1]
                   << " " << addScalarLinearKernelGroupSize[2];
    } else {
        testStream << "linear add scalar kernel " << addScalarLinearKernelGroupSize[0]
                   << " " << addScalarLinearKernelGroupSize[1]
                   << " " << addScalarLinearKernelGroupSize[2]
                   << " mutate into local work size "
                   << localWorkSizeKernelGroupSize[0]
                   << " " << localWorkSizeKernelGroupSize[1]
                   << " " << localWorkSizeKernelGroupSize[2];
    }
    testStream << " Test";

    return testStream.str();
}

bool testMutateKernelsLocalWorkSize(MclTests::ExecEnv *execEnv, ze_module_handle_t module, uint32_t *localWorkSizeKernelGroupSize, uint32_t *addScalarLinearKernelGroupSize, bool localWorkSizeKernelFirst, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 1024;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);
    size_t processSize = addScalarLinearKernelGroupSize[0] * addScalarLinearKernelGroupSize[1] * addScalarLinearKernelGroupSize[2];

    ze_kernel_handle_t lwsKernel = execEnv->getKernelHandle(module, "getLocalWorkSize");
    ze_kernel_handle_t addLinearKernel = execEnv->getKernelHandle(module, "addScalarKernelLinear");

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    void *addSrcBuffer = nullptr;
    void *addDstBuffer = nullptr;
    void *lwsDstBuffer = nullptr;
    void *lwsDataBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &lwsDataBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &lwsDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &addSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &addDstBuffer));

    constexpr ElemType val1 = 10;
    for (uint32_t i = 0; i < processSize; i++) {
        reinterpret_cast<ElemType *>(addSrcBuffer)[i] = val1;
    }
    memset(addDstBuffer, 0, allocSize);
    memset(lwsDstBuffer, 0, allocSize);

    ElemType scalarValue = 0x2;

    if (localWorkSizeKernelFirst) {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(lwsKernel, localWorkSizeKernelGroupSize[0], localWorkSizeKernelGroupSize[1], localWorkSizeKernelGroupSize[2]));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(lwsKernel, 0, sizeof(lwsDataBuffer), &lwsDataBuffer));
    } else {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addLinearKernel, addScalarLinearKernelGroupSize[0], addScalarLinearKernelGroupSize[1], addScalarLinearKernelGroupSize[2]));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addLinearKernel, 0, sizeof(void *), &addSrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addLinearKernel, 1, sizeof(void *), &addDstBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addLinearKernel, 2, sizeof(scalarValue), &scalarValue));
    }

    ze_kernel_handle_t firstKernel = localWorkSizeKernelFirst ? lwsKernel : addLinearKernel;
    ze_kernel_handle_t mutableKernel = localWorkSizeKernelFirst ? addLinearKernel : lwsKernel;

    ze_kernel_handle_t kernelGroup[] = {addLinearKernel, lwsKernel};

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1;
    dispatchTraits.groupCountY = 1;
    dispatchTraits.groupCountZ = 1;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, firstKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        if (localWorkSizeKernelFirst) {
            // copy from device memory into host
            SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(execEnv->immCmdList, lwsDstBuffer, lwsDataBuffer, allocSize, nullptr, 0, nullptr));
            uint64_t *outputData = reinterpret_cast<uint64_t *>(lwsDstBuffer);
            valid &= MclTests::validateGroupSizeData(outputData, localWorkSizeKernelGroupSize, true);
        } else {
            auto expectedValue = val1 + scalarValue;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, addDstBuffer, processSize) == false) {
                std::cerr << "before mutation - add linear kernel" << std::endl;
                valid = false;
            }
        }
    }

    // mutate kernels and replace arguments
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mutableKernel));
    if (localWorkSizeKernelFirst) {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &addSrcBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &addDstBuffer;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelScalarArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelScalarArg.commandId = commandId;
        mutateKernelScalarArg.argIndex = 2;
        mutateKernelScalarArg.argSize = sizeof(ElemType);
        mutateKernelScalarArg.pArgValue = &scalarValue;
        mutateKernelScalarArg.pNext = &mutateKernelDstArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = &dispatchTraits;
        mutateGroupCount.pNext = &mutateKernelScalarArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = addScalarLinearKernelGroupSize[0];
        mutateGroupSize.groupSizeY = addScalarLinearKernelGroupSize[1];
        mutateGroupSize.groupSizeZ = addScalarLinearKernelGroupSize[2];
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 0;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &lwsDataBuffer;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = &dispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = localWorkSizeKernelGroupSize[0];
        mutateGroupSize.groupSizeY = localWorkSizeKernelGroupSize[1];
        mutateGroupSize.groupSizeZ = localWorkSizeKernelGroupSize[2];
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (localWorkSizeKernelFirst) {
            auto expectedValue = val1 + scalarValue;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, addDstBuffer, processSize) == false) {
                std::cerr << "after mutation - add linear kernel" << std::endl;
                valid = false;
            }
        } else {
            // copy from device memory into host
            SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(execEnv->immCmdList, lwsDstBuffer, lwsDataBuffer, allocSize, nullptr, 0, nullptr));
            uint64_t *outputData = reinterpret_cast<uint64_t *>(lwsDstBuffer);
            valid &= MclTests::validateGroupSizeData(outputData, localWorkSizeKernelGroupSize, false);
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, lwsDataBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, lwsDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, addSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, addDstBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(lwsKernel);
    execEnv->destroyKernelHandle(addLinearKernel);

    return valid;
}

std::string testNameMutateKernelsGlobalWorkSize(bool globalWorkSizeKernelFirst, uint32_t *globalWorkSizeKernelGroupCount, uint32_t *addScalarLinearKernelGroupCount) {
    std::ostringstream testStream;
    testStream << "Mutate Kernels with Global Work Size check:" << std::endl;
    if (globalWorkSizeKernelFirst) {
        testStream << "global work size kernel " << globalWorkSizeKernelGroupCount[0]
                   << " " << globalWorkSizeKernelGroupCount[1]
                   << " " << globalWorkSizeKernelGroupCount[2]
                   << " mutate into linear add scalar kernel "
                   << addScalarLinearKernelGroupCount[0]
                   << " " << addScalarLinearKernelGroupCount[1]
                   << " " << addScalarLinearKernelGroupCount[2];
    } else {
        testStream << "linear add scalar kernel " << addScalarLinearKernelGroupCount[0]
                   << " " << addScalarLinearKernelGroupCount[1]
                   << " " << addScalarLinearKernelGroupCount[2]
                   << " mutate into global work size "
                   << globalWorkSizeKernelGroupCount[0]
                   << " " << globalWorkSizeKernelGroupCount[1]
                   << " " << globalWorkSizeKernelGroupCount[2];
    }
    testStream << " Test";

    return testStream.str();
}

bool testMutateKernelsGlobalWorkSize(MclTests::ExecEnv *execEnv, ze_module_handle_t module, uint32_t *globalWorkSizeKernelGroupCount, uint32_t *addScalarLinearKernelGroupCount, bool globalWorkSizeKernelFirst, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 1024;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);
    size_t processSize = addScalarLinearKernelGroupCount[0] * addScalarLinearKernelGroupCount[1] * addScalarLinearKernelGroupCount[2];

    ze_kernel_handle_t gwsKernel = execEnv->getKernelHandle(module, "getGlobalWorkSize");
    ze_kernel_handle_t addLinearKernel = execEnv->getKernelHandle(module, "addScalarKernelLinear");

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    void *addSrcBuffer = nullptr;
    void *addDstBuffer = nullptr;
    void *gwsDstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &gwsDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &addSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &addDstBuffer));

    constexpr ElemType val1 = 31;
    for (uint32_t i = 0; i < processSize; i++) {
        reinterpret_cast<ElemType *>(addSrcBuffer)[i] = val1;
    }
    memset(addDstBuffer, 0, allocSize);
    memset(gwsDstBuffer, 0, allocSize);

    ElemType scalarValue = 0x5;

    if (globalWorkSizeKernelFirst) {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(gwsKernel, 1, 1, 1));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(gwsKernel, 0, sizeof(gwsDstBuffer), &gwsDstBuffer));
    } else {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addLinearKernel, 1, 1, 1));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addLinearKernel, 0, sizeof(void *), &addSrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addLinearKernel, 1, sizeof(void *), &addDstBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addLinearKernel, 2, sizeof(scalarValue), &scalarValue));
    }

    ze_kernel_handle_t firstKernel = globalWorkSizeKernelFirst ? gwsKernel : addLinearKernel;
    ze_kernel_handle_t mutableKernel = globalWorkSizeKernelFirst ? addLinearKernel : gwsKernel;

    ze_kernel_handle_t kernelGroup[] = {addLinearKernel, gwsKernel};

    ze_group_count_t gwsDispatchTraits;
    gwsDispatchTraits.groupCountX = globalWorkSizeKernelGroupCount[0];
    gwsDispatchTraits.groupCountY = globalWorkSizeKernelGroupCount[1];
    gwsDispatchTraits.groupCountZ = globalWorkSizeKernelGroupCount[2];

    ze_group_count_t addScalarDispatchTraits;
    addScalarDispatchTraits.groupCountX = addScalarLinearKernelGroupCount[0];
    addScalarDispatchTraits.groupCountY = addScalarLinearKernelGroupCount[1];
    addScalarDispatchTraits.groupCountZ = addScalarLinearKernelGroupCount[2];

    ze_group_count_t *firstDispatchTraits = globalWorkSizeKernelFirst ? &gwsDispatchTraits : &addScalarDispatchTraits;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, firstKernel, firstDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        if (globalWorkSizeKernelFirst) {
            uint64_t *outputData = reinterpret_cast<uint64_t *>(gwsDstBuffer);
            valid &= MclTests::validateGroupCountData(outputData, globalWorkSizeKernelGroupCount, true);
        } else {
            auto expectedValue = val1 + scalarValue;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, addDstBuffer, processSize) == false) {
                std::cerr << "before mutation - add linear kernel" << std::endl;
                valid = false;
            }
        }
    }

    // mutate kernels and replace arguments
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mutableKernel));
    if (globalWorkSizeKernelFirst) {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &addSrcBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &addDstBuffer;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelScalarArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelScalarArg.commandId = commandId;
        mutateKernelScalarArg.argIndex = 2;
        mutateKernelScalarArg.argSize = sizeof(ElemType);
        mutateKernelScalarArg.pArgValue = &scalarValue;
        mutateKernelScalarArg.pNext = &mutateKernelDstArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = &addScalarDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelScalarArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = 1;
        mutateGroupSize.groupSizeY = 1;
        mutateGroupSize.groupSizeZ = 1;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 0;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &gwsDstBuffer;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = &gwsDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = 1;
        mutateGroupSize.groupSizeY = 1;
        mutateGroupSize.groupSizeZ = 1;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (globalWorkSizeKernelFirst) {
            auto expectedValue = val1 + scalarValue;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, addDstBuffer, processSize) == false) {
                std::cerr << "after mutation - add linear kernel" << std::endl;
                valid = false;
            }
        } else {
            uint64_t *outputData = reinterpret_cast<uint64_t *>(gwsDstBuffer);
            valid &= MclTests::validateGroupCountData(outputData, globalWorkSizeKernelGroupCount, false);
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, gwsDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, addSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, addDstBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(gwsKernel);
    execEnv->destroyKernelHandle(addLinearKernel);

    return valid;
}

std::string testNameMutateKernelsGlobalOffset(bool globalOffsetKernelFirst, uint32_t *globalOffsets) {
    std::ostringstream testStream;
    testStream << "Mutate Kernels with Global Offset check:" << std::endl;
    if (globalOffsetKernelFirst) {
        testStream << "global offset kernel " << globalOffsets[0]
                   << " " << globalOffsets[1]
                   << " " << globalOffsets[2]
                   << " mutate into copy kernel";
    } else {
        testStream << "copy kernel"
                   << " mutate into global offset kernel "
                   << globalOffsets[0]
                   << " " << globalOffsets[1]
                   << " " << globalOffsets[2];
    }
    testStream << " Test";

    return testStream.str();
}

bool testMutateKernelsGlobalOffset(MclTests::ExecEnv *execEnv, ze_module_handle_t module, uint32_t *globalOffsets, bool globalOffsetKernelFirst, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 64;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);
    constexpr size_t offsetAllocSize = 1024;

    ze_kernel_handle_t globalOffsetKernel = execEnv->getKernelHandle(module, "getGlobaOffset");
    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    void *copySrcBuffer = nullptr;
    void *copyDstBuffer = nullptr;
    void *offsetDstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, offsetAllocSize, 1, &offsetDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copyDstBuffer));

    constexpr ElemType val1 = 31;
    for (uint32_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(copySrcBuffer)[i] = val1;
    }
    memset(copyDstBuffer, 0, allocSize);
    memset(offsetDstBuffer, 0, offsetAllocSize);

    uint32_t copyGroupSizeX = 32u;
    uint32_t copyGroupSizeY = 1u;
    uint32_t copyGroupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &copyGroupSizeX, &copyGroupSizeY, &copyGroupSizeZ));

    ze_group_count_t copyDispatchTraits;
    copyDispatchTraits.groupCountX = allocSize / copyGroupSizeX;
    copyDispatchTraits.groupCountY = 1;
    copyDispatchTraits.groupCountZ = 1;

    uint32_t startGroupCount[] = {4, 4, 4};
    ze_group_count_t offsetDispatchTraits;
    offsetDispatchTraits.groupCountX = startGroupCount[0];
    offsetDispatchTraits.groupCountY = startGroupCount[1];
    offsetDispatchTraits.groupCountZ = startGroupCount[2];

    if (globalOffsetKernelFirst) {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(globalOffsetKernel, 1, 1, 1));
        SUCCESS_OR_TERMINATE(zeKernelSetGlobalOffsetExp(globalOffsetKernel, globalOffsets[0], globalOffsets[1], globalOffsets[2]));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(globalOffsetKernel, 0, sizeof(offsetDstBuffer), &offsetDstBuffer));
    } else {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, copyGroupSizeX, copyGroupSizeY, copyGroupSizeZ));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &copySrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &copyDstBuffer));
    }

    ze_kernel_handle_t firstKernel = globalOffsetKernelFirst ? globalOffsetKernel : copyKernel;
    ze_kernel_handle_t mutableKernel = globalOffsetKernelFirst ? copyKernel : globalOffsetKernel;

    ze_kernel_handle_t kernelGroup[] = {copyKernel, globalOffsetKernel};

    ze_group_count_t *firstDispatchTraits = globalOffsetKernelFirst ? &offsetDispatchTraits : &copyDispatchTraits;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, firstKernel, firstDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        if (globalOffsetKernelFirst) {
            uint64_t *outputData = reinterpret_cast<uint64_t *>(offsetDstBuffer);
            valid &= MclTests::validateGlobalOffsetData(outputData, globalOffsets, startGroupCount, true);
        } else {
            auto expectedValue = val1;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, copyDstBuffer, elemSize) == false) {
                std::cerr << "before mutation - copy kernel" << std::endl;
                valid = false;
            }
        }
    }

    // mutate kernels and replace arguments
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mutableKernel));
    if (globalOffsetKernelFirst) {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &copySrcBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &copyDstBuffer;
        mutateKernelDstArg.pNext = &mutateKernelSrcArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = &copyDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = copyGroupSizeX;
        mutateGroupSize.groupSizeY = copyGroupSizeY;
        mutateGroupSize.groupSizeZ = copyGroupSizeZ;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 0;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &offsetDstBuffer;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = &offsetDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.groupSizeX = 1;
        mutateGroupSize.groupSizeY = 1;
        mutateGroupSize.groupSizeZ = 1;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_global_offset_exp_desc_t mutateGlobalOffset = {ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXP_DESC};
        mutateGlobalOffset.commandId = commandId;
        mutateGlobalOffset.offsetX = globalOffsets[0];
        mutateGlobalOffset.offsetY = globalOffsets[1];
        mutateGlobalOffset.offsetZ = globalOffsets[2];
        mutateGlobalOffset.pNext = &mutateGroupSize;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGlobalOffset;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (globalOffsetKernelFirst) {
            auto expectedValue = val1;
            if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, copyDstBuffer, elemSize) == false) {
                std::cerr << "after mutation - copy kernel" << std::endl;
                valid = false;
            }
        } else {
            uint64_t *outputData = reinterpret_cast<uint64_t *>(offsetDstBuffer);
            valid &= MclTests::validateGlobalOffsetData(outputData, globalOffsets, startGroupCount, false);
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, offsetDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copyDstBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(globalOffsetKernel);
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

std::string testNameSameKernelsMutateArgumentsShapesEvents(uint32_t *initGroupSize, uint32_t *mutateGroupSize, MclTests::EventOptions eventOptions) {
    std::ostringstream testStream;
    testStream << "- same kernels, mutate kernel arguments, shapes, events" << std::endl;
    testStream << "initial group size " << initGroupSize[0]
               << " " << initGroupSize[1]
               << " " << initGroupSize[2] << std::endl;
    testStream << "mutate group size " << mutateGroupSize[0]
               << " " << mutateGroupSize[1]
               << " " << mutateGroupSize[2] << std::endl;
    MclTests::setEventTestStream(eventOptions, testStream);
    testStream << " Test";

    return testStream.str();
}

bool testSameKernelsMutateArgumentsShapesEvents(MclTests::ExecEnv *execEnv, ze_module_handle_t module, uint32_t *initGroupSize, uint32_t *mutateGroupSize, MclTests::EventOptions eventOptions, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 1024;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    bool isCbEvent = MclTests::isCbEvent(eventOptions);
    bool isTsEvent = MclTests::isTimestampEvent(eventOptions);

    ze_kernel_handle_t lwsKernel = execEnv->getKernelHandle(module, "getLocalWorkSize");
    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> events(numEvents);
    std::vector<ze_kernel_timestamp_result_t> eventTimestamps(numEvents);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, isCbEvent, false);

    void *outputBuffer1 = nullptr;
    void *lwsDstBuffer1 = nullptr;
    void *outputBuffer2 = nullptr;
    void *lwsDstBuffer2 = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &outputBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &outputBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &lwsDstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &lwsDstBuffer2));

    memset(outputBuffer1, 0, allocSize);
    memset(outputBuffer2, 0, allocSize);

    ze_group_count_t lwsDispatchTraits;
    lwsDispatchTraits.groupCountX = 1;
    lwsDispatchTraits.groupCountY = 1;
    lwsDispatchTraits.groupCountZ = 1;

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(lwsKernel, initGroupSize[0], initGroupSize[1], initGroupSize[2]));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(lwsKernel, 0, sizeof(lwsDstBuffer1), &lwsDstBuffer1));

    uint32_t copyKernelGroupSizeX = 32;
    uint32_t copyKernelGroupSizeY = 1;
    uint32_t copyKernelGroupSizeZ = 1;

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &copyKernelGroupSizeX, &copyKernelGroupSizeY, &copyKernelGroupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, copyKernelGroupSizeX, copyKernelGroupSizeY, copyKernelGroupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &lwsDstBuffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &outputBuffer1));

    ze_group_count_t copyKernelDispatchTraits;
    copyKernelDispatchTraits.groupCountX = allocSize / copyKernelGroupSizeX;
    copyKernelDispatchTraits.groupCountY = copyKernelGroupSizeY;
    copyKernelDispatchTraits.groupCountZ = copyKernelGroupSizeZ;

    ze_kernel_handle_t kernelGroup[] = {copyKernel, lwsKernel};

    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;

    uint64_t commandIdLws = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandIdLws));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, lwsKernel, &lwsDispatchTraits, events[0], 0, nullptr));

    uint64_t commandIdCopy = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandIdCopy));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &copyKernelDispatchTraits, nullptr, 1, &events[0]));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        uint64_t *outputData = reinterpret_cast<uint64_t *>(outputBuffer1);
        valid &= MclTests::validateGroupSizeData(outputData, initGroupSize, true);

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[0]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[0], &eventTimestamps[0]));
        }
        if (!isCbEvent) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(events[0]));
        }
    }

    ze_mutable_kernel_argument_exp_desc_t mutateKernelCopySrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelCopySrcArg.commandId = commandIdCopy;
    mutateKernelCopySrcArg.argIndex = 0;
    mutateKernelCopySrcArg.argSize = sizeof(void *);
    mutateKernelCopySrcArg.pArgValue = &lwsDstBuffer2;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelCopyDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelCopyDstArg.commandId = commandIdCopy;
    mutateKernelCopyDstArg.argIndex = 1;
    mutateKernelCopyDstArg.argSize = sizeof(void *);
    mutateKernelCopyDstArg.pArgValue = &outputBuffer2;
    mutateKernelCopyDstArg.pNext = &mutateKernelCopySrcArg;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelLwsDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelLwsDstArg.commandId = commandIdLws;
    mutateKernelLwsDstArg.argIndex = 0;
    mutateKernelLwsDstArg.argSize = sizeof(void *);
    mutateKernelLwsDstArg.pArgValue = &lwsDstBuffer2;
    mutateKernelLwsDstArg.pNext = &mutateKernelCopyDstArg;

    ze_mutable_group_size_exp_desc_t mutateKernelLwsGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    mutateKernelLwsGroupSize.commandId = commandIdLws;
    mutateKernelLwsGroupSize.groupSizeX = mutateGroupSize[0];
    mutateKernelLwsGroupSize.groupSizeY = mutateGroupSize[1];
    mutateKernelLwsGroupSize.groupSizeZ = mutateGroupSize[2];
    mutateKernelLwsGroupSize.pNext = &mutateKernelLwsDstArg;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateKernelLwsGroupSize;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandIdLws, events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandIdCopy, 1, &events[1]));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        uint64_t *outputData = reinterpret_cast<uint64_t *>(outputBuffer2);
        valid &= MclTests::validateGroupSizeData(outputData, mutateGroupSize, false);

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[1]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[1], &eventTimestamps[1]));

            if (eventTimestamps[0].context.kernelEnd > eventTimestamps[1].context.kernelEnd) {
                std::cerr << "eventTimestamps[0] greater than eventTimestamps[1]" << std::endl;
                valid = false;
            }
        }
    }

    for (auto &event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, lwsDstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, lwsDstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, outputBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, outputBuffer2));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(lwsKernel);
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

std::string testNameDifferentKernelsMutateArgumentsShapesEvents(bool globalOffsetFirst, uint32_t *groupCount, uint32_t *globalOffset, MclTests::EventOptions eventOptions) {
    std::ostringstream testStream;

    auto globalOffsetLambda = [&testStream, &globalOffset]() {
        testStream << "Global offset with offsets "
                   << globalOffset[0]
                   << " " << globalOffset[1]
                   << " " << globalOffset[2] << std::endl;
    };

    auto groupCountLambda = [&testStream, &groupCount]() {
        testStream << "Group count with shape "
                   << groupCount[0]
                   << " " << groupCount[1]
                   << " " << groupCount[2] << std::endl;
    };

    testStream << "- mutate kernels, kernel arguments, shapes, events" << std::endl;
    testStream << "First ";
    if (globalOffsetFirst) {
        globalOffsetLambda();
    } else {
        groupCountLambda();
    }
    testStream << "Mutate ";
    if (globalOffsetFirst) {
        groupCountLambda();
    } else {
        globalOffsetLambda();
    }

    MclTests::setEventTestStream(eventOptions, testStream);
    testStream << " Test";

    return testStream.str();
}

bool testDifferentKernelsMutateArgumentsShapesEvents(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool globalOffsetFirst, uint32_t *groupCount, uint32_t *globalOffset, MclTests::EventOptions eventOptions, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 1024;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    bool isCbEvent = MclTests::isCbEvent(eventOptions);
    bool isTsEvent = MclTests::isTimestampEvent(eventOptions);

    ze_kernel_handle_t gwsKernel = execEnv->getKernelHandle(module, "getGlobalWorkSize");
    ze_kernel_handle_t offsetKernel = execEnv->getKernelHandle(module, "getGlobaOffset");
    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> events(numEvents);
    std::vector<ze_kernel_timestamp_result_t> eventTimestamps(numEvents);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), eventOptions);

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, isCbEvent, false);

    void *gwsDstBuffer = nullptr;
    void *offsetDstBuffer = nullptr;

    void *outputBuffer1 = nullptr;
    void *outputBuffer2 = nullptr;

    void *patternBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &patternBuffer));

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &outputBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &outputBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &gwsDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &offsetDstBuffer));

    // reset offset destination buffers on the GPU first - synchronous immediate for simplicity
    uint64_t *zeroPtr = reinterpret_cast<uint64_t *>(patternBuffer);
    *zeroPtr = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, offsetDstBuffer, zeroPtr, sizeof(uint64_t), allocSize, nullptr, 0, nullptr));
    uint64_t *tenPtr = zeroPtr + 1;
    *tenPtr = 10;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, gwsDstBuffer, tenPtr, sizeof(uint64_t), allocSize, nullptr, 0, nullptr));

    memset(outputBuffer1, 0, allocSize);
    memset(outputBuffer2, 0, allocSize);

    uint32_t startGroupCount[] = {4, 4, 4};
    ze_group_count_t offsetDispatchTraits;
    offsetDispatchTraits.groupCountX = startGroupCount[0];
    offsetDispatchTraits.groupCountY = startGroupCount[1];
    offsetDispatchTraits.groupCountZ = startGroupCount[2];

    ze_group_count_t gwsDispatchTraits;
    gwsDispatchTraits.groupCountX = groupCount[0];
    gwsDispatchTraits.groupCountY = groupCount[1];
    gwsDispatchTraits.groupCountZ = groupCount[2];

    if (globalOffsetFirst) {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(offsetKernel, 1, 1, 1));
        SUCCESS_OR_TERMINATE(zeKernelSetGlobalOffsetExp(offsetKernel, globalOffset[0], globalOffset[1], globalOffset[2]));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(offsetKernel, 0, sizeof(offsetDstBuffer), &offsetDstBuffer));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &offsetDstBuffer));
    } else {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(gwsKernel, 1, 1, 1));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(gwsKernel, 0, sizeof(gwsDstBuffer), &gwsDstBuffer));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &gwsDstBuffer));
    }

    uint32_t copyKernelGroupSizeX = 32;
    uint32_t copyKernelGroupSizeY = 1;
    uint32_t copyKernelGroupSizeZ = 1;

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &copyKernelGroupSizeX, &copyKernelGroupSizeY, &copyKernelGroupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, copyKernelGroupSizeX, copyKernelGroupSizeY, copyKernelGroupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &outputBuffer1));

    ze_group_count_t copyKernelDispatchTraits;
    copyKernelDispatchTraits.groupCountX = allocSize / copyKernelGroupSizeX;
    copyKernelDispatchTraits.groupCountY = copyKernelGroupSizeY;
    copyKernelDispatchTraits.groupCountZ = copyKernelGroupSizeZ;

    ze_kernel_handle_t kernelGroup[] = {offsetKernel, gwsKernel};
    ze_kernel_handle_t firstKernel = globalOffsetFirst ? offsetKernel : gwsKernel;
    ze_kernel_handle_t mutableKernel = globalOffsetFirst ? gwsKernel : offsetKernel;

    ze_group_count_t *firstDispatchTraits = globalOffsetFirst ? &offsetDispatchTraits : &gwsDispatchTraits;

    ze_mutable_command_id_exp_desc_t commandIdDescMutableKernel = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDescMutableKernel.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                                       ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                                       ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                                       ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET |
                                       ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT |
                                       ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;

    uint64_t commandIdMutableKernel = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDescMutableKernel, 2, kernelGroup, &commandIdMutableKernel));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, firstKernel, firstDispatchTraits, events[0], 0, nullptr));

    ze_mutable_command_id_exp_desc_t commandIdDescCopy = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDescMutableKernel.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                                       ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS;

    uint64_t commandIdCopy = 0;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDescCopy, &commandIdCopy));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &copyKernelDispatchTraits, nullptr, 1, &events[0]));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        uint64_t *outputData = reinterpret_cast<uint64_t *>(outputBuffer1);
        if (globalOffsetFirst) {
            valid &= MclTests::validateGlobalOffsetData(outputData, globalOffset, startGroupCount, true);
        } else {
            valid &= MclTests::validateGroupCountData(outputData, groupCount, true);
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[0]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[0], &eventTimestamps[0]));
        }
        if (!isCbEvent) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(events[0]));
        }
    }

    // mutate events - can be done before kernel mutation
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandSignalEventExp(cmdList, commandIdMutableKernel, events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandWaitEventsExp(cmdList, commandIdCopy, 1, &events[1]));
    // mutate first kernel
    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandIdMutableKernel, &mutableKernel));
    if (globalOffsetFirst) {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandIdMutableKernel;
        mutateKernelDstArg.argIndex = 0;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &gwsDstBuffer;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandIdMutableKernel;
        mutateGroupCount.pGroupCount = &gwsDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandIdMutableKernel;
        mutateGroupSize.groupSizeX = 1;
        mutateGroupSize.groupSizeY = 1;
        mutateGroupSize.groupSizeZ = 1;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_kernel_argument_exp_desc_t mutateCopyKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateCopyKernelDstArg.commandId = commandIdCopy;
        mutateCopyKernelDstArg.argIndex = 1;
        mutateCopyKernelDstArg.argSize = sizeof(void *);
        mutateCopyKernelDstArg.pArgValue = &outputBuffer2;
        mutateCopyKernelDstArg.pNext = &mutateGroupSize;

        ze_mutable_kernel_argument_exp_desc_t mutateCopyKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateCopyKernelSrcArg.commandId = commandIdCopy;
        mutateCopyKernelSrcArg.argIndex = 0;
        mutateCopyKernelSrcArg.argSize = sizeof(void *);
        mutateCopyKernelSrcArg.pArgValue = &gwsDstBuffer;
        mutateCopyKernelSrcArg.pNext = &mutateCopyKernelDstArg;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateCopyKernelSrcArg;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandIdMutableKernel;
        mutateKernelDstArg.argIndex = 0;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &offsetDstBuffer;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandIdMutableKernel;
        mutateGroupCount.pGroupCount = &offsetDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelDstArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandIdMutableKernel;
        mutateGroupSize.groupSizeX = 1;
        mutateGroupSize.groupSizeY = 1;
        mutateGroupSize.groupSizeZ = 1;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_global_offset_exp_desc_t mutateGlobalOffset = {ZE_STRUCTURE_TYPE_MUTABLE_GLOBAL_OFFSET_EXP_DESC};
        mutateGlobalOffset.commandId = commandIdMutableKernel;
        mutateGlobalOffset.offsetX = globalOffset[0];
        mutateGlobalOffset.offsetY = globalOffset[1];
        mutateGlobalOffset.offsetZ = globalOffset[2];
        mutateGlobalOffset.pNext = &mutateGroupSize;

        ze_mutable_kernel_argument_exp_desc_t mutateCopyKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateCopyKernelDstArg.commandId = commandIdCopy;
        mutateCopyKernelDstArg.argIndex = 1;
        mutateCopyKernelDstArg.argSize = sizeof(void *);
        mutateCopyKernelDstArg.pArgValue = &outputBuffer2;
        mutateCopyKernelDstArg.pNext = &mutateGlobalOffset;

        ze_mutable_kernel_argument_exp_desc_t mutateCopyKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateCopyKernelSrcArg.commandId = commandIdCopy;
        mutateCopyKernelSrcArg.argIndex = 0;
        mutateCopyKernelSrcArg.argSize = sizeof(void *);
        mutateCopyKernelSrcArg.pArgValue = &offsetDstBuffer;
        mutateCopyKernelSrcArg.pNext = &mutateCopyKernelDstArg;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateCopyKernelSrcArg;
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        uint64_t *outputData = reinterpret_cast<uint64_t *>(outputBuffer2);
        if (globalOffsetFirst) {
            valid &= MclTests::validateGroupCountData(outputData, groupCount, false);
        } else {
            valid &= MclTests::validateGlobalOffsetData(outputData, globalOffset, startGroupCount, false);
        }

        SUCCESS_OR_TERMINATE(zeEventQueryStatus(events[1]));
        if (isTsEvent) {
            SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[1], &eventTimestamps[1]));

            if (eventTimestamps[0].context.kernelEnd > eventTimestamps[1].context.kernelEnd) {
                std::cerr << "eventTimestamps[0] greater than eventTimestamps[1]" << std::endl;
                valid = false;
            }
        }
    }

    for (auto &event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    MclTests::destroyEventPool(eventPool, eventOptions);

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, patternBuffer));

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, gwsDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, offsetDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, outputBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, outputBuffer2));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(gwsKernel);
    execEnv->destroyKernelHandle(offsetKernel);
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

std::string testNameMutateSingleTaskKernelArgumentsAndKeepGroupCount(bool passGroupCountDesc) {
    std::ostringstream testStream;

    testStream << "Mutate SingleTask arguments:";
    if (!passGroupCountDesc) {
        testStream << " do not";
    }
    testStream << " pass unchanged group count/size desc";

    return testStream.str();
}

bool testMutateSingleTaskKernelArgumentsAndKeepGroupCount(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool passGroupCountDesc, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 1024;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_kernel_handle_t taskKernel = execEnv->getKernelHandle(module, "single_task");

    void *dstBuffer1 = nullptr;
    void *dstBuffer2 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer2));

    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    uint32_t groupSizeX = 1u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(taskKernel, groupSizeX, groupSizeY, groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(taskKernel, 0, sizeof(void *), &dstBuffer1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdExp(cmdList, &commandIdDesc, &commandId));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, taskKernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        for (ElemType i = 0; i < 1024; ++i) {
            auto outcome = reinterpret_cast<ElemType *>(dstBuffer1)[i];
            if (!(outcome == i)) {
                std::cerr << "before mutation - fail operation: expected " << i << " received " << outcome << std::endl;
                valid = false;
                break;
            }
        }
    }

    ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelDstArg.commandId = commandId;
    mutateKernelDstArg.argIndex = 0;
    mutateKernelDstArg.argSize = sizeof(void *);
    mutateKernelDstArg.pArgValue = &dstBuffer2;

    ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
    mutateGroupCount.commandId = commandId;
    mutateGroupCount.pGroupCount = &dispatchTraits;
    mutateGroupCount.pNext = &mutateKernelDstArg;

    ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    mutateGroupSize.commandId = commandId;
    mutateGroupSize.groupSizeX = groupSizeX;
    mutateGroupSize.groupSizeY = groupSizeY;
    mutateGroupSize.groupSizeZ = groupSizeZ;
    mutateGroupSize.pNext = &mutateGroupCount;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    if (passGroupCountDesc) {
        mutateDesc.pNext = &mutateGroupSize;
    } else {
        mutateDesc.pNext = &mutateKernelDstArg;
    }

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        for (ElemType i = 0; i < 1024; ++i) {
            auto outcome = reinterpret_cast<ElemType *>(dstBuffer2)[i];
            if (!(outcome == i)) {
                std::cerr << "after mutation - fail operation: expected " << i << " received " << outcome << std::endl;
                valid = false;
                break;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(taskKernel);

    return valid;
}

std::string testNameMixingGlobalBarrierKernels(const std::string &baseName, KernelType firstKernelType, KernelType mutateKernelType, KernelType mutateBackKernelType) {
    std::ostringstream testStream;
    auto kernelTypeToString = [](KernelType type) -> std::string {
        switch (type) {
        case KernelType::copy:
            return "copy";
        case KernelType::globalBarrier:
            return "globalBarrier";
        case KernelType::globalBarrierMultiplicationGroupCount:
            return "globalBarrierMultiplicationGroupCount";
        default:
            return "Unknown";
        }
    };

    testStream << "Test is based on: " << baseName << "." << std::endl;
    testStream << "First Kernel is: " << kernelTypeToString(firstKernelType) << std::endl;
    testStream << "Mutate Kernel is: " << kernelTypeToString(mutateKernelType) << std::endl;
    testStream << "Mutate Back Kernel is: " << kernelTypeToString(mutateBackKernelType) << ".";

    return testStream.str();
}

bool testMutateGlobalBarrierKernels(MclTests::ExecEnv *execEnv, ze_module_handle_t module, ze_module_handle_t barrierModule, KernelType firstKernelType, KernelType mutateKernelType, KernelType mutateBackKernelType, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 2048;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    constexpr bool cooperative = true;

    ze_kernel_handle_t barrierKernel = execEnv->getKernelHandle(barrierModule, "globalBarrierKernel");
    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    auto testQueue = execEnv->getCooperativeQueue();

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, cooperative);

    void *copySrcBuffer = nullptr;
    void *copyDstBuffer = nullptr;

    void *barrierSrcBuffer = nullptr;
    void *barrierStageBuffer = nullptr;
    void *barrierDstBuffer = nullptr;
    void *barrierSrcBufferMultiple = nullptr;
    void *barrierStageBufferMultiple = nullptr;
    void *barrierDstBufferMultiple = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &barrierSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &barrierDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &barrierSrcBufferMultiple));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &barrierDstBufferMultiple));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copyDstBuffer));

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &barrierStageBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, allocSize, 1, execEnv->device, &barrierStageBufferMultiple));

    ElemType zero = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, barrierStageBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, barrierStageBufferMultiple, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));

    constexpr uint8_t copyVal = 2;
    constexpr ElemType barrierVal = 1;
    for (uint32_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(barrierSrcBuffer)[i] = barrierVal;
        reinterpret_cast<ElemType *>(barrierSrcBufferMultiple)[i] = barrierVal;
    }

    memset(copySrcBuffer, copyVal, allocSize);
    memset(copyDstBuffer, 0, allocSize);
    memset(barrierDstBuffer, 0, allocSize);
    memset(barrierDstBufferMultiple, 0, allocSize);

    uint32_t copyGroupSizeX = 32u;
    uint32_t copyGroupSizeY = 1u;
    uint32_t copyGroupSizeZ = 1u;

    uint32_t barrierGroupSizeX = 32u;
    uint32_t barrierGroupSizeY = 1u;
    uint32_t barrierGroupSizeZ = 1u;

    ze_group_count_t copyDispatchTraits;
    copyDispatchTraits.groupCountX = allocSize / copyGroupSizeX;
    copyDispatchTraits.groupCountY = 1;
    copyDispatchTraits.groupCountZ = 1;

    ze_group_count_t barrierDispatchTraits;
    barrierDispatchTraits.groupCountX = 2;
    barrierDispatchTraits.groupCountY = 1;
    barrierDispatchTraits.groupCountZ = 1;

    ze_group_count_t barrierMultipleCountDispatchTraits;
    barrierMultipleCountDispatchTraits.groupCountX = 32;
    barrierMultipleCountDispatchTraits.groupCountY = 1;
    barrierMultipleCountDispatchTraits.groupCountZ = 1;

    size_t barrierMultipleSize = barrierGroupSizeX * barrierMultipleCountDispatchTraits.groupCountX;
    std::vector<ElemType> verificationDataMultiple(barrierMultipleSize);

    for (uint32_t globalId = 0; globalId < barrierMultipleSize; globalId++) {
        ElemType partialSum = 0;
        for (uint32_t groupId = 0; groupId < barrierMultipleCountDispatchTraits.groupCountX; groupId++) {
            partialSum += (barrierVal + groupId);
        }
        verificationDataMultiple[globalId] = partialSum;
    }

    size_t barrierSize = barrierGroupSizeX * barrierDispatchTraits.groupCountX;
    std::vector<ElemType> verificationData(barrierSize);

    for (uint32_t globalId = 0; globalId < barrierSize; globalId++) {
        ElemType partialSum = 0;
        for (uint32_t groupId = 0; groupId < barrierDispatchTraits.groupCountX; groupId++) {
            partialSum += (barrierVal + groupId);
        }
        verificationData[globalId] = partialSum;
    }

    ze_kernel_handle_t firstKernel = nullptr;
    ze_kernel_handle_t mutableKernel = nullptr;
    ze_kernel_handle_t mutableBackKernel = nullptr;

    ze_group_count_t *firstDispatchTraits = nullptr;
    ze_group_count_t *mutableDispatchTraits = nullptr;
    ze_group_count_t *mutableBackDispatchTraits = nullptr;

    if (firstKernelType == KernelType::copy) {
        firstKernel = copyKernel;
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, copyGroupSizeX, copyGroupSizeY, copyGroupSizeZ));

        firstDispatchTraits = &copyDispatchTraits;
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &copySrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &copyDstBuffer));
    } else {
        firstKernel = barrierKernel;
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(barrierKernel, barrierGroupSizeX, barrierGroupSizeY, barrierGroupSizeZ));

        if (firstKernelType == KernelType::globalBarrier) {
            firstDispatchTraits = &barrierDispatchTraits;

            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(barrierKernel, 0, sizeof(barrierSrcBuffer), &barrierSrcBuffer));
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(barrierKernel, 1, sizeof(barrierDstBuffer), &barrierDstBuffer));
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(barrierKernel, 2, sizeof(barrierStageBuffer), &barrierStageBuffer));
        } else {
            firstDispatchTraits = &barrierMultipleCountDispatchTraits;

            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(barrierKernel, 0, sizeof(barrierSrcBufferMultiple), &barrierSrcBufferMultiple));
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(barrierKernel, 1, sizeof(barrierDstBufferMultiple), &barrierDstBufferMultiple));
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(barrierKernel, 2, sizeof(barrierStageBufferMultiple), &barrierStageBufferMultiple));
        }
    }

    if (mutateKernelType == KernelType::copy) {
        mutableKernel = copyKernel;
        mutableDispatchTraits = &copyDispatchTraits;
    } else {
        mutableKernel = barrierKernel;
        if (mutateKernelType == KernelType::globalBarrier) {
            mutableDispatchTraits = &barrierDispatchTraits;
        } else {
            mutableDispatchTraits = &barrierMultipleCountDispatchTraits;
        }
    }

    if (mutateBackKernelType == KernelType::copy) {
        mutableBackKernel = copyKernel;
        mutableBackDispatchTraits = &copyDispatchTraits;
    } else {
        mutableBackKernel = barrierKernel;
        if (mutateBackKernelType == KernelType::globalBarrier) {
            mutableBackDispatchTraits = &barrierDispatchTraits;
        } else {
            mutableBackDispatchTraits = &barrierMultipleCountDispatchTraits;
        }
    }

    ze_kernel_handle_t kernelGroup[] = {copyKernel, barrierKernel};

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandId));
    ze_command_list_append_launch_kernel_param_cooperative_desc_t coopDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC};
    coopDesc.isCooperative = true;
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelWithParameters(cmdList, firstKernel, firstDispatchTraits, &coopDesc, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(testQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(testQueue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        if (firstKernelType == KernelType::copy) {
            auto expectedValue = copyVal;
            if (LevelZeroBlackBoxTests::validateToValue(expectedValue, copyDstBuffer, allocSize) == false) {
                std::cerr << "before mutation - copy kernel" << std::endl;
                valid = false;
            }
        } else {
            if (firstKernelType == KernelType::globalBarrier) {
                if (LevelZeroBlackBoxTests::validate<ElemType>(verificationData.data(), barrierDstBuffer, barrierSize) == false) {
                    std::cerr << "before mutation - global barrier kernel" << std::endl;
                    valid = false;
                }
            } else {
                if (LevelZeroBlackBoxTests::validate<ElemType>(verificationDataMultiple.data(), barrierDstBufferMultiple, barrierMultipleSize) == false) {
                    std::cerr << "before mutation - global barrier kernel multiple group count" << std::endl;
                    valid = false;
                }
            }
        }
    }

    // mutate kernels and replace arguments
    if (firstKernel != mutableKernel) {
        // mutation of kernels => update mutable kernel
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mutableKernel));

        ze_mutable_kernel_argument_exp_desc_t mutateKernelStageArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelStageArg.commandId = commandId;
        mutateKernelStageArg.argIndex = 2;
        mutateKernelStageArg.argSize = sizeof(void *);

        // mutate group count / size / arg
        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pNext = &mutateKernelDstArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = mutableDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelSrcArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;

        if (mutateKernelType == KernelType::copy) {
            mutateGroupSize.groupSizeX = copyGroupSizeX;
            mutateGroupSize.groupSizeY = copyGroupSizeY;
            mutateGroupSize.groupSizeZ = copyGroupSizeZ;

            mutateKernelDstArg.pArgValue = &copyDstBuffer;
            mutateKernelSrcArg.pArgValue = &copySrcBuffer;
        } else {
            mutateKernelDstArg.pNext = &mutateKernelStageArg;

            mutateGroupSize.groupSizeX = barrierGroupSizeX;
            mutateGroupSize.groupSizeY = barrierGroupSizeY;
            mutateGroupSize.groupSizeZ = barrierGroupSizeZ;

            if (mutateKernelType == KernelType::globalBarrier) {
                mutateKernelDstArg.pArgValue = &barrierDstBuffer;
                mutateKernelSrcArg.pArgValue = &barrierSrcBuffer;
                mutateKernelStageArg.pArgValue = &barrierStageBuffer;
            } else {
                mutateKernelDstArg.pArgValue = &barrierDstBufferMultiple;
                mutateKernelSrcArg.pArgValue = &barrierSrcBufferMultiple;
                mutateKernelStageArg.pArgValue = &barrierStageBufferMultiple;
            }
        }

        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        // same kernel => mutate group count and args only
        ze_mutable_kernel_argument_exp_desc_t mutateKernelStageArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelStageArg.commandId = commandId;
        mutateKernelStageArg.argIndex = 2;
        mutateKernelStageArg.argSize = sizeof(void *);

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pNext = &mutateKernelStageArg;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pNext = &mutateKernelDstArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = mutableDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelSrcArg;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupCount;

        if (mutateKernelType == KernelType::globalBarrier) {
            mutateKernelDstArg.pArgValue = &barrierDstBuffer;
            mutateKernelSrcArg.pArgValue = &barrierSrcBuffer;
            mutateKernelStageArg.pArgValue = &barrierStageBuffer;
        } else {
            mutateKernelDstArg.pArgValue = &barrierDstBufferMultiple;
            mutateKernelSrcArg.pArgValue = &barrierSrcBufferMultiple;
            mutateKernelStageArg.pArgValue = &barrierStageBufferMultiple;
        }

        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(testQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(testQueue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (mutateKernelType == KernelType::copy) {
            auto expectedValue = copyVal;
            if (LevelZeroBlackBoxTests::validateToValue(expectedValue, copyDstBuffer, allocSize) == false) {
                std::cerr << "after mutation - copy kernel" << std::endl;
                valid = false;
            }
        } else {
            if (mutateKernelType == KernelType::globalBarrier) {
                if (LevelZeroBlackBoxTests::validate<ElemType>(verificationData.data(), barrierDstBuffer, barrierSize) == false) {
                    std::cerr << "after mutation - global barrier kernel" << std::endl;
                    valid = false;
                }
            } else {
                if (LevelZeroBlackBoxTests::validate<ElemType>(verificationDataMultiple.data(), barrierDstBufferMultiple, barrierMultipleSize) == false) {
                    std::cerr << "after mutation - global barrier kernel multiple group count" << std::endl;
                    valid = false;
                }
            }
        }
    }

    // when first and third operation is the same, just clear destination again before mutate back
    if (firstKernelType == mutateBackKernelType) {
        if (mutateBackKernelType == KernelType::copy) {
            SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, copyDstBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
        } else {
            if (mutateBackKernelType == KernelType::globalBarrier) {
                SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, barrierDstBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
                SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, barrierStageBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
            } else {
                SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, barrierDstBufferMultiple, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
                SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, barrierStageBufferMultiple, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
            }
        }
    }

    if (mutableKernel != mutableBackKernel) {
        // mutation of kernels => update mutable kernel
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mutableBackKernel));

        ze_mutable_kernel_argument_exp_desc_t mutateKernelStageArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelStageArg.commandId = commandId;
        mutateKernelStageArg.argIndex = 2;
        mutateKernelStageArg.argSize = sizeof(void *);

        // mutate group count / size / arg
        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pNext = &mutateKernelDstArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = mutableBackDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelSrcArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.pNext = &mutateGroupCount;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;

        if (mutateBackKernelType == KernelType::copy) {
            mutateGroupSize.groupSizeX = copyGroupSizeX;
            mutateGroupSize.groupSizeY = copyGroupSizeY;
            mutateGroupSize.groupSizeZ = copyGroupSizeZ;

            mutateKernelDstArg.pArgValue = &copyDstBuffer;
            mutateKernelSrcArg.pArgValue = &copySrcBuffer;
        } else {
            mutateKernelDstArg.pNext = &mutateKernelStageArg;

            mutateGroupSize.groupSizeX = barrierGroupSizeX;
            mutateGroupSize.groupSizeY = barrierGroupSizeY;
            mutateGroupSize.groupSizeZ = barrierGroupSizeZ;

            if (mutateBackKernelType == KernelType::globalBarrier) {
                mutateKernelDstArg.pArgValue = &barrierDstBuffer;
                mutateKernelSrcArg.pArgValue = &barrierSrcBuffer;
                mutateKernelStageArg.pArgValue = &barrierStageBuffer;
            } else {
                mutateKernelDstArg.pArgValue = &barrierDstBufferMultiple;
                mutateKernelSrcArg.pArgValue = &barrierSrcBufferMultiple;
                mutateKernelStageArg.pArgValue = &barrierStageBufferMultiple;
            }
        }

        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        // same kernel => mutate group count and args only
        ze_mutable_kernel_argument_exp_desc_t mutateKernelStageArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelStageArg.commandId = commandId;
        mutateKernelStageArg.argIndex = 2;
        mutateKernelStageArg.argSize = sizeof(void *);

        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pNext = &mutateKernelStageArg;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pNext = &mutateKernelDstArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = mutableBackDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelSrcArg;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupCount;

        if (mutateBackKernelType == KernelType::globalBarrier) {
            mutateKernelDstArg.pArgValue = &barrierDstBuffer;
            mutateKernelSrcArg.pArgValue = &barrierSrcBuffer;
            mutateKernelStageArg.pArgValue = &barrierStageBuffer;
        } else {
            mutateKernelDstArg.pArgValue = &barrierDstBufferMultiple;
            mutateKernelSrcArg.pArgValue = &barrierSrcBufferMultiple;
            mutateKernelStageArg.pArgValue = &barrierStageBufferMultiple;
        }

        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(testQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(testQueue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (mutateBackKernelType == KernelType::copy) {
            auto expectedValue = copyVal;
            if (LevelZeroBlackBoxTests::validateToValue(expectedValue, copyDstBuffer, allocSize) == false) {
                std::cerr << "after back mutation - copy kernel" << std::endl;
                valid = false;
            }
        } else {
            if (mutateBackKernelType == KernelType::globalBarrier) {
                if (LevelZeroBlackBoxTests::validate<ElemType>(verificationData.data(), barrierDstBuffer, barrierSize) == false) {
                    std::cerr << "after back mutation - global barrier kernel" << std::endl;
                    valid = false;
                }
            } else {
                if (LevelZeroBlackBoxTests::validate<ElemType>(verificationDataMultiple.data(), barrierDstBufferMultiple, barrierMultipleSize) == false) {
                    std::cerr << "after back mutation - global barrier kernel multiple group count" << std::endl;
                    valid = false;
                }
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, barrierSrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, barrierDstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, barrierStageBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, barrierSrcBufferMultiple));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, barrierDstBufferMultiple));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, barrierStageBufferMultiple));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copyDstBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(barrierKernel);
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

std::string testNameMixingSlmKernels(KernelType firstKernel, KernelType mutateKernel, KernelType mutateBackKernel) {
    std::ostringstream testStream;
    auto kernelTypeToString = [](KernelType type) -> std::string {
        switch (type) {
        case KernelType::copy:
            return "copy";
        case KernelType::slmKernelOneArgs:
            return "slmKernelOneArgs";
        case KernelType::slmKernelTwoArgs:
            return "slmKernelTwoArgs";
        default:
            return "Unknown";
        }
    };

    testStream << "Mutation of SLM Argument kernels." << std::endl;
    testStream << "First Kernel is: " << kernelTypeToString(firstKernel) << std::endl;
    testStream << "Mutate Kernel is: " << kernelTypeToString(mutateKernel) << std::endl;
    testStream << "Mutate Back Kernel is: " << kernelTypeToString(mutateBackKernel) << ".";

    return testStream.str();
}

bool testMutateSlmKernels(MclTests::ExecEnv *execEnv, ze_module_handle_t module, ze_module_handle_t slmModule, KernelType firstKernel, KernelType mutateKernel, KernelType mutateBackKernel, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 512;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

    uint32_t slmGroupSizeArray[] = {16, 32, 64};
    uint32_t slmGroupCountArray[] = {3, 2, 1};

    const std::string kernelNameSlmOneArg = "slmKernelOneArg";
    const std::string kernelNameSlmTwoArgs = "slmKernelTwoArgs";
    const std::string kernelNameCopy = "copyKernel";

    ze_kernel_handle_t slmOneArgKernel = execEnv->getKernelHandle(slmModule, kernelNameSlmOneArg);
    ze_kernel_handle_t slmTwoArgsKernel = execEnv->getKernelHandle(slmModule, kernelNameSlmTwoArgs);
    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, kernelNameCopy);

    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    void *outBuffer = nullptr;
    void *outBuffer2 = nullptr;
    void *copySrcBuffer = nullptr;
    void *copyDstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &outBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &outBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &copyDstBuffer));

    memset(copySrcBuffer, 5, allocSize);
    memset(copyDstBuffer, 0, allocSize);

    uint32_t slmStep = 0;

    uint32_t groupSizeX = slmGroupSizeArray[slmStep];
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = slmGroupCountArray[slmStep];
    dispatchTraits.groupCountY = 1;
    dispatchTraits.groupCountZ = 1;

    uint32_t dataSize = slmGroupSizeArray[slmStep] * slmGroupCountArray[slmStep];

    std::vector<ElemType> verificationData;

    uint32_t copyGroupSizeX = 32;
    uint32_t copyGroupSizeY = 1u;
    uint32_t copyGroupSizeZ = 1u;

    ze_group_count_t copyDispatchTraits;
    copyDispatchTraits.groupCountX = allocSize / copyGroupSizeX;
    copyDispatchTraits.groupCountY = 1;
    copyDispatchTraits.groupCountZ = 1;

    ze_kernel_handle_t firstKernelHandle = nullptr;
    ze_kernel_handle_t mutateKernelHandle = nullptr;
    ze_kernel_handle_t mutateBackKernelHandle = nullptr;

    ze_group_count_t *firstDispatchTraits = nullptr;

    if (firstKernel == KernelType::copy) {
        firstKernelHandle = copyKernel;
        firstDispatchTraits = &copyDispatchTraits;
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, copyGroupSizeX, copyGroupSizeY, copyGroupSizeZ));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(void *), &copySrcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(void *), &copyDstBuffer));
    } else {
        firstDispatchTraits = &dispatchTraits;
        if (firstKernel == KernelType::slmKernelOneArgs) {
            firstKernelHandle = slmOneArgKernel;
            SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(slmOneArgKernel, groupSizeX, groupSizeY, groupSizeZ));

            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(slmOneArgKernel, 0, sizeof(void *), &outBuffer));
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(slmOneArgKernel, 1, groupSizeX * sizeof(ElemType), nullptr));

            MclTests::prepareOutputSlmKernelOneArg(verificationData, groupSizeX, dispatchTraits.groupCountX);
        } else {
            firstKernelHandle = slmTwoArgsKernel;
            SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(slmTwoArgsKernel, groupSizeX, groupSizeY, groupSizeZ));

            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(slmTwoArgsKernel, 0, sizeof(void *), &outBuffer2));
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(slmTwoArgsKernel, 1, groupSizeX * sizeof(ElemType), nullptr));
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(slmTwoArgsKernel, 2, groupSizeX * sizeof(ElemType), nullptr));

            MclTests::prepareOutputSlmKernelTwoArgs(verificationData, groupSizeX, dispatchTraits.groupCountX);
        }
    }

    if (mutateKernel == KernelType::copy) {
        mutateKernelHandle = copyKernel;
    } else {
        if (mutateKernel == KernelType::slmKernelOneArgs) {
            mutateKernelHandle = slmOneArgKernel;
        } else {
            mutateKernelHandle = slmTwoArgsKernel;
        }
    }

    if (mutateBackKernel == KernelType::copy) {
        mutateBackKernelHandle = copyKernel;
    } else {
        if (mutateBackKernel == KernelType::slmKernelOneArgs) {
            mutateBackKernelHandle = slmOneArgKernel;
        } else {
            mutateBackKernelHandle = slmTwoArgsKernel;
        }
    }

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE |
                          ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;

    ze_kernel_handle_t kernels[] = {slmOneArgKernel, slmTwoArgsKernel, copyKernel};
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 3, kernels, &commandId));
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, firstKernelHandle, firstDispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;

    if (!aubMode) {
        if (firstKernel == KernelType::copy) {
            if (LevelZeroBlackBoxTests::validate(copySrcBuffer, copyDstBuffer, allocSize) == false) {
                std::cerr << "before mutation - copy kernel" << std::endl;
                valid = false;
            }
        } else {
            void *data = firstKernel == KernelType::slmKernelOneArgs ? outBuffer : outBuffer2;
            if (LevelZeroBlackBoxTests::validate(verificationData.data(), data, dataSize * sizeof(ElemType)) == false) {
                std::cerr << "before mutation - slm kernel: " << static_cast<uint32_t>(firstKernel) << std::endl;
                valid = false;
            }
        }
    }

    ElemType zero = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, outBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, outBuffer2, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, copyDstBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));

    slmStep++;
    groupSizeX = slmGroupSizeArray[slmStep];
    dispatchTraits.groupCountX = slmGroupCountArray[slmStep];
    dataSize = slmGroupSizeArray[slmStep] * slmGroupCountArray[slmStep];

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mutateKernelHandle));

    if (mutateKernel == KernelType::copy) {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &copyDstBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &copySrcBuffer;
        mutateKernelSrcArg.pNext = &mutateKernelDstArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = &copyDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelSrcArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.pNext = &mutateGroupCount;
        mutateGroupSize.groupSizeX = copyGroupSizeX;
        mutateGroupSize.groupSizeY = copyGroupSizeY;
        mutateGroupSize.groupSizeZ = copyGroupSizeZ;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;

        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        if (mutateKernel == KernelType::slmKernelOneArgs) {
            ze_mutable_kernel_argument_exp_desc_t mutateKernelSlmOneArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
            mutateKernelSlmOneArg.commandId = commandId;
            mutateKernelSlmOneArg.argIndex = 1;
            mutateKernelSlmOneArg.argSize = groupSizeX * sizeof(ElemType);
            mutateKernelSlmOneArg.pArgValue = nullptr;

            ze_mutable_kernel_argument_exp_desc_t mutateKernelOutArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
            mutateKernelOutArg.commandId = commandId;
            mutateKernelOutArg.argIndex = 0;
            mutateKernelOutArg.argSize = sizeof(void *);
            mutateKernelOutArg.pArgValue = &outBuffer;
            mutateKernelOutArg.pNext = &mutateKernelSlmOneArg;

            ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
            mutateGroupCount.commandId = commandId;
            mutateGroupCount.pGroupCount = &dispatchTraits;
            mutateGroupCount.pNext = &mutateKernelOutArg;

            ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
            mutateGroupSize.commandId = commandId;
            mutateGroupSize.pNext = &mutateGroupCount;
            mutateGroupSize.groupSizeX = groupSizeX;
            mutateGroupSize.groupSizeY = groupSizeY;
            mutateGroupSize.groupSizeZ = groupSizeZ;

            ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
            mutateDesc.pNext = &mutateGroupSize;

            SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));

            MclTests::prepareOutputSlmKernelOneArg(verificationData, groupSizeX, dispatchTraits.groupCountX);
        } else {
            ze_mutable_kernel_argument_exp_desc_t mutateKernelSlmTwoArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
            mutateKernelSlmTwoArg.commandId = commandId;
            mutateKernelSlmTwoArg.argIndex = 2;
            mutateKernelSlmTwoArg.argSize = groupSizeX * sizeof(ElemType);
            mutateKernelSlmTwoArg.pArgValue = nullptr;

            ze_mutable_kernel_argument_exp_desc_t mutateKernelSlmOneArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
            mutateKernelSlmOneArg.commandId = commandId;
            mutateKernelSlmOneArg.argIndex = 1;
            mutateKernelSlmOneArg.argSize = groupSizeX * sizeof(ElemType);
            mutateKernelSlmOneArg.pArgValue = nullptr;
            mutateKernelSlmOneArg.pNext = &mutateKernelSlmTwoArg;

            ze_mutable_kernel_argument_exp_desc_t mutateKernelOutArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
            mutateKernelOutArg.commandId = commandId;
            mutateKernelOutArg.argIndex = 0;
            mutateKernelOutArg.argSize = sizeof(void *);
            mutateKernelOutArg.pArgValue = &outBuffer2;
            mutateKernelOutArg.pNext = &mutateKernelSlmOneArg;

            ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
            mutateGroupCount.commandId = commandId;
            mutateGroupCount.pGroupCount = &dispatchTraits;
            mutateGroupCount.pNext = &mutateKernelOutArg;

            ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
            mutateGroupSize.commandId = commandId;
            mutateGroupSize.pNext = &mutateGroupCount;
            mutateGroupSize.groupSizeX = groupSizeX;
            mutateGroupSize.groupSizeY = groupSizeY;
            mutateGroupSize.groupSizeZ = groupSizeZ;

            ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
            mutateDesc.pNext = &mutateGroupSize;

            SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));

            MclTests::prepareOutputSlmKernelTwoArgs(verificationData, groupSizeX, dispatchTraits.groupCountX);
        }
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (mutateKernel == KernelType::copy) {
            if (LevelZeroBlackBoxTests::validate(copySrcBuffer, copyDstBuffer, allocSize) == false) {
                std::cerr << "after mutation - copy kernel" << std::endl;
                valid = false;
            }
        } else {
            void *data = mutateKernel == KernelType::slmKernelOneArgs ? outBuffer : outBuffer2;
            if (LevelZeroBlackBoxTests::validate(verificationData.data(), data, dataSize * sizeof(ElemType)) == false) {
                std::cerr << "after mutation - slm kernel: " << static_cast<uint32_t>(mutateKernel) << std::endl;
                valid = false;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, outBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, outBuffer2, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, copyDstBuffer, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));

    slmStep++;
    groupSizeX = slmGroupSizeArray[slmStep];
    dispatchTraits.groupCountX = slmGroupCountArray[slmStep];
    dataSize = slmGroupSizeArray[slmStep] * slmGroupCountArray[slmStep];

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mutateBackKernelHandle));

    if (mutateBackKernel == KernelType::copy) {
        ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelDstArg.commandId = commandId;
        mutateKernelDstArg.argIndex = 1;
        mutateKernelDstArg.argSize = sizeof(void *);
        mutateKernelDstArg.pArgValue = &copyDstBuffer;

        ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
        mutateKernelSrcArg.commandId = commandId;
        mutateKernelSrcArg.argIndex = 0;
        mutateKernelSrcArg.argSize = sizeof(void *);
        mutateKernelSrcArg.pArgValue = &copySrcBuffer;
        mutateKernelSrcArg.pNext = &mutateKernelDstArg;

        ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
        mutateGroupCount.commandId = commandId;
        mutateGroupCount.pGroupCount = &copyDispatchTraits;
        mutateGroupCount.pNext = &mutateKernelSrcArg;

        ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
        mutateGroupSize.commandId = commandId;
        mutateGroupSize.pNext = &mutateGroupCount;
        mutateGroupSize.groupSizeX = copyGroupSizeX;
        mutateGroupSize.groupSizeY = copyGroupSizeY;
        mutateGroupSize.groupSizeZ = copyGroupSizeZ;

        ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
        mutateDesc.pNext = &mutateGroupSize;

        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    } else {
        if (mutateBackKernel == KernelType::slmKernelOneArgs) {
            ze_mutable_kernel_argument_exp_desc_t mutateKernelSlmOneArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
            mutateKernelSlmOneArg.commandId = commandId;
            mutateKernelSlmOneArg.argIndex = 1;
            mutateKernelSlmOneArg.argSize = groupSizeX * sizeof(ElemType);
            mutateKernelSlmOneArg.pArgValue = nullptr;

            ze_mutable_kernel_argument_exp_desc_t mutateKernelOutArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
            mutateKernelOutArg.commandId = commandId;
            mutateKernelOutArg.argIndex = 0;
            mutateKernelOutArg.argSize = sizeof(void *);
            mutateKernelOutArg.pArgValue = &outBuffer;
            mutateKernelOutArg.pNext = &mutateKernelSlmOneArg;

            ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
            mutateGroupCount.commandId = commandId;
            mutateGroupCount.pGroupCount = &dispatchTraits;
            mutateGroupCount.pNext = &mutateKernelOutArg;

            ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
            mutateGroupSize.commandId = commandId;
            mutateGroupSize.pNext = &mutateGroupCount;
            mutateGroupSize.groupSizeX = groupSizeX;
            mutateGroupSize.groupSizeY = groupSizeY;
            mutateGroupSize.groupSizeZ = groupSizeZ;

            ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
            mutateDesc.pNext = &mutateGroupSize;

            SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));

            MclTests::prepareOutputSlmKernelOneArg(verificationData, groupSizeX, dispatchTraits.groupCountX);
        } else {
            ze_mutable_kernel_argument_exp_desc_t mutateKernelSlmTwoArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
            mutateKernelSlmTwoArg.commandId = commandId;
            mutateKernelSlmTwoArg.argIndex = 2;
            mutateKernelSlmTwoArg.argSize = groupSizeX * sizeof(ElemType);
            mutateKernelSlmTwoArg.pArgValue = nullptr;

            ze_mutable_kernel_argument_exp_desc_t mutateKernelSlmOneArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
            mutateKernelSlmOneArg.commandId = commandId;
            mutateKernelSlmOneArg.argIndex = 1;
            mutateKernelSlmOneArg.argSize = groupSizeX * sizeof(ElemType);
            mutateKernelSlmOneArg.pArgValue = nullptr;
            mutateKernelSlmOneArg.pNext = &mutateKernelSlmTwoArg;

            ze_mutable_kernel_argument_exp_desc_t mutateKernelOutArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
            mutateKernelOutArg.commandId = commandId;
            mutateKernelOutArg.argIndex = 0;
            mutateKernelOutArg.argSize = sizeof(void *);
            mutateKernelOutArg.pArgValue = &outBuffer2;
            mutateKernelOutArg.pNext = &mutateKernelSlmOneArg;

            ze_mutable_group_count_exp_desc_t mutateGroupCount = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
            mutateGroupCount.commandId = commandId;
            mutateGroupCount.pGroupCount = &dispatchTraits;
            mutateGroupCount.pNext = &mutateKernelOutArg;

            ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
            mutateGroupSize.commandId = commandId;
            mutateGroupSize.pNext = &mutateGroupCount;
            mutateGroupSize.groupSizeX = groupSizeX;
            mutateGroupSize.groupSizeY = groupSizeY;
            mutateGroupSize.groupSizeZ = groupSizeZ;

            ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
            mutateDesc.pNext = &mutateGroupSize;

            SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));

            MclTests::prepareOutputSlmKernelTwoArgs(verificationData, groupSizeX, dispatchTraits.groupCountX);
        }
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (mutateBackKernel == KernelType::copy) {
            if (LevelZeroBlackBoxTests::validate(copySrcBuffer, copyDstBuffer, allocSize) == false) {
                std::cerr << "after mutation - copy kernel" << std::endl;
                valid = false;
            }
        } else {
            void *data = mutateBackKernel == KernelType::slmKernelOneArgs ? outBuffer : outBuffer2;
            if (LevelZeroBlackBoxTests::validate(verificationData.data(), data, dataSize * sizeof(ElemType)) == false) {
                std::cerr << "after mutation - slm kernel: " << static_cast<uint32_t>(mutateBackKernel) << std::endl;
                valid = false;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, outBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, outBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copySrcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, copyDstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(slmOneArgKernel);
    execEnv->destroyKernelHandle(slmTwoArgsKernel);
    execEnv->destroyKernelHandle(copyKernel);

    return valid;
}

std::string testNameMutateBackKernel(bool firstMultiple) {
    std::ostringstream testStream;

    std::string firstKernel = firstMultiple ? "multiple scalar" : "add scalar";
    std::string mutateKernel = firstMultiple ? "add scalar" : "multiple scalar";

    testStream << "Append kernel " << firstKernel;
    testStream << " mutate it into " << mutateKernel << "." << std::endl;
    testStream << "Mutate back into " << firstKernel;
    testStream << " mutate back again into " << mutateKernel;
    testStream << " Test.";

    return testStream.str();
}

bool testMutateBackKernel(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool firstMultiple, bool aubMode) {
    using ElemType = uint32_t;
    constexpr size_t elemSize = 256;
    constexpr size_t allocSize = elemSize * sizeof(ElemType);

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

    const std::string firstOp = firstMultiple ? "mul" : "add";
    const std::string mutateOp = firstMultiple ? "add" : "mul";

    void *srcBufferFirst = firstMultiple ? srcBuffer1 : srcBuffer2;
    void *dstBufferFirst = firstMultiple ? dstBuffer1 : dstBuffer2;

    void *srcBufferMutable = firstMultiple ? srcBuffer2 : srcBuffer1;
    void *dstBufferMutable = firstMultiple ? dstBuffer2 : dstBuffer1;

    constexpr ElemType addSrcValue = 10;
    constexpr ElemType mulSrcValue = 12;
    for (size_t i = 0; i < elemSize; i++) {
        reinterpret_cast<ElemType *>(srcBuffer1)[i] = mulSrcValue;
        reinterpret_cast<ElemType *>(srcBuffer2)[i] = addSrcValue;
    }
    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    ElemType scalarValueAdd = 0x1;
    ElemType scalarValueMul = 0x2;
    ElemType firstScalarValue = firstMultiple ? scalarValueMul : scalarValueAdd;
    ElemType mutableScalarValue = firstMultiple ? scalarValueAdd : scalarValueMul;

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    if (firstMultiple) {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(mulKernel, groupSizeX, groupSizeY, groupSizeZ));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 0, sizeof(void *), &srcBuffer1));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 1, sizeof(void *), &dstBuffer1));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(mulKernel, 2, sizeof(scalarValueMul), &scalarValueMul));
    } else {
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addKernel, groupSizeX, groupSizeY, groupSizeZ));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 0, sizeof(void *), &srcBuffer2));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 1, sizeof(void *), &dstBuffer2));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(addKernel, 2, sizeof(scalarValueAdd), &scalarValueAdd));
    }

    uint32_t firstGroupCount = (elemSize / groupSizeX) / 2;
    uint32_t mutateGroupCount = elemSize / groupSizeX;

    size_t firstKernelSize = elemSize / 2;
    size_t mutateKernelSize = elemSize;

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = firstGroupCount;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_kernel_handle_t kernelGroup[] = {addKernel, mulKernel};

    uint64_t commandId = 0;
    ze_mutable_command_id_exp_desc_t commandIdDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_ID_EXP_DESC};
    commandIdDesc.flags = ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE | ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
    SUCCESS_OR_TERMINATE(zeCommandListGetNextCommandIdWithKernelsExp(cmdList, &commandIdDesc, 2, kernelGroup, &commandId));
    if (firstMultiple) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, mulKernel, &dispatchTraits, nullptr, 0, nullptr));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, addKernel, &dispatchTraits, nullptr, 0, nullptr));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    ElemType expectedValue = 0;
    if (firstMultiple) {
        expectedValue = mulSrcValue * scalarValueMul;
    } else {
        expectedValue = addSrcValue + scalarValueAdd;
    }

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBufferFirst, firstKernelSize) == false) {
            std::cerr << "before mutation - fail " << firstOp << " operation " << std::endl;
            valid = false;
        }
    }

    // select new kernel
    if (firstMultiple) {
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &addKernel));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mulKernel));
    }

    // select arguments and dispatch parameters for new kernel
    ze_mutable_kernel_argument_exp_desc_t mutateKernelSrcArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelSrcArg.commandId = commandId;
    mutateKernelSrcArg.argIndex = 0;
    mutateKernelSrcArg.argSize = sizeof(void *);
    mutateKernelSrcArg.pArgValue = &srcBufferMutable;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelDstArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelDstArg.commandId = commandId;
    mutateKernelDstArg.argIndex = 1;
    mutateKernelDstArg.argSize = sizeof(void *);
    mutateKernelDstArg.pArgValue = &dstBufferMutable;
    mutateKernelDstArg.pNext = &mutateKernelSrcArg;

    ze_mutable_kernel_argument_exp_desc_t mutateKernelScalarArg = {ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_ARGUMENT_EXP_DESC};
    mutateKernelScalarArg.commandId = commandId;
    mutateKernelScalarArg.argIndex = 2;
    mutateKernelScalarArg.argSize = sizeof(ElemType);
    mutateKernelScalarArg.pArgValue = &mutableScalarValue;
    mutateKernelScalarArg.pNext = &mutateKernelDstArg;

    dispatchTraits.groupCountX = mutateGroupCount;
    ze_mutable_group_count_exp_desc_t mutateGroupCountDesc = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_COUNT_EXP_DESC};
    mutateGroupCountDesc.commandId = commandId;
    mutateGroupCountDesc.pGroupCount = &dispatchTraits;
    mutateGroupCountDesc.pNext = &mutateKernelScalarArg;

    ze_mutable_group_size_exp_desc_t mutateGroupSize = {ZE_STRUCTURE_TYPE_MUTABLE_GROUP_SIZE_EXP_DESC};
    mutateGroupSize.commandId = commandId;
    mutateGroupSize.groupSizeX = groupSizeX;
    mutateGroupSize.groupSizeY = groupSizeY;
    mutateGroupSize.groupSizeZ = groupSizeZ;
    mutateGroupSize.pNext = &mutateGroupCountDesc;

    ze_mutable_commands_exp_desc_t mutateDesc = {ZE_STRUCTURE_TYPE_MUTABLE_COMMANDS_EXP_DESC};
    mutateDesc.pNext = &mutateGroupSize;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (firstMultiple) {
        expectedValue = addSrcValue + scalarValueAdd;
    } else {
        expectedValue = mulSrcValue * scalarValueMul;
    }

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBufferMutable, mutateKernelSize) == false) {
            std::cerr << "after mutation - fail " << mutateOp << " operation " << std::endl;
            valid = false;
        }
    }

    ElemType zero = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, dstBufferFirst, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));

    // mutate back
    if (firstMultiple) {
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mulKernel));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &addKernel));
    }

    mutateKernelSrcArg.pArgValue = &srcBufferFirst;
    mutateKernelDstArg.pArgValue = &dstBufferFirst;
    mutateKernelScalarArg.pArgValue = &firstScalarValue;
    dispatchTraits.groupCountX = firstGroupCount;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (firstMultiple) {
        expectedValue = mulSrcValue * scalarValueMul;
    } else {
        expectedValue = addSrcValue + scalarValueAdd;
    }

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBufferFirst, firstKernelSize) == false) {
            std::cerr << "after back mutation - fail " << firstOp << " operation " << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(execEnv->immCmdList, dstBufferMutable, &zero, sizeof(zero), allocSize, nullptr, 0, nullptr));

    if (firstMultiple) {
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &addKernel));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandKernelsExp(cmdList, 1, &commandId, &mulKernel));
    }

    mutateKernelSrcArg.pArgValue = &srcBufferMutable;
    mutateKernelDstArg.pArgValue = &dstBufferMutable;
    mutateKernelScalarArg.pArgValue = &mutableScalarValue;
    dispatchTraits.groupCountX = mutateGroupCount;

    SUCCESS_OR_TERMINATE(zeCommandListUpdateMutableCommandsExp(cmdList, &mutateDesc));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (firstMultiple) {
        expectedValue = addSrcValue + scalarValueAdd;
    } else {
        expectedValue = mulSrcValue * scalarValueMul;
    }

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validateToValue<ElemType>(expectedValue, dstBufferMutable, mutateKernelSize) == false) {
            std::cerr << "after back to mutable mutation - fail " << mutateOp << " operation " << std::endl;
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(mulKernel);
    execEnv->destroyKernelHandle(addKernel);

    return valid;
}

int main(int argc, char *argv[]) {
    constexpr uint32_t bitNumberTestTwoKernels = 0u;
    constexpr uint32_t bitNumberKernelsArgumentsSignalEvents = 1u;
    constexpr uint32_t bitNumberKernelsWaitAndSignalEvents = 2u;
    constexpr uint32_t bitNumberMixedDifferentKernels = 3u;
    constexpr uint32_t bitNumberMixedKernelsAndShapes = 4u;
    constexpr uint32_t bitNumberMixingKernelArgumentsShapesEvents = 5u;
    constexpr uint32_t bitNumberMixingKernelsArgumentsShapesEvents = 6u;
    constexpr uint32_t bitNumberTestSingleTask = 7u;
    constexpr uint32_t bitNumberMixingGlobalBarrierKernels = 8u;
    constexpr uint32_t bitNumberMixingSlmKernels = 9u;
    constexpr uint32_t bitNumberMutateBack = 10u;

    const std::string blackBoxName = "Zello MCL Kernels";

    constexpr uint32_t defaultTestMask = std::numeric_limits<uint32_t>::max() & ~(1u << bitNumberMixingGlobalBarrierKernels);
    LevelZeroBlackBoxTests::TestBitMask testMask = LevelZeroBlackBoxTests::getTestMask(argc, argv, defaultTestMask);
    LevelZeroBlackBoxTests::TestBitMask testSubMask = LevelZeroBlackBoxTests::getTestSubmask(argc, argv, std::numeric_limits<uint32_t>::max());
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    LevelZeroBlackBoxTests::getErrorMax(argc, argv);

    auto env = std::unique_ptr<MclTests::ExecEnv>(MclTests::ExecEnv::create());
    env->setGlobalKernels(argc, argv);

    ze_module_handle_t module;
    env->buildMainModule(module);

    bool valid = true;
    bool caseResult = true;
    std::string caseName = "";

    if (testMask.test(bitNumberTestTwoKernels)) {
        std::vector<bool> useSameArgumentsValues = {true, false};
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::noEvent,
                                                                 MclTests::EventOptions::none,
                                                                 MclTests::EventOptions::timestamp,
                                                                 MclTests::EventOptions::signalEvent,
                                                                 MclTests::EventOptions::signalEventTimestamp,
                                                                 MclTests::EventOptions::cbEvent,
                                                                 MclTests::EventOptions::cbEventTimestamp,
                                                                 MclTests::EventOptions::cbEventSignal,
                                                                 MclTests::EventOptions::cbEventSignalTimestamp};
        for (auto eventOption : eventOptionValues) {
            for (auto useSameArguments : useSameArgumentsValues) {
                caseName = testNameMutateMulKernelIntoAddKernelBackMulKernel(useSameArguments, eventOption);
                caseResult = testMutateMulKernelIntoAddKernelBackMulKernel(env.get(), module, useSameArguments, aubMode, eventOption);
                LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                valid &= caseResult;
                MclTests::addFailedCase(caseResult, caseName, bitNumberTestTwoKernels);
            }
        }
    }

    if (testMask.test(bitNumberKernelsArgumentsSignalEvents)) {
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::none,
                                                                 MclTests::EventOptions::timestamp,
                                                                 MclTests::EventOptions::signalEvent,
                                                                 MclTests::EventOptions::signalEventTimestamp,
                                                                 MclTests::EventOptions::cbEvent,
                                                                 MclTests::EventOptions::cbEventTimestamp,
                                                                 MclTests::EventOptions::cbEventSignal,
                                                                 MclTests::EventOptions::cbEventSignalTimestamp};
        for (auto eventOption : eventOptionValues) {
            caseName = testNameMutateSignalEventArgumentsThenKernelAndKernelBackMutateIntoThirdSignalEventArguments(eventOption);
            caseResult = testMutateSignalEventArgumentsThenKernelAndKernelBackMutateIntoThirdSignalEventArguments(env.get(), module, aubMode, eventOption);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberKernelsArgumentsSignalEvents);
        }
    }

    if (testMask.test(bitNumberKernelsWaitAndSignalEvents)) {
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::none,
                                                                 MclTests::EventOptions::timestamp,
                                                                 MclTests::EventOptions::signalEvent,
                                                                 MclTests::EventOptions::signalEventTimestamp,
                                                                 MclTests::EventOptions::cbEvent,
                                                                 MclTests::EventOptions::cbEventTimestamp,
                                                                 MclTests::EventOptions::cbEventSignal,
                                                                 MclTests::EventOptions::cbEventSignalTimestamp};
        for (auto eventOption : eventOptionValues) {
            caseName = testNameMutateKernelsWaitSignalEvents(eventOption);
            caseResult = testMutateKernelsWaitSignalEvents(env.get(), module, aubMode, eventOption);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberKernelsWaitAndSignalEvents);
        }
    }

    if (testMask.test(bitNumberMixedDifferentKernels)) {
        if (testSubMask.test(0)) {
            std::vector<bool> scratchKernelFirstValues = {false, true};
            ze_module_handle_t scratchModule = nullptr;
            ze_kernel_handle_t scratchKernel = nullptr;

            LevelZeroBlackBoxTests::createScratchModuleKernel(env->context, env->device, scratchModule, scratchKernel, &MclTests::mclBuildOption);

            for (auto scratchKernelFirst : scratchKernelFirstValues) {
                caseName = testNameMutateKernelsScratch(scratchKernelFirst);
                caseResult = testMutateKernelsScratch(env.get(), module, scratchKernel, scratchKernelFirst, aubMode);
                LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                valid &= caseResult;
                MclTests::addFailedCase(caseResult, caseName, bitNumberMixedDifferentKernels);
            }

            SUCCESS_OR_TERMINATE(zeKernelDestroy(scratchKernel));
            SUCCESS_OR_TERMINATE(zeModuleDestroy(scratchModule));
        }

        if (testSubMask.test(1)) {
            std::vector<bool> slmKernelFirstValues = {true, false};
            for (auto slmKernelFirst : slmKernelFirstValues) {
                caseName = testNameMutateKernelsSlm(slmKernelFirst);
                caseResult = testMutateKernelsSlm(env.get(), module, slmKernelFirst, aubMode);
                LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                valid &= caseResult;
                MclTests::addFailedCase(caseResult, caseName, bitNumberMixedDifferentKernels);
            }
        }

        if (testSubMask.test(2)) {
            ze_module_handle_t privateModule = nullptr;
            ze_kernel_handle_t privateKernel = nullptr;
            std::vector<bool> privateMemoryKernelFirstValues = {false, true};

            MclTests::buildPrivateMemoryKernel(env->context, env->device, privateModule, privateKernel, true);

            for (auto privateMemoryKernelFirst : privateMemoryKernelFirstValues) {
                caseName = testNameMutateKernelsPrivateMemory(privateMemoryKernelFirst);
                caseResult = testMutateKernelsPrivateMemory(env.get(), module, privateKernel, privateMemoryKernelFirst, aubMode);
                LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                valid &= caseResult;
                MclTests::addFailedCase(caseResult, caseName, bitNumberMixedDifferentKernels);
            }

            SUCCESS_OR_TERMINATE(zeKernelDestroy(privateKernel));
            SUCCESS_OR_TERMINATE(zeModuleDestroy(privateModule));
        }
    }

    if (testMask.test(bitNumberMixedKernelsAndShapes)) {
        if (testSubMask.test(0)) {
            std::vector<bool> localWorkSizeKernelFirstValues = {false, true};
            std::vector<std::array<uint32_t, 3>> localWorkSizeKernelGroupSizeValues = {{16, 2, 4},
                                                                                       {3, 3, 3}};

            std::vector<std::array<uint32_t, 3>> addScalarLinearKernelGroupSizeValues = {{32, 1, 1},
                                                                                         {5, 7, 3}};

            for (auto &localWorkSizeKernelGroupSize : localWorkSizeKernelGroupSizeValues) {
                for (auto &addScalarLinearKernelGroupSize : addScalarLinearKernelGroupSizeValues) {
                    for (auto localWorkSizeKernelFirst : localWorkSizeKernelFirstValues) {
                        caseName = testNameMutateKernelsLocalWorkSize(localWorkSizeKernelFirst, localWorkSizeKernelGroupSize.data(), addScalarLinearKernelGroupSize.data());
                        caseResult = testMutateKernelsLocalWorkSize(env.get(), module, localWorkSizeKernelGroupSize.data(), addScalarLinearKernelGroupSize.data(), localWorkSizeKernelFirst, aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberMixedKernelsAndShapes);
                    }
                }
            }
        }

        if (testSubMask.test(1)) {
            std::vector<bool> globalWorkSizeKernelFirstValues = {false, true};
            std::vector<std::array<uint32_t, 3>> globalWorkSizeKernelGroupCountValues = {{16, 2, 4},
                                                                                         {4, 2, 4}};

            std::vector<std::array<uint32_t, 3>> addScalarLinearKernelGroupCountValues = {{32, 1, 1},
                                                                                          {8, 1, 4}};

            for (auto &globalWorkSizeKernelGroupCount : globalWorkSizeKernelGroupCountValues) {
                for (auto &addScalarLinearKernelGroupCount : addScalarLinearKernelGroupCountValues) {
                    for (auto globalWorkSizeKernelFirst : globalWorkSizeKernelFirstValues) {
                        caseName = testNameMutateKernelsGlobalWorkSize(globalWorkSizeKernelFirst, globalWorkSizeKernelGroupCount.data(), addScalarLinearKernelGroupCount.data());
                        caseResult = testMutateKernelsGlobalWorkSize(env.get(), module, globalWorkSizeKernelGroupCount.data(), addScalarLinearKernelGroupCount.data(), globalWorkSizeKernelFirst, aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberMixedKernelsAndShapes);
                    }
                }
            }
        }

        if (testSubMask.test(2)) {
            std::vector<bool> globalOffsetKernelFirstValues = {false, true};
            std::vector<std::array<uint32_t, 3>> globalOffsetValues = {{2, 2, 3},
                                                                       {0, 3, 1},
                                                                       {1, 0, 2},
                                                                       {0, 0, 0}};
            for (auto &globalOffsets : globalOffsetValues) {
                for (auto globalOffsetKernelFirst : globalOffsetKernelFirstValues) {
                    caseName = testNameMutateKernelsGlobalOffset(globalOffsetKernelFirst, globalOffsets.data());
                    caseResult = testMutateKernelsGlobalOffset(env.get(), module, globalOffsets.data(), globalOffsetKernelFirst, aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberMixedKernelsAndShapes);
                }
            }
        }
    }

    if (testMask.test(bitNumberMixingKernelArgumentsShapesEvents)) {
        std::vector<std::array<uint32_t, 3>> localWorkSizeKernelInitGroupSizeValues = {{16, 2, 1},
                                                                                       {3, 3, 3}};
        std::vector<std::array<uint32_t, 3>> localWorkSizeKernelMutateGroupSizeValues = {{32, 1, 1},
                                                                                         {5, 3, 5}};

        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::none,
                                                                 MclTests::EventOptions::timestamp,
                                                                 MclTests::EventOptions::signalEvent,
                                                                 MclTests::EventOptions::signalEventTimestamp,
                                                                 MclTests::EventOptions::cbEvent,
                                                                 MclTests::EventOptions::cbEventTimestamp,
                                                                 MclTests::EventOptions::cbEventSignal,
                                                                 MclTests::EventOptions::cbEventSignalTimestamp};
        for (auto &localWorkSizeKernelInitGroupSize : localWorkSizeKernelInitGroupSizeValues) {
            for (auto &localWorkSizeKernelMutateGroupSize : localWorkSizeKernelMutateGroupSizeValues) {
                for (auto eventOption : eventOptionValues) {
                    caseName = testNameSameKernelsMutateArgumentsShapesEvents(localWorkSizeKernelInitGroupSize.data(), localWorkSizeKernelMutateGroupSize.data(), eventOption);
                    caseResult = testSameKernelsMutateArgumentsShapesEvents(env.get(), module, localWorkSizeKernelInitGroupSize.data(), localWorkSizeKernelMutateGroupSize.data(), eventOption, aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberMixingKernelArgumentsShapesEvents);
                }
            }
        }
    }

    if (testMask.test(bitNumberMixingKernelsArgumentsShapesEvents)) {
        std::vector<bool> globalOffsetKernelFirstValues = {false, true};
        std::vector<std::array<uint32_t, 3>> globalOffsetValues = {{2, 2, 3},
                                                                   {0, 0, 0}};
        std::vector<std::array<uint32_t, 3>> gwsGroupCountValues = {{16, 2, 4},
                                                                    {4, 2, 4}};
        std::vector<MclTests::EventOptions> eventOptionValues = {MclTests::EventOptions::none,
                                                                 MclTests::EventOptions::timestamp,
                                                                 MclTests::EventOptions::signalEvent,
                                                                 MclTests::EventOptions::signalEventTimestamp,
                                                                 MclTests::EventOptions::cbEvent,

                                                                 MclTests::EventOptions::cbEventTimestamp,
                                                                 MclTests::EventOptions::cbEventSignal,
                                                                 MclTests::EventOptions::cbEventSignalTimestamp};
        for (auto globalOffsetKernelFirst : globalOffsetKernelFirstValues) {
            for (auto &globalOffsets : globalOffsetValues) {
                for (auto &gwsGroupCount : gwsGroupCountValues) {
                    for (auto eventOption : eventOptionValues) {
                        caseName = testNameDifferentKernelsMutateArgumentsShapesEvents(globalOffsetKernelFirst, gwsGroupCount.data(), globalOffsets.data(), eventOption);
                        caseResult = testDifferentKernelsMutateArgumentsShapesEvents(env.get(), module, globalOffsetKernelFirst, gwsGroupCount.data(), globalOffsets.data(), eventOption, aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberMixingKernelsArgumentsShapesEvents);
                    }
                }
            }
        }
    }

    if (testMask.test(bitNumberTestSingleTask)) {
        std::vector<bool> passGroupCountDescValues = {false, true};
        for (auto passGroupCountDesc : passGroupCountDescValues) {
            caseName = testNameMutateSingleTaskKernelArgumentsAndKeepGroupCount(passGroupCountDesc);
            caseResult = testMutateSingleTaskKernelArgumentsAndKeepGroupCount(env.get(), module, passGroupCountDesc, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
            valid &= caseResult;
            MclTests::addFailedCase(caseResult, caseName, bitNumberTestTwoKernels);
        }
    }

    if (testMask.test(bitNumberMixingGlobalBarrierKernels)) {
        if (env->getCooperativeQueueOrdinal() != std::numeric_limits<uint32_t>::max()) {
            std::vector<KernelType> firstKernelValues = {
                KernelType::globalBarrier,
                KernelType::globalBarrierMultiplicationGroupCount,
                KernelType::copy};
            std::vector<KernelType> mutateKernelValues = {
                KernelType::globalBarrier,
                KernelType::globalBarrierMultiplicationGroupCount,
                KernelType::copy};
            std::vector<KernelType> mutateBackKernelValues = {
                KernelType::globalBarrier,
                KernelType::globalBarrierMultiplicationGroupCount,
                KernelType::copy};

            for (auto firstKernel : firstKernelValues) {
                for (auto mutateKernel : mutateKernelValues) {
                    for (auto mutateBackKernel : mutateBackKernelValues) {
                        if (firstKernel == mutateKernel || mutateKernel == mutateBackKernel) {
                            continue;
                        }
                        const std::string baseName = "Global Barrier";
                        caseName = testNameMixingGlobalBarrierKernels(baseName, firstKernel, mutateKernel, mutateBackKernel);
                        caseResult = testMutateGlobalBarrierKernels(env.get(), module, env->getGlobalBarrierModule(), firstKernel, mutateKernel, mutateBackKernel, aubMode);
                        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                        valid &= caseResult;
                        MclTests::addFailedCase(caseResult, caseName, bitNumberMixingGlobalBarrierKernels);
                    }
                }
            }
        } else {
            std::cerr << "Cooperative queues are not supported, skipping tests" << std::endl;
        }
    }

    if (testMask.test(bitNumberMixingSlmKernels)) {
        std::vector<KernelType> firstKernelValues = {
            KernelType::slmKernelOneArgs,
            KernelType::slmKernelTwoArgs,
            KernelType::copy};
        std::vector<KernelType> mutateKernelValues = {
            KernelType::slmKernelOneArgs,
            KernelType::slmKernelTwoArgs,
            KernelType::copy};
        std::vector<KernelType> mutateBackKernelValues = {
            KernelType::slmKernelOneArgs,
            KernelType::slmKernelTwoArgs,
            KernelType::copy};
        for (auto firstKernel : firstKernelValues) {
            for (auto mutateKernel : mutateKernelValues) {
                for (auto mutateBackKernel : mutateBackKernelValues) {
                    if (firstKernel == mutateKernel || mutateKernel == mutateBackKernel) {
                        continue;
                    }
                    caseName = testNameMixingSlmKernels(firstKernel, mutateKernel, mutateBackKernel);
                    caseResult = testMutateSlmKernels(env.get(), module, env->getSlmModule(), firstKernel, mutateKernel, mutateBackKernel, aubMode);
                    LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
                    valid &= caseResult;
                    MclTests::addFailedCase(caseResult, caseName, bitNumberMixingSlmKernels);
                }
            }
        }
    }

    if (testMask.test(bitNumberMutateBack)) {
        bool firstMultiple = false;

        caseName = testNameMutateBackKernel(firstMultiple);
        caseResult = testMutateBackKernel(env.get(), module, firstMultiple, aubMode);
        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
        valid &= caseResult;
        MclTests::addFailedCase(caseResult, caseName, bitNumberMutateBack);

        firstMultiple = true;

        caseName = testNameMutateBackKernel(firstMultiple);
        caseResult = testMutateBackKernel(env.get(), module, firstMultiple, aubMode);
        LevelZeroBlackBoxTests::printResult(aubMode, caseResult, blackBoxName, caseName);
        valid &= caseResult;
        MclTests::addFailedCase(caseResult, caseName, bitNumberMutateBack);
    }

    env->releaseMainModule(module);
    int mainRetCode = aubMode ? 0 : (valid ? 0 : 1);
    std::string finalStatus = (mainRetCode != 0) ? " FAILED" : " SUCCESS";
    std::cerr << blackBoxName << finalStatus << std::endl;
    MclTests::printFailedCases();
    return mainRetCode;
}

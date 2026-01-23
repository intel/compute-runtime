/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/black_box_tests/mcl_samples/zello_mcl_test_helper.h"

#include <cstring>
#include <memory>

bool variableKernelArgumentTest(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    constexpr size_t allocSize = 4096;

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    zex_variable_handle_t varSrc = MclTests::getVariable(cmdList, "source", execEnv);
    zex_variable_handle_t varDst = MclTests::getVariable(cmdList, "destination", execEnv);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    // prepare buffers
    void *srcBuffer1 = nullptr;
    void *srcBuffer2 = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));
    constexpr uint8_t val1 = 0xab;
    constexpr uint8_t val2 = 0xcd;
    memset(srcBuffer1, val1, allocSize);
    memset(srcBuffer2, val2, allocSize);
    memset(dstBuffer, 0, allocSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(copyKernel, 0, varSrc));
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(copyKernel, 1, varDst));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varSrc, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varDst, 0, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    bool valid = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer, allocSize) == false) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cerr << "destination does not match source 1" << std::endl;
            }
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varSrc, 0, sizeof(void *), &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer2, dstBuffer, allocSize) == false) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cerr << "destination does not match source 2" << std::endl;
            }
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);
    return valid;
}

bool variableDispatchTest(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    zex_variable_handle_t varSrc = MclTests::getVariable(cmdList, "source", execEnv);
    zex_variable_handle_t varDst = MclTests::getVariable(cmdList, "destination", execEnv);
    zex_variable_handle_t varGroupSize = MclTests::getVariable(cmdList, "groupSize", execEnv);
    zex_variable_handle_t varGroupCount = MclTests::getVariable(cmdList, "groupCount", execEnv);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(copyKernel, 0, varSrc));
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(copyKernel, 1, varDst));
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetVariableGroupSizeFunc(copyKernel, varGroupSize));

    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendVariableLaunchKernelFunc(cmdList, copyKernel, varGroupCount, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // prepare buffers
    constexpr size_t allocSize1 = 2048;
    void *srcBuffer1 = nullptr;
    void *dstBuffer1 = nullptr;

    constexpr size_t allocSize2 = 4096;
    void *srcBuffer2 = nullptr;
    void *dstBuffer2 = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize1, 1, &srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize2, 1, &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize1, 1, &dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize2, 1, &dstBuffer2));
    constexpr uint8_t val = 0xab;
    memset(srcBuffer1, val, allocSize1);
    memset(dstBuffer1, 0, allocSize1);

    memset(srcBuffer2, val, allocSize2);
    memset(dstBuffer2, 0, allocSize2);

    // set and run
    uint32_t groupSize1[3] = {32, 1, 1};
    uint32_t groupCount1[3] = {static_cast<uint32_t>(allocSize1) / groupSize1[0], 1, 1};
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varSrc, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varDst, 0, sizeof(void *), &dstBuffer1));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varGroupSize, 0, 3 * sizeof(uint32_t), groupSize1));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varGroupCount, 0, 3 * sizeof(uint32_t), groupCount1));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    bool valid = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize1) == false) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cerr << "destination 1 does not match source 1" << std::endl;
            }
            valid = false;
        }
    }

    uint32_t groupSize2[3] = {32, 1, 1};
    uint32_t groupCount2[3] = {static_cast<uint32_t>(allocSize2) / groupSize2[0], 1, 1};
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varSrc, 0, sizeof(void *), &srcBuffer2));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varDst, 0, sizeof(void *), &dstBuffer2));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varGroupSize, 0, 3 * sizeof(uint32_t), groupSize2));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varGroupCount, 0, 3 * sizeof(uint32_t), groupCount2));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer2, dstBuffer2, allocSize2) == false) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cerr << "destination 2 does not match source 2" << std::endl;
            }
            valid = false;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);
    return valid;
}

bool temporaryVariableTest(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    ze_event_pool_handle_t eventPool = nullptr;
    uint32_t numEvents = 1;
    std::vector<ze_event_handle_t> events(numEvents);

    MclTests::createEventPoolAndEvents(*execEnv, eventPool, numEvents, events.data(), MclTests::EventOptions::none);

    zex_variable_handle_t varSrc = MclTests::getVariable(cmdList, "source", execEnv);
    zex_variable_handle_t varDst = MclTests::getVariable(cmdList, "destination", execEnv);
    zex_variable_handle_t varTmpBuffer = MclTests::getTmpVariable(cmdList, 1U, true, execEnv);
    zex_variable_handle_t varGroupSize = MclTests::getVariable(cmdList, "groupSize", execEnv);
    zex_variable_handle_t varGroupCount = MclTests::getVariable(cmdList, "groupCount", execEnv);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");

    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(copyKernel, 0, varSrc));
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(copyKernel, 1, varTmpBuffer));
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetVariableGroupSizeFunc(copyKernel, varGroupSize));
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendVariableLaunchKernelFunc(cmdList, copyKernel, varGroupCount, events[0], 0, nullptr));

    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(copyKernel, 0, varTmpBuffer));
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(copyKernel, 1, varDst));
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendVariableLaunchKernelFunc(cmdList, copyKernel, varGroupCount, nullptr, 1, &events[0]));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // prepare buffers
    constexpr size_t allocSize = 2048;
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, allocSize, 1, &dstBuffer));
    constexpr uint8_t val = 0xab;
    memset(srcBuffer, val, allocSize);
    memset(dstBuffer, 0, allocSize);

    // set and run
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListTempMemSetEleCountFunc(cmdList, allocSize));
    size_t tempMemSize = 0U;
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListTempMemGetSizeFunc(cmdList, &tempMemSize));
    void *tempMem = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(execEnv->context, &deviceDesc, tempMemSize, 1, execEnv->device, &tempMem));
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListTempMemSetFunc(cmdList, &tempMem));

    uint32_t groupSize[3] = {32, 1, 1};
    uint32_t groupCount[3] = {static_cast<uint32_t>(allocSize) / groupSize[0], 1, 1};
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varSrc, 0, sizeof(void *), &srcBuffer));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varDst, 0, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varGroupSize, 0, 3 * sizeof(uint32_t), groupSize));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varGroupCount, 0, 3 * sizeof(uint32_t), groupCount));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    bool valid = true;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer, dstBuffer, allocSize) == false) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cerr << "destination does not match source" << std::endl;
            }
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, tempMem));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);
    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    return valid;
}

bool controlFlowIfStatementTest(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);
    auto varGs = MclTests::getVariable(cmdList, "groupSize", execEnv);
    auto varGc = MclTests::getVariable(cmdList, "groupCount", execEnv);
    auto varBuffer = MclTests::getVariable(cmdList, "buffer", execEnv);
    auto varControl = MclTests::getVariable(cmdList, "controlVariable", execEnv);
    auto labelEnd = MclTests::getLabel(cmdList, "end", execEnv);

    ze_kernel_handle_t addOneKernel = execEnv->getKernelHandle(module, "addOneKernel");
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetVariableGroupSizeFunc(addOneKernel, varGs));
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(addOneKernel, 0, varBuffer));

    // cmdlist
    zex_operand_desc_t condition = {ZEX_STRUCTURE_TYPE_OPERAND_DESCRIPTOR};
    condition.memory = varControl;
    condition.offset = 0U;
    condition.flags = ZEX_OPERAND_FLAG_USES_VARIABLE;
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendJumpFunc(cmdList, labelEnd, &condition));

    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendVariableLaunchKernelFunc(cmdList, addOneKernel, varGc, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListSetLabelFunc(cmdList, labelEnd));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // prepare buffers
    constexpr size_t bufferSize = 2048;

    void *controlFlowBuffer = nullptr;
    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocShared(execEnv->context, &deviceDesc, &hostDesc, 4, 1, execEnv->device, &controlFlowBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocShared(execEnv->context, &deviceDesc, &hostDesc, bufferSize, 1, execEnv->device, &dstBuffer));
    memset(dstBuffer, 0, bufferSize);

    // Set and run
    uint32_t groupSize[3] = {32, 1, 1};
    uint32_t groupCount[3] = {static_cast<uint32_t>(bufferSize) / groupSize[0], 1, 1};
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varBuffer, 0, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varGs, 0, 3 * sizeof(uint32_t), groupSize));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varGc, 0, 3 * sizeof(uint32_t), groupCount));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varControl, 0, sizeof(void *), &controlFlowBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    bool valid = true;
    memset(controlFlowBuffer, 0, 4); // no jump
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    uint8_t *bufferChar = reinterpret_cast<uint8_t *>(dstBuffer);
    if (!aubMode) {
        for (size_t i = 0; i < bufferSize; i++) {
            uint32_t value = *(bufferChar++);
            if (value != 1u) {
                if (LevelZeroBlackBoxTests::verbose) {
                    std::cerr << "destination value " << value << " does not match 1 at " << i << std::endl;
                }
                valid = false;
                break;
            }
        }
    }
    memset(dstBuffer, 0, bufferSize);
    memset(controlFlowBuffer, 1, 4); // jump to end
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    bufferChar = reinterpret_cast<uint8_t *>(dstBuffer);
    if (!aubMode) {
        for (size_t i = 0; i < bufferSize; i++) {
            uint32_t value = *(bufferChar++);
            if (value != 0u) {
                if (LevelZeroBlackBoxTests::verbose) {
                    std::cerr << "destination value " << value << " does not match 0 at " << i << std::endl;
                }
                valid = false;
                break;
            }
        }
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, controlFlowBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(addOneKernel);
    return valid;
}

bool controlFlowWhileLoopTest(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);
    auto varGs = MclTests::getVariable(cmdList, "groupSize", execEnv);
    auto varGc = MclTests::getVariable(cmdList, "groupCount", execEnv);
    auto varBuffer = MclTests::getVariable(cmdList, "buffer", execEnv);
    auto varCounter = MclTests::getVariable(cmdList, "counter", execEnv);
    auto labelEnd = MclTests::getLabel(cmdList, "end", execEnv);
    auto labelBegin = MclTests::getLabel(cmdList, "begin", execEnv);

    ze_kernel_handle_t addOneKernel = execEnv->getKernelHandle(module, "addOneKernel");
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetVariableGroupSizeFunc(addOneKernel, varGs));
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(addOneKernel, 0, varBuffer));

    // cmdlist
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListSetLabelFunc(cmdList, labelBegin));

    // GPR0 = (counter == 0)
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendLoadRegVariableFunc(cmdList, zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_GPR0, varCounter));
    zex_mcl_alu_operation_t checkCtr[] = {{zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_XOR,
                                           zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_GPR0,
                                           zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_CONST0,
                                           zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_GPR0,
                                           zex_mcl_alu_flag_t::ZE_MCL_ALU_FLAG_ZF}};
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendMIMathFunc(cmdList, checkCtr, 1));

    zex_operand_desc_t finishCondition = {ZEX_STRUCTURE_TYPE_OPERAND_DESCRIPTOR};
    finishCondition.memory = nullptr;
    finishCondition.offset = zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_GPR0;
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendJumpFunc(cmdList, labelEnd, &finishCondition));

    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendVariableLaunchKernelFunc(cmdList, addOneKernel, varGc, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // GPR0 = GPR0 - 1
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendLoadRegVariableFunc(cmdList, zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_GPR0, varCounter));
    zex_mcl_alu_operation_t miDecCounter[] = {{zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_ADD,
                                               zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_GPR0,
                                               zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_GPR0,
                                               zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_CONST1,
                                               zex_mcl_alu_flag_t::ZE_MCL_ALU_FLAG_ACC}};
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendMIMathFunc(cmdList, miDecCounter, 1));
    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendStoreRegVariableFunc(cmdList, zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_GPR0, varCounter));

    SUCCESS_OR_TERMINATE(execEnv->zexCommandListAppendJumpFunc(cmdList, labelBegin, nullptr));

    SUCCESS_OR_TERMINATE(execEnv->zexCommandListSetLabelFunc(cmdList, labelEnd));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // prepare buffers
    constexpr size_t bufferSize = 2048;

    void *ctrBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, 8, 1, &ctrBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(execEnv->context, &hostDesc, bufferSize, 1, &dstBuffer));
    memset(dstBuffer, 0, bufferSize);
    memset(ctrBuffer, 0, 8);
    auto ctrBufferInt = reinterpret_cast<uint32_t *>(ctrBuffer);

    // Set and run
    constexpr uint32_t counter = 10;
    *ctrBufferInt = counter;
    uint32_t groupSize[3] = {32, 1, 1};
    uint32_t groupCount[3] = {static_cast<uint32_t>(bufferSize) / groupSize[0], 1, 1};
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varBuffer, 0, sizeof(void *), &dstBuffer));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varGs, 0, 3 * sizeof(uint32_t), groupSize));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varGc, 0, 3 * sizeof(uint32_t), groupCount));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varCounter, 0, sizeof(void *), &ctrBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    bool valid = true;
    if (!aubMode) {
        uint8_t *bufferChar = reinterpret_cast<uint8_t *>(dstBuffer);
        for (size_t i = 0; i < bufferSize; i++) {
            uint32_t value = *(bufferChar++);
            if (value != counter) {
                if (LevelZeroBlackBoxTests::verbose) {
                    std::cerr << "destination value " << value << " does not match counter " << counter << " at " << i << std::endl;
                }
                valid = false;
                break;
            }
        }
        if (*ctrBufferInt != 0) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cerr << "control value " << *ctrBufferInt << " does not match 0" << std::endl;
            }
            valid = false;
        }
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, ctrBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(addOneKernel);
    return valid;
}

bool variableKernelMultipleArgumentsTest(MclTests::ExecEnv *execEnv, ze_module_handle_t module, bool aubMode) {
    constexpr size_t allocSize = 4096;

    // prepare
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(execEnv->context, execEnv->device, cmdList, false, false);

    zex_variable_handle_t varSrc1 = MclTests::getVariable(cmdList, "source1", execEnv);
    zex_variable_handle_t varDst1 = MclTests::getVariable(cmdList, "destination1", execEnv);

    zex_variable_handle_t varSrc2 = MclTests::getVariable(cmdList, "source2", execEnv);
    zex_variable_handle_t varDst2 = MclTests::getVariable(cmdList, "destination2", execEnv);

    ze_kernel_handle_t copyKernel = execEnv->getKernelHandle(module, "copyKernel");
    ze_kernel_handle_t addSourceAndOneKernel = execEnv->getKernelHandle(module, "addSourceAndOneKernel");

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
    constexpr uint8_t val1 = 0xab;
    constexpr uint8_t val2 = 0xcd;
    memset(srcBuffer1, val1, allocSize);
    memset(srcBuffer2, val2, allocSize);
    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(copyKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(copyKernel, 0, varSrc1));
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(copyKernel, 1, varDst1));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, copyKernel, &dispatchTraits, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(addSourceAndOneKernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(addSourceAndOneKernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(addSourceAndOneKernel, 0, varSrc2));
    SUCCESS_OR_TERMINATE(execEnv->zexKernelSetArgumentVariableFunc(addSourceAndOneKernel, 1, varDst2));

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, addSourceAndOneKernel, &dispatchTraits, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    // set and run
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varSrc1, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varDst1, 0, sizeof(void *), &dstBuffer1));

    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varSrc2, 0, sizeof(void *), &srcBuffer1));
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varDst2, 0, sizeof(void *), &dstBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));
    bool valid = true;
    uint8_t expected = 0;
    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cerr << "before mutation - copy kernel: destination 1 does not match source 1" << std::endl;
            }
            valid = false;
        }

        expected = val1 + 1;

        if (LevelZeroBlackBoxTests::validateToValue<uint8_t>(expected, dstBuffer2, allocSize) == false) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cerr << "before mutation - add source one kernel: destination 2 does not match source 1" << std::endl;
            }
            valid = false;
        }
    }

    memset(dstBuffer1, 0, allocSize);
    memset(dstBuffer2, 0, allocSize);

    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(varSrc2, 0, sizeof(void *), &srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(execEnv->queue, 1, &cmdList, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(execEnv->queue, std::numeric_limits<uint64_t>::max()));

    if (!aubMode) {
        if (LevelZeroBlackBoxTests::validate(srcBuffer1, dstBuffer1, allocSize) == false) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cerr << "after mutation - copy kernel: destination 1 does not match source 1" << std::endl;
            }
            valid = false;
        }

        expected = val2 + 1;

        if (LevelZeroBlackBoxTests::validateToValue<uint8_t>(expected, dstBuffer2, allocSize) == false) {
            if (LevelZeroBlackBoxTests::verbose) {
                std::cerr << "after mutation - add source one kernel: destination 2 does not match source 2" << std::endl;
            }
            valid = false;
        }
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(execEnv->context, srcBuffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    execEnv->destroyKernelHandle(copyKernel);
    execEnv->destroyKernelHandle(addSourceAndOneKernel);
    return valid;
}

int main(int argc, char *argv[]) {
    constexpr uint32_t bitNumberTestMemoryVariableKernelArgument = 0u;
    constexpr uint32_t bitNumberTestVariableDispatch = 1u;
    constexpr uint32_t bitNumberTestTemporaryVariable = 2u;
    constexpr uint32_t bitNumberTestControlFlowIfStatement = 3u;
    constexpr uint32_t bitNumberTestControlFlowWhileLoop = 4u;
    constexpr uint32_t bitNumberTestMemoryVariableMultipleKernelArguments = 5u;

    const std::string blackBoxName = "Zello MCL Samples";

    LevelZeroBlackBoxTests::TestBitMask testMask = LevelZeroBlackBoxTests::getTestMask(argc, argv, std::numeric_limits<uint32_t>::max());
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    auto env = std::unique_ptr<MclTests::ExecEnv>(MclTests::ExecEnv::create());
    env->setGlobalKernels(argc, argv);

    ze_module_handle_t module;
    env->buildMainModule(module);

    bool valid = true;
    bool testCase = true;
    std::string caseName = "";

    if (testMask.test(bitNumberTestMemoryVariableKernelArgument)) {
        testCase = variableKernelArgumentTest(env.get(), module, aubMode);
        caseName = "Variable Kernel Argument Test";
        LevelZeroBlackBoxTests::printResult(aubMode, testCase, blackBoxName, caseName);
        valid &= testCase;
    }

    if (testMask.test(bitNumberTestVariableDispatch)) {
        testCase = variableDispatchTest(env.get(), module, aubMode);
        caseName = "Variable Dispatch Test";
        LevelZeroBlackBoxTests::printResult(aubMode, testCase, blackBoxName, caseName);
        valid &= testCase;
    }

    if (testMask.test(bitNumberTestTemporaryVariable)) {
        testCase = temporaryVariableTest(env.get(), module, aubMode);
        caseName = "Temporary Variable Test";
        LevelZeroBlackBoxTests::printResult(aubMode, testCase, blackBoxName, caseName);
        valid &= testCase;
    }

    if (testMask.test(bitNumberTestControlFlowIfStatement)) {
        testCase = controlFlowIfStatementTest(env.get(), module, aubMode);
        caseName = "Control Flow If Statement Test";
        LevelZeroBlackBoxTests::printResult(aubMode, testCase, blackBoxName, caseName);
        valid &= testCase;
    }

    if (testMask.test(bitNumberTestControlFlowWhileLoop)) {
        testCase = controlFlowWhileLoopTest(env.get(), module, aubMode);
        caseName = "Control Flow While Loop Test";
        LevelZeroBlackBoxTests::printResult(aubMode, testCase, blackBoxName, caseName);
        valid &= testCase;
    }

    if (testMask.test(bitNumberTestMemoryVariableMultipleKernelArguments)) {
        testCase = variableKernelMultipleArgumentsTest(env.get(), module, aubMode);
        caseName = "Variable Multiple Kernel Arguments Test";
        LevelZeroBlackBoxTests::printResult(aubMode, testCase, blackBoxName, caseName);
        valid &= testCase;
    }

    env->releaseMainModule(module);
    int mainRetCode = aubMode ? 0 : (valid ? 0 : 1);
    std::string finalStatus = (mainRetCode != 0) ? " FAILED" : " SUCCESS";
    std::cerr << blackBoxName << finalStatus << std::endl;
    return mainRetCode;
}

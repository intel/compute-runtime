/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <iostream>

namespace {

using ze_pfnCommandQueueGetOrdinal_t = ze_result_t(ZE_APICALL *)(ze_command_queue_handle_t hCommandQueue, uint32_t *pOrdinal);
using ze_pfnCommandQueueGetIndex_t = ze_result_t(ZE_APICALL *)(ze_command_queue_handle_t hCommandQueue, uint32_t *pIndex);
using ze_pfnCommandQueueGetFlags_t = ze_result_t(ZE_APICALL *)(ze_command_queue_handle_t hCommandQueue, ze_command_queue_flags_t *pFlags);
using ze_pfnCommandQueueGetMode_t = ze_result_t(ZE_APICALL *)(ze_command_queue_handle_t hCommandQueue, ze_command_queue_mode_t *pMode);
using ze_pfnCommandQueueGetPriority_t = ze_result_t(ZE_APICALL *)(ze_command_queue_handle_t hCommandQueue, ze_command_queue_priority_t *pPriority);

using ze_pfnCommandListGetDeviceHandle_t = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList, ze_device_handle_t *phDevice);
using ze_pfnCommandListGetContextHandle_t = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList, ze_context_handle_t *phContext);
using ze_pfnCommandListGetOrdinal_t = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList, uint32_t *pOrdinal);
using ze_pfnCommandListGetFlags_t = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList, ze_command_list_flags_t *pFlags);
using ze_pfnCommandListImmediateGetIndex_t = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandListImmediate, uint32_t *pIndex);
using ze_pfnCommandListImmediateGetFlags_t = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandListImmediate, ze_command_queue_flags_t *pFlags);
using ze_pfnCommandListImmediateGetMode_t = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandListImmediate, ze_command_queue_mode_t *pMode);
using ze_pfnCommandListImmediateGetPriority_t = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandListImmediate, ze_command_queue_priority_t *pPriority);
using ze_pfnCommandListIsImmediate_t = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList, ze_bool_t *pIsImmediate);
using ze_pfnCommandListIsMutableExp_t = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList, ze_bool_t *pIsMutable);

struct QueryApiFunctions {
    ze_pfnCommandQueueGetOrdinal_t commandQueueGetOrdinal = nullptr;
    ze_pfnCommandQueueGetIndex_t commandQueueGetIndex = nullptr;
    ze_pfnCommandQueueGetFlags_t commandQueueGetFlags = nullptr;
    ze_pfnCommandQueueGetMode_t commandQueueGetMode = nullptr;
    ze_pfnCommandQueueGetPriority_t commandQueueGetPriority = nullptr;

    ze_pfnCommandListGetDeviceHandle_t commandListGetDeviceHandle = nullptr;
    ze_pfnCommandListGetContextHandle_t commandListGetContextHandle = nullptr;
    ze_pfnCommandListGetOrdinal_t commandListGetOrdinal = nullptr;
    ze_pfnCommandListGetFlags_t commandListGetFlags = nullptr;
    ze_pfnCommandListImmediateGetIndex_t commandListImmediateGetIndex = nullptr;
    ze_pfnCommandListImmediateGetFlags_t commandListImmediateGetFlags = nullptr;
    ze_pfnCommandListImmediateGetMode_t commandListImmediateGetMode = nullptr;
    ze_pfnCommandListImmediateGetPriority_t commandListImmediateGetPriority = nullptr;
    ze_pfnCommandListIsImmediate_t commandListIsImmediate = nullptr;
    ze_pfnCommandListIsMutableExp_t commandListIsMutableExp = nullptr;
};

template <typename FuncType>
void loadFunction(ze_driver_handle_t driverHandle, const char *functionName, FuncType &functionPtr) {
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, functionName, reinterpret_cast<void **>(&functionPtr)));
    SUCCESS_OR_TERMINATE_BOOL(functionPtr != nullptr);
}

void loadQueryFunctions(ze_driver_handle_t driverHandle, QueryApiFunctions &functions) {
    loadFunction(driverHandle, "zeCommandQueueGetOrdinal", functions.commandQueueGetOrdinal);
    loadFunction(driverHandle, "zeCommandQueueGetIndex", functions.commandQueueGetIndex);
    loadFunction(driverHandle, "zeCommandQueueGetFlags", functions.commandQueueGetFlags);
    loadFunction(driverHandle, "zeCommandQueueGetMode", functions.commandQueueGetMode);
    loadFunction(driverHandle, "zeCommandQueueGetPriority", functions.commandQueueGetPriority);

    loadFunction(driverHandle, "zeCommandListGetDeviceHandle", functions.commandListGetDeviceHandle);
    loadFunction(driverHandle, "zeCommandListGetContextHandle", functions.commandListGetContextHandle);
    loadFunction(driverHandle, "zeCommandListGetOrdinal", functions.commandListGetOrdinal);
    loadFunction(driverHandle, "zeCommandListGetFlags", functions.commandListGetFlags);
    loadFunction(driverHandle, "zeCommandListImmediateGetIndex", functions.commandListImmediateGetIndex);
    loadFunction(driverHandle, "zeCommandListImmediateGetFlags", functions.commandListImmediateGetFlags);
    loadFunction(driverHandle, "zeCommandListImmediateGetMode", functions.commandListImmediateGetMode);
    loadFunction(driverHandle, "zeCommandListImmediateGetPriority", functions.commandListImmediateGetPriority);
    loadFunction(driverHandle, "zeCommandListIsImmediate", functions.commandListIsImmediate);
    loadFunction(driverHandle, "zeCommandListIsMutableExp", functions.commandListIsMutableExp);
}

} // namespace

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Query APIs";

    bool outputValidationSuccessful = true;
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    QueryApiFunctions functions{};
    loadQueryFunctions(driverHandle, functions);

    const auto expect = [&](bool condition, const char *message) {
        if (condition) {
            return;
        }
        outputValidationSuccessful = false;
        if (LevelZeroBlackBoxTests::verbose) {
            std::cerr << "Validation failed: " << message << std::endl;
        }
    };

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    ze_command_queue_handle_t cmdQueue = nullptr;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    uint32_t queueOrdinal = 7U;
    uint32_t queueIndex = 7U;
    ze_command_queue_flags_t queueFlags = 7;
    ze_command_queue_mode_t queueMode = ZE_COMMAND_QUEUE_MODE_FORCE_UINT32;
    ze_command_queue_priority_t queuePriority = ZE_COMMAND_QUEUE_PRIORITY_FORCE_UINT32;

    SUCCESS_OR_TERMINATE(functions.commandQueueGetOrdinal(cmdQueue, &queueOrdinal));
    SUCCESS_OR_TERMINATE(functions.commandQueueGetIndex(cmdQueue, &queueIndex));
    SUCCESS_OR_TERMINATE(functions.commandQueueGetFlags(cmdQueue, &queueFlags));
    SUCCESS_OR_TERMINATE(functions.commandQueueGetMode(cmdQueue, &queueMode));
    SUCCESS_OR_TERMINATE(functions.commandQueueGetPriority(cmdQueue, &queuePriority));

    expect(queueOrdinal == cmdQueueDesc.ordinal, "queue ordinal mismatch");
    expect(queueIndex == cmdQueueDesc.index, "queue index mismatch");
    expect(queueFlags == cmdQueueDesc.flags, "queue flags mismatch");
    expect(queueMode == cmdQueueDesc.mode, "queue mode mismatch");
    expect(queuePriority == cmdQueueDesc.priority, "queue priority mismatch");

    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.commandQueueGroupOrdinal = cmdQueueDesc.ordinal;
    cmdListDesc.flags = ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING;

    ze_command_list_handle_t cmdList = nullptr;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    ze_device_handle_t cmdListDevice = nullptr;
    ze_context_handle_t cmdListContext = nullptr;
    uint32_t cmdListOrdinal = 7U;
    ze_command_list_flags_t cmdListFlags = ZE_COMMAND_LIST_FLAG_FORCE_UINT32;
    ze_bool_t isImmediate = static_cast<ze_bool_t>(1);
    ze_bool_t isMutable = static_cast<ze_bool_t>(1);

    SUCCESS_OR_TERMINATE(functions.commandListGetDeviceHandle(cmdList, &cmdListDevice));
    SUCCESS_OR_TERMINATE(functions.commandListGetContextHandle(cmdList, &cmdListContext));
    SUCCESS_OR_TERMINATE(functions.commandListGetOrdinal(cmdList, &cmdListOrdinal));
    SUCCESS_OR_TERMINATE(functions.commandListGetFlags(cmdList, &cmdListFlags));
    SUCCESS_OR_TERMINATE(functions.commandListIsImmediate(cmdList, &isImmediate));
    SUCCESS_OR_TERMINATE(functions.commandListIsMutableExp(cmdList, &isMutable));

    expect(cmdListDevice == device, "command list device mismatch");
    expect(cmdListContext == context, "command list context mismatch");
    expect(cmdListOrdinal == cmdListDesc.commandQueueGroupOrdinal, "command list ordinal mismatch");
    expect(cmdListFlags == cmdListDesc.flags, "command list flags mismatch");
    expect(isImmediate == static_cast<ze_bool_t>(0), "regular command list isImmediate mismatch");
    expect(isMutable == static_cast<ze_bool_t>(0), "regular command list isMutable mismatch");

    uint32_t immediateIndex = 99U;
    ze_command_queue_flags_t immediateFlags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
    ze_command_queue_mode_t immediateMode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_command_queue_priority_t immediatePriority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
    auto immediateIndexResult = functions.commandListImmediateGetIndex(cmdList, &immediateIndex);
    auto immediateFlagsResult = functions.commandListImmediateGetFlags(cmdList, &immediateFlags);
    auto immediateModeResult = functions.commandListImmediateGetMode(cmdList, &immediateMode);
    auto immediatePriorityResult = functions.commandListImmediateGetPriority(cmdList, &immediatePriority);
    expect(immediateIndexResult == ZE_RESULT_ERROR_INVALID_ARGUMENT, "regular command list immediate index should return invalid argument");
    expect(immediateFlagsResult == ZE_RESULT_ERROR_INVALID_ARGUMENT, "regular command list immediate flags should return invalid argument");
    expect(immediateModeResult == ZE_RESULT_ERROR_INVALID_ARGUMENT, "regular command list immediate mode should return invalid argument");
    expect(immediatePriorityResult == ZE_RESULT_ERROR_INVALID_ARGUMENT, "regular command list immediate priority should return invalid argument");

    ze_command_queue_desc_t immediateCmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    immediateCmdListDesc.ordinal = cmdQueueDesc.ordinal;
    immediateCmdListDesc.index = cmdQueueDesc.index;
    immediateCmdListDesc.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
    immediateCmdListDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    immediateCmdListDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    ze_command_list_handle_t immediateCmdList = nullptr;
    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &immediateCmdListDesc, &immediateCmdList));

    ze_device_handle_t immediateCmdListDevice = nullptr;
    ze_context_handle_t immediateCmdListContext = nullptr;
    uint32_t immediateCmdListOrdinal = 7U;
    ze_command_list_flags_t immediateCmdListFlags = ZE_COMMAND_LIST_FLAG_FORCE_UINT32;
    ze_bool_t immediateCmdListIsImmediate = static_cast<ze_bool_t>(0);
    ze_bool_t immediateCmdListIsMutable = static_cast<ze_bool_t>(1);

    SUCCESS_OR_TERMINATE(functions.commandListGetDeviceHandle(immediateCmdList, &immediateCmdListDevice));
    SUCCESS_OR_TERMINATE(functions.commandListGetContextHandle(immediateCmdList, &immediateCmdListContext));
    SUCCESS_OR_TERMINATE(functions.commandListGetOrdinal(immediateCmdList, &immediateCmdListOrdinal));
    SUCCESS_OR_TERMINATE(functions.commandListGetFlags(immediateCmdList, &immediateCmdListFlags));
    SUCCESS_OR_TERMINATE(functions.commandListIsImmediate(immediateCmdList, &immediateCmdListIsImmediate));
    SUCCESS_OR_TERMINATE(functions.commandListIsMutableExp(immediateCmdList, &immediateCmdListIsMutable));

    expect(immediateCmdListDevice == device, "immediate command list device mismatch");
    expect(immediateCmdListContext == context, "immediate command list context mismatch");
    expect(immediateCmdListOrdinal == immediateCmdListDesc.ordinal, "immediate command list ordinal mismatch");
    expect(immediateCmdListFlags == 0, "immediate command list flags should be 0");
    expect(immediateCmdListIsImmediate == static_cast<ze_bool_t>(1), "immediate command list isImmediate mismatch");
    expect(immediateCmdListIsMutable == static_cast<ze_bool_t>(0), "immediate command list isMutable mismatch");

    uint32_t queriedImmediateIndex = 7U;
    ze_command_queue_flags_t queriedImmediateFlags = ZE_COMMAND_QUEUE_FLAG_FORCE_UINT32;
    ze_command_queue_mode_t queriedImmediateMode = ZE_COMMAND_QUEUE_MODE_FORCE_UINT32;
    ze_command_queue_priority_t queriedImmediatePriority = ZE_COMMAND_QUEUE_PRIORITY_FORCE_UINT32;

    SUCCESS_OR_TERMINATE(functions.commandListImmediateGetIndex(immediateCmdList, &queriedImmediateIndex));
    SUCCESS_OR_TERMINATE(functions.commandListImmediateGetFlags(immediateCmdList, &queriedImmediateFlags));
    SUCCESS_OR_TERMINATE(functions.commandListImmediateGetMode(immediateCmdList, &queriedImmediateMode));
    SUCCESS_OR_TERMINATE(functions.commandListImmediateGetPriority(immediateCmdList, &queriedImmediatePriority));

    expect(queriedImmediateIndex == immediateCmdListDesc.index, "immediate command list index mismatch");
    expect(queriedImmediateFlags == immediateCmdListDesc.flags, "immediate command list queue flags mismatch");
    expect(queriedImmediateMode == immediateCmdListDesc.mode, "immediate command list queue mode mismatch");
    expect(queriedImmediatePriority == immediateCmdListDesc.priority, "immediate command list queue priority mismatch");

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(immediateCmdList));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;

    return (outputValidationSuccessful ? 0 : 1);
}

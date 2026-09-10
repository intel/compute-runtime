/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <iostream>

namespace {

using ze_pfnEventGetCounterBasedFlags_t = ze_result_t(ZE_APICALL *)(ze_event_handle_t hEvent, ze_event_counter_based_flags_t *pFlags);

using ze_pfnKernelGetModuleHandle_t = ze_result_t(ZE_APICALL *)(ze_kernel_handle_t hKernel, ze_module_handle_t *phModule);
using ze_pfnModuleGetDeviceHandle_t = ze_result_t(ZE_APICALL *)(ze_module_handle_t hModule, ze_device_handle_t *phDevice);

struct QueryApiFunctions {
    ze_pfnEventGetCounterBasedFlags_t eventGetCounterBasedFlags = nullptr;

    ze_pfnKernelGetModuleHandle_t kernelGetModuleHandle = nullptr;
    ze_pfnModuleGetDeviceHandle_t moduleGetDeviceHandle = nullptr;
};

template <typename FuncType>
void loadFunction(ze_driver_handle_t driverHandle, const char *functionName, FuncType &functionPtr) {
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, functionName, reinterpret_cast<void **>(&functionPtr)));
    SUCCESS_OR_TERMINATE_BOOL(functionPtr != nullptr);
}

void loadQueryFunctions(ze_driver_handle_t driverHandle, QueryApiFunctions &functions) {
    loadFunction(driverHandle, "zeEventGetCounterBasedFlags", functions.eventGetCounterBasedFlags);

    loadFunction(driverHandle, "zeKernelGetModuleHandleExt", functions.kernelGetModuleHandle);
    loadFunction(driverHandle, "zeModuleGetDeviceHandleExt", functions.moduleGetDeviceHandle);
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

    SUCCESS_OR_TERMINATE(zeCommandQueueGetOrdinal(cmdQueue, &queueOrdinal));
    SUCCESS_OR_TERMINATE(zeCommandQueueGetIndex(cmdQueue, &queueIndex));
    SUCCESS_OR_TERMINATE(zeCommandQueueGetFlags(cmdQueue, &queueFlags));
    SUCCESS_OR_TERMINATE(zeCommandQueueGetMode(cmdQueue, &queueMode));
    SUCCESS_OR_TERMINATE(zeCommandQueueGetPriority(cmdQueue, &queuePriority));

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

    SUCCESS_OR_TERMINATE(zeCommandListGetDeviceHandle(cmdList, &cmdListDevice));
    SUCCESS_OR_TERMINATE(zeCommandListGetContextHandle(cmdList, &cmdListContext));
    SUCCESS_OR_TERMINATE(zeCommandListGetOrdinal(cmdList, &cmdListOrdinal));
    SUCCESS_OR_TERMINATE(zeCommandListGetFlags(cmdList, &cmdListFlags));
    SUCCESS_OR_TERMINATE(zeCommandListIsImmediate(cmdList, &isImmediate));
    SUCCESS_OR_TERMINATE(zeCommandListIsMutableExp(cmdList, &isMutable));

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
    auto immediateIndexResult = zeCommandListImmediateGetIndex(cmdList, &immediateIndex);
    auto immediateFlagsResult = zeCommandListImmediateGetFlags(cmdList, &immediateFlags);
    auto immediateModeResult = zeCommandListImmediateGetMode(cmdList, &immediateMode);
    auto immediatePriorityResult = zeCommandListImmediateGetPriority(cmdList, &immediatePriority);
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

    SUCCESS_OR_TERMINATE(zeCommandListGetDeviceHandle(immediateCmdList, &immediateCmdListDevice));
    SUCCESS_OR_TERMINATE(zeCommandListGetContextHandle(immediateCmdList, &immediateCmdListContext));
    SUCCESS_OR_TERMINATE(zeCommandListGetOrdinal(immediateCmdList, &immediateCmdListOrdinal));
    SUCCESS_OR_TERMINATE(zeCommandListGetFlags(immediateCmdList, &immediateCmdListFlags));
    SUCCESS_OR_TERMINATE(zeCommandListIsImmediate(immediateCmdList, &immediateCmdListIsImmediate));
    SUCCESS_OR_TERMINATE(zeCommandListIsMutableExp(immediateCmdList, &immediateCmdListIsMutable));

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

    SUCCESS_OR_TERMINATE(zeCommandListImmediateGetIndex(immediateCmdList, &queriedImmediateIndex));
    SUCCESS_OR_TERMINATE(zeCommandListImmediateGetFlags(immediateCmdList, &queriedImmediateFlags));
    SUCCESS_OR_TERMINATE(zeCommandListImmediateGetMode(immediateCmdList, &queriedImmediateMode));
    SUCCESS_OR_TERMINATE(zeCommandListImmediateGetPriority(immediateCmdList, &queriedImmediatePriority));

    expect(queriedImmediateIndex == immediateCmdListDesc.index, "immediate command list index mismatch");
    expect(queriedImmediateFlags == immediateCmdListDesc.flags, "immediate command list queue flags mismatch");
    expect(queriedImmediateMode == immediateCmdListDesc.mode, "immediate command list queue mode mismatch");
    expect(queriedImmediatePriority == immediateCmdListDesc.priority, "immediate command list queue priority mismatch");

    ze_event_counter_based_desc_t counterBasedEventDesc{
        .stype = ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC,
        .pNext = nullptr,
        .flags = ZE_EVENT_COUNTER_BASED_FLAG_NON_IMMEDIATE};
    ze_event_handle_t hCounterBasedEvent{};
    SUCCESS_OR_TERMINATE(zeEventCounterBasedCreate(context, device, &counterBasedEventDesc, &hCounterBasedEvent));

    ze_event_counter_based_flags_t counterBasedEventFlags{};
    SUCCESS_OR_TERMINATE(functions.eventGetCounterBasedFlags(hCounterBasedEvent, &counterBasedEventFlags));

    expect(counterBasedEventFlags != 0, "counter based events have a non-zero flags value");

    SUCCESS_OR_TERMINATE(zeEventDestroy(hCounterBasedEvent));

    ze_module_handle_t module = nullptr;
    LevelZeroBlackBoxTests::createModuleFromSpirV(context, device, LevelZeroBlackBoxTests::openCLKernelsSource, module);

    ze_kernel_handle_t kernel = nullptr;
    LevelZeroBlackBoxTests::createKernelWithName(module, "memcpy_bytes", kernel);

    ze_module_handle_t queriedModule = nullptr;
    SUCCESS_OR_TERMINATE(functions.kernelGetModuleHandle(kernel, &queriedModule));
    expect(queriedModule == module, "kernel parent module mismatch");

    ze_device_handle_t queriedModuleDevice = nullptr;
    SUCCESS_OR_TERMINATE(functions.moduleGetDeviceHandle(module, &queriedModuleDevice));
    expect(queriedModuleDevice == device, "module device mismatch");

    queriedModule = nullptr;
    queriedModuleDevice = nullptr;

    SUCCESS_OR_TERMINATE(zeKernelGetModuleHandle(kernel, &queriedModule));
    expect(queriedModule == module, "kernel parent module mismatch (zeKernelGetModuleHandle)");

    SUCCESS_OR_TERMINATE(zeModuleGetDeviceHandle(module, &queriedModuleDevice));
    expect(queriedModuleDevice == device, "module device mismatch (zeModuleGetDeviceHandle)");

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(immediateCmdList));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;

    return (outputValidationSuccessful ? 0 : 1);
}

/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_graph.h"

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>

using zeGraphCreateExpFP = ze_result_t(ZE_APICALL *)(ze_context_handle_t context, ze_graph_handle_t *phGraph, void *pNext);
using zeCommandListBeginGraphCaptureExpFP = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList, void *pNext);
using zeCommandListBeginCaptureIntoGraphExpFP = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList, ze_graph_handle_t hGraph, void *pNext);
using zeCommandListEndGraphCaptureExpFP = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph, void *pNext);
using zeCommandListInstantiateGraphExpFP = ze_result_t(ZE_APICALL *)(ze_graph_handle_t hGraph, ze_executable_graph_handle_t *phGraph, void *pNext);
using zeCommandListAppendGraphExpFP = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList, ze_executable_graph_handle_t hGraph, void *pNext,
                                                                ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
using zeGraphDestroyExpFP = ze_result_t(ZE_APICALL *)(ze_graph_handle_t hGraph);
using zeExecutableGraphDestroyExpFP = ze_result_t(ZE_APICALL *)(ze_executable_graph_handle_t hGraph);

using zeCommandListIsGraphCaptureEnabledExpFP = ze_result_t(ZE_APICALL *)(ze_command_list_handle_t hCommandList);
using zeGraphIsEmptyExpFP = ze_result_t(ZE_APICALL *)(ze_graph_handle_t hGraph);
using zeGraphDumpContentsExpFP = ze_result_t(ZE_APICALL *)(ze_graph_handle_t hGraph, const char *filePath, void *pNext);

struct GraphApi {
    zeGraphCreateExpFP graphCreate = nullptr;
    zeCommandListBeginGraphCaptureExpFP commandListBeginGraphCapture = nullptr;
    zeCommandListBeginCaptureIntoGraphExpFP commandListBeginCaptureIntoGraph = nullptr;
    zeCommandListEndGraphCaptureExpFP commandListEndGraphCapture = nullptr;
    zeCommandListInstantiateGraphExpFP commandListInstantiateGraph = nullptr;
    zeCommandListAppendGraphExpFP commandListAppendGraph = nullptr;
    zeGraphDestroyExpFP graphDestroy = nullptr;
    zeExecutableGraphDestroyExpFP executableGraphDestroy = nullptr;

    zeCommandListIsGraphCaptureEnabledExpFP commandListIsGraphCaptureEnabled = nullptr;
    zeGraphIsEmptyExpFP graphIsEmpty = nullptr;
    zeGraphDumpContentsExpFP graphDumpContents = nullptr;

    bool valid() const {
        return graphCreate && commandListBeginGraphCapture && commandListBeginCaptureIntoGraph && commandListEndGraphCapture && commandListInstantiateGraph && commandListAppendGraph && graphDestroy && executableGraphDestroy && commandListIsGraphCaptureEnabled && graphIsEmpty && graphDumpContents;
    }
};

GraphApi loadGraphApi(ze_driver_handle_t driver) {
    GraphApi ret;
    zeDriverGetExtensionFunctionAddress(driver, "zeGraphCreateExp", reinterpret_cast<void **>(&ret.graphCreate));
    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListBeginGraphCaptureExp", reinterpret_cast<void **>(&ret.commandListBeginGraphCapture));
    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListBeginCaptureIntoGraphExp", reinterpret_cast<void **>(&ret.commandListBeginCaptureIntoGraph));
    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListEndGraphCaptureExp", reinterpret_cast<void **>(&ret.commandListEndGraphCapture));
    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListInstantiateGraphExp", reinterpret_cast<void **>(&ret.commandListInstantiateGraph));
    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListAppendGraphExp", reinterpret_cast<void **>(&ret.commandListAppendGraph));
    zeDriverGetExtensionFunctionAddress(driver, "zeGraphDestroyExp", reinterpret_cast<void **>(&ret.graphDestroy));
    zeDriverGetExtensionFunctionAddress(driver, "zeExecutableGraphDestroyExp", reinterpret_cast<void **>(&ret.executableGraphDestroy));

    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListIsGraphCaptureEnabledExp", reinterpret_cast<void **>(&ret.commandListIsGraphCaptureEnabled));
    zeDriverGetExtensionFunctionAddress(driver, "zeGraphIsEmptyExp", reinterpret_cast<void **>(&ret.graphIsEmpty));
    zeDriverGetExtensionFunctionAddress(driver, "zeGraphDumpContentsExp", reinterpret_cast<void **>(&ret.graphDumpContents));

    return ret;
}

void testAppendMemoryCopy(ze_driver_handle_t driver, ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
    auto graphApi = loadGraphApi(driver);
    if (false == graphApi.valid()) {
        std::cerr << "Graph API not available" << std::endl;
        validRet = false;
        return;
    }

    ze_graph_handle_t virtualGraph = nullptr;
    ze_executable_graph_handle_t physicalGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.graphCreate(context, &virtualGraph, nullptr));

    const size_t allocSize = 4096;
    char *heapBuffer = new char[allocSize];
    void *zeBuffer = nullptr;
    char stackBuffer[allocSize];
    ze_command_list_handle_t cmdList;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer, 0, allocSize);

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));

    SUCCESS_OR_TERMINATE(graphApi.commandListBeginCaptureIntoGraph(cmdList, virtualGraph, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeBuffer, heapBuffer, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer, zeBuffer, allocSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListEndGraphCapture(cmdList, nullptr, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListInstantiateGraph(virtualGraph, &physicalGraph, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListAppendGraph(cmdList, physicalGraph, nullptr, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, -1));

    // Validate stack and ze buffers have the original data from heapBuffer
    validRet = LevelZeroBlackBoxTests::validate(heapBuffer, stackBuffer, allocSize);

    if (!validRet) {
        std::cerr << "Data mismatches found!\n";
        std::cerr << "heapBuffer == " << static_cast<void *>(heapBuffer) << "\n";
        std::cerr << "stackBuffer == " << static_cast<void *>(stackBuffer) << std::endl;
    }

    delete[] heapBuffer;
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));

    SUCCESS_OR_TERMINATE(graphApi.graphDestroy(virtualGraph));
    SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph));
}

void testMultiGraph(ze_driver_handle_t driver, ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
    auto graphApi = loadGraphApi(driver);
    if (false == graphApi.valid()) {
        std::cerr << "Graph API not available" << std::endl;
        validRet = false;
        return;
    }

    ze_graph_handle_t virtualGraph = nullptr;
    ze_executable_graph_handle_t physicalGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.graphCreate(context, &virtualGraph, nullptr));

    const size_t allocSize = 4096;
    char *heapBuffer = new char[allocSize];
    void *zeBuffer = nullptr;
    char stackBuffer[allocSize];
    ze_command_list_handle_t cmdListMain;
    ze_command_list_handle_t cmdListSub;

    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 2;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    SUCCESS_OR_TERMINATE(zeEventPoolCreate(context, &eventPoolDesc, 1, &device, &eventPool));

    ze_event_handle_t eventFork = nullptr;
    ze_event_handle_t eventJoin = nullptr;
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    SUCCESS_OR_TERMINATE(zeEventCreate(eventPool, &eventDesc, &eventFork));

    eventDesc.index = 1;
    SUCCESS_OR_TERMINATE(zeEventCreate(eventPool, &eventDesc, &eventJoin));

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer, 0, allocSize);

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListMain));

    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdListSub));

    SUCCESS_OR_TERMINATE(graphApi.commandListBeginCaptureIntoGraph(cmdListMain, virtualGraph, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListMain, zeBuffer, heapBuffer, allocSize, eventFork, 0, nullptr));
    // fork :
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListSub, stackBuffer, zeBuffer, allocSize, eventJoin, 1, &eventFork));
    // join :
    SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdListMain, 1, &eventJoin));

    SUCCESS_OR_TERMINATE(graphApi.commandListEndGraphCapture(cmdListMain, nullptr, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListInstantiateGraph(virtualGraph, &physicalGraph, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListAppendGraph(cmdListMain, physicalGraph, nullptr, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdListMain, -1));

    // Validate stack and ze buffers have the original data from heapBuffer
    validRet = LevelZeroBlackBoxTests::validate(heapBuffer, stackBuffer, allocSize);

    if (!validRet) {
        std::cerr << "Data mismatches found!\n";
        std::cerr << "heapBuffer == " << static_cast<void *>(heapBuffer) << "\n";
        std::cerr << "stackBuffer == " << static_cast<void *>(stackBuffer) << std::endl;
    }

    delete[] heapBuffer;
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));

    SUCCESS_OR_TERMINATE(graphApi.graphDestroy(virtualGraph));
    SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListSub));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListMain));
    SUCCESS_OR_TERMINATE(zeEventDestroy(eventJoin));
    SUCCESS_OR_TERMINATE(zeEventDestroy(eventFork));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName("Zello Graph");
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);

    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device0 = devices[0];

    ze_device_properties_t device0Properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device0, &device0Properties));
    LevelZeroBlackBoxTests::printDeviceProperties(device0Properties);

    bool outputValidationSuccessful = false;

    std::string currentTest;
    currentTest = "Standard Memory Copy";
    testAppendMemoryCopy(driverHandle, context, device0, outputValidationSuccessful);
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

    currentTest = "Standard Memory Copy - multigraph";
    testMultiGraph(driverHandle, context, device0, outputValidationSuccessful);
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

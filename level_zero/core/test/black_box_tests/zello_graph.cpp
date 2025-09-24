/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_graph.h"

#include "zello_common.h"
#include "zello_compile.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>

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

using TestKernelsContainer = std::map<std::string, ze_kernel_handle_t>;

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
    bool loaded = false;
};

void dumpGraphToDotIfEnabled(const GraphApi &graphApi, ze_graph_handle_t virtualGraph, const std::string &testName, bool dumpGraph) {
    if (!dumpGraph) {
        return;
    }

    std::string filename = testName + "_graph.gv";
    ze_result_t dumpResult = graphApi.graphDumpContents(virtualGraph, filename.c_str(), nullptr);

    if (dumpResult == ZE_RESULT_SUCCESS) {
        std::cout << "Graph dumped to " << filename << std::endl;
    } else {
        std::cerr << "Failed to dump graph for test " << testName << " (result: " << std::hex << dumpResult << ")" << std::endl;
    }
}

inline void createImmediateCmdlistWithMode(ze_context_handle_t context,
                                           ze_device_handle_t device,
                                           ze_command_queue_mode_t mode,
                                           ze_command_queue_flags_t flags,
                                           ze_command_list_handle_t &cmdList) {
    ze_command_queue_desc_t cmdQueueDesc{
        .stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .pNext = nullptr,
        .ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false),
        .index = 0,
        .flags = flags,
        .mode = mode,
        .priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
    };
    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
}

GraphApi loadGraphApi(ze_driver_handle_t driver) {
    static GraphApi testGraphFunctions;

    if (testGraphFunctions.loaded) {
        return testGraphFunctions;
    }
    zeDriverGetExtensionFunctionAddress(driver, "zeGraphCreateExp", reinterpret_cast<void **>(&testGraphFunctions.graphCreate));
    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListBeginGraphCaptureExp", reinterpret_cast<void **>(&testGraphFunctions.commandListBeginGraphCapture));
    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListBeginCaptureIntoGraphExp", reinterpret_cast<void **>(&testGraphFunctions.commandListBeginCaptureIntoGraph));
    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListEndGraphCaptureExp", reinterpret_cast<void **>(&testGraphFunctions.commandListEndGraphCapture));
    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListInstantiateGraphExp", reinterpret_cast<void **>(&testGraphFunctions.commandListInstantiateGraph));
    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListAppendGraphExp", reinterpret_cast<void **>(&testGraphFunctions.commandListAppendGraph));
    zeDriverGetExtensionFunctionAddress(driver, "zeGraphDestroyExp", reinterpret_cast<void **>(&testGraphFunctions.graphDestroy));
    zeDriverGetExtensionFunctionAddress(driver, "zeExecutableGraphDestroyExp", reinterpret_cast<void **>(&testGraphFunctions.executableGraphDestroy));

    zeDriverGetExtensionFunctionAddress(driver, "zeCommandListIsGraphCaptureEnabledExp", reinterpret_cast<void **>(&testGraphFunctions.commandListIsGraphCaptureEnabled));
    zeDriverGetExtensionFunctionAddress(driver, "zeGraphIsEmptyExp", reinterpret_cast<void **>(&testGraphFunctions.graphIsEmpty));
    zeDriverGetExtensionFunctionAddress(driver, "zeGraphDumpContentsExp", reinterpret_cast<void **>(&testGraphFunctions.graphDumpContents));

    testGraphFunctions.loaded = true;
    return testGraphFunctions;
}

bool testAppendMemoryCopy(GraphApi &graphApi, ze_context_handle_t &context, ze_device_handle_t &device, bool aubMode, bool dumpGraph) {
    bool validRet = true;
    ze_graph_handle_t virtualGraph = nullptr;
    ze_executable_graph_handle_t physicalGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.graphCreate(context, &virtualGraph, nullptr));

    const size_t allocSize = 4096;

    void *heapBuffer = nullptr;
    void *stackBuffer = nullptr;
    void *zeBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &heapBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &stackBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        static_cast<char *>(heapBuffer)[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer, 0, allocSize);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    ze_command_list_handle_t cmdList;
    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, 0, cmdList);

    SUCCESS_OR_TERMINATE(graphApi.commandListBeginCaptureIntoGraph(cmdList, virtualGraph, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeBuffer, heapBuffer, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer, zeBuffer, allocSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListEndGraphCapture(cmdList, nullptr, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListInstantiateGraph(virtualGraph, &physicalGraph, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListAppendGraph(cmdList, physicalGraph, nullptr, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));

    if (aubMode == false) {
        // Validate stack and ze buffers have the original data from heapBuffer
        validRet = LevelZeroBlackBoxTests::validate(heapBuffer, stackBuffer, allocSize);

        if (!validRet) {
            std::cerr << "Data mismatches found!\n";
            std::cerr << "heapBuffer == " << heapBuffer << "\n";
            std::cerr << "stackBuffer == " << stackBuffer << std::endl;
        }
    }

    dumpGraphToDotIfEnabled(graphApi, virtualGraph, __func__, dumpGraph);

    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, heapBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, stackBuffer));

    SUCCESS_OR_TERMINATE(graphApi.graphDestroy(virtualGraph));
    SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph));

    return validRet;
}

bool testMultiGraph(GraphApi &graphApi, ze_context_handle_t &context, ze_device_handle_t &device, bool aubMode, bool dumpGraph) {
    bool validRet = true;
    ze_graph_handle_t virtualGraph = nullptr;
    ze_executable_graph_handle_t physicalGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.graphCreate(context, &virtualGraph, nullptr));

    const size_t allocSize = 4096;
    void *heapBuffer = nullptr;
    void *stackBuffer = nullptr;
    void *zeBuffer = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &heapBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &stackBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        static_cast<char *>(heapBuffer)[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer, 0, allocSize);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

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

    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, 0, cmdListMain);
    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, 0, cmdListSub);

    SUCCESS_OR_TERMINATE(graphApi.commandListBeginCaptureIntoGraph(cmdListMain, virtualGraph, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListMain, zeBuffer, heapBuffer, allocSize, eventFork, 0, nullptr));
    // fork :
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListSub, stackBuffer, zeBuffer, allocSize, eventJoin, 1, &eventFork));
    // join :
    SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdListMain, 1, &eventJoin));

    SUCCESS_OR_TERMINATE(graphApi.commandListEndGraphCapture(cmdListMain, nullptr, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListInstantiateGraph(virtualGraph, &physicalGraph, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListAppendGraph(cmdListMain, physicalGraph, nullptr, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdListMain, std::numeric_limits<uint64_t>::max()));

    if (aubMode == false) {
        // Validate stack and ze buffers have the original data from heapBuffer
        validRet = LevelZeroBlackBoxTests::validate(heapBuffer, stackBuffer, allocSize);

        if (!validRet) {
            std::cerr << "Data mismatches found!\n";
            std::cerr << "heapBuffer == " << heapBuffer << "\n";
            std::cerr << "stackBuffer == " << stackBuffer << std::endl;
        }
    }

    dumpGraphToDotIfEnabled(graphApi, virtualGraph, __func__, dumpGraph);

    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, heapBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, stackBuffer));

    SUCCESS_OR_TERMINATE(graphApi.graphDestroy(virtualGraph));
    SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListSub));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListMain));
    SUCCESS_OR_TERMINATE(zeEventDestroy(eventJoin));
    SUCCESS_OR_TERMINATE(zeEventDestroy(eventFork));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    return validRet;
}

inline auto allocateDispatchTraits(ze_context_handle_t context, bool indirect) {

    auto dispatchTraitsDeleter = [context, indirect](ze_group_count_t *ptr) noexcept {
        if (indirect) {
            zeMemFree(context, ptr);
        } else {
            delete ptr;
        }
    };
    using RetUniquePtr = std::unique_ptr<ze_group_count_t, decltype(dispatchTraitsDeleter)>;

    ze_group_count_t *rawPtr = nullptr;
    if (indirect) {
        ze_host_mem_alloc_desc_t hostAllocDesc{
            .stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC,
            .pNext = nullptr,
            .flags = 0U,
        };
        SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostAllocDesc, sizeof(ze_group_count_t), 4096, reinterpret_cast<void **>(&rawPtr)));
    } else {
        rawPtr = new ze_group_count_t;
    }
    return RetUniquePtr(rawPtr, dispatchTraitsDeleter);
}

bool testAppendLaunchKernel(GraphApi &graphApi,
                            ze_context_handle_t &context,
                            ze_device_handle_t &device,
                            TestKernelsContainer &testKernels,
                            bool areDispatchTraitsIndirect,
                            bool aubMode,
                            bool dumpGraph) {
    bool validRet = true;

    auto kernel = testKernels["memcpy_bytes"];

    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_handle_t eventCopied = nullptr;

    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device,
                                                     eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                                                     false, nullptr, nullptr,
                                                     1, &eventCopied, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);

    ze_command_list_handle_t cmdList;
    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, 0, cmdList);

    // Buffers
    constexpr size_t allocSize = 4096;
    void *srcBuffer = nullptr;
    void *interimBuffer = nullptr;
    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t devAllocDesc = {
        .stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC,
        .pNext = nullptr,
        .flags = 0,
        .ordinal = 0,
    };
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &devAllocDesc, allocSize, allocSize, device, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &devAllocDesc, allocSize, allocSize, device, &interimBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &devAllocDesc, allocSize, allocSize, device, &dstBuffer));

    // Kernel groups size
    constexpr size_t bytesPerThread = sizeof(std::byte);
    constexpr size_t numThreads = allocSize / bytesPerThread;
    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, static_cast<uint32_t>(numThreads), 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE_BOOL(numThreads % groupSizeX == 0);
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Group size : (" << groupSizeX << ", " << groupSizeY << ", " << groupSizeZ << ")" << std::endl;
    }
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    // Start capturing commands
    ze_graph_handle_t virtualGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.graphCreate(context, &virtualGraph, nullptr));
    SUCCESS_OR_TERMINATE(graphApi.commandListBeginCaptureIntoGraph(cmdList, virtualGraph, nullptr));

    // Encode buffers initialization
    uint8_t srcInitValue = 0xa;
    uint8_t dstInitValue = 0x5;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, srcBuffer, &srcInitValue, sizeof(srcInitValue), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, dstBuffer, &dstInitValue, sizeof(dstInitValue), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    auto dispatchTraits = allocateDispatchTraits(context, areDispatchTraitsIndirect);
    dispatchTraits->groupCountX = static_cast<uint32_t>(numThreads) / groupSizeX,
    dispatchTraits->groupCountY = 1u,
    dispatchTraits->groupCountZ = 1u,
    LevelZeroBlackBoxTests::printGroupCount(*dispatchTraits);
    SUCCESS_OR_TERMINATE_BOOL(dispatchTraits->groupCountX * groupSizeX == allocSize);

    // Launch first copy
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(interimBuffer), &interimBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcBuffer), &srcBuffer));
    if (areDispatchTraitsIndirect) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelIndirect(cmdList, kernel, dispatchTraits.get(), eventCopied, 0, nullptr));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, dispatchTraits.get(), eventCopied, 0, nullptr));
    }

    // Launch second copy
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(interimBuffer), &interimBuffer));
    if (areDispatchTraitsIndirect) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernelIndirect(cmdList, kernel, dispatchTraits.get(), nullptr, 1, &eventCopied));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, dispatchTraits.get(), nullptr, 1, &eventCopied));
    }
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Encode reading data back
    auto outputData = std::make_unique<std::byte[]>(allocSize);
    std::memset(outputData.get(), 0x9, allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, outputData.get(), dstBuffer, allocSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListEndGraphCapture(cmdList, nullptr, nullptr));
    ze_executable_graph_handle_t physicalGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.commandListInstantiateGraph(virtualGraph, &physicalGraph, nullptr));

    // Dispatch and wait
    SUCCESS_OR_TERMINATE(graphApi.commandListAppendGraph(cmdList, physicalGraph, nullptr, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));

    // Validate
    if (aubMode == false) {
        validRet = LevelZeroBlackBoxTests::validateToValue(srcInitValue, outputData.get(), numThreads);
        if (!validRet) {
            std::cerr << "Data mismatches found!\n";
            std::cerr << "srcInitValue == " << std::dec << srcInitValue << "\n";
            std::cerr << "outputData == " << static_cast<void *>(outputData.get()) << std::endl;
        }
    }

    dumpGraphToDotIfEnabled(graphApi, virtualGraph, __func__, dumpGraph);

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, interimBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));

    SUCCESS_OR_TERMINATE(zeEventDestroy(eventCopied));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));

    SUCCESS_OR_TERMINATE(graphApi.graphDestroy(virtualGraph));
    SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph));
    return validRet;
}

bool testAppendLaunchMultipleKernelsIndirect(GraphApi &graphApi,
                                             ze_context_handle_t &context,
                                             ze_device_handle_t &device,
                                             TestKernelsContainer &testKernels,
                                             bool aubMode,
                                             bool dumpGraph) {
    bool validRet = true;

    auto kernelMemcpySrcToDst = testKernels["memcpy_bytes"];
    auto kernelAddConstant = testKernels["add_constant"];

    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_handle_t eventCopied = nullptr;

    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device,
                                                     eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                                                     false, nullptr, nullptr,
                                                     1, &eventCopied, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);

    ze_command_list_handle_t cmdList;
    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, 0, cmdList);

    // Buffers
    constexpr size_t allocSize = 4096;
    void *srcBuffer = nullptr;
    void *incrementedBuffer = nullptr;
    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t devAllocDesc = {
        .stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC,
        .pNext = nullptr,
        .flags = 0,
        .ordinal = 0,
    };
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &devAllocDesc, allocSize, allocSize, device, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &devAllocDesc, allocSize, allocSize, device, &dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &devAllocDesc, allocSize, allocSize, device, &incrementedBuffer));

    constexpr uint32_t kernelsNum = 2U;
    uint32_t *kernelsNumBuff = nullptr;
    ze_group_count_t *dispatchTraits = nullptr;
    ze_host_mem_alloc_desc_t hostAllocDesc{
        .stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC,
        .pNext = nullptr,
        .flags = 0U,
    };
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostAllocDesc, sizeof(uint32_t), 4096, reinterpret_cast<void **>(&kernelsNumBuff)));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostAllocDesc, sizeof(ze_group_count_t) * kernelsNum, 4096, reinterpret_cast<void **>(&dispatchTraits)));

    // Kernel groups size
    constexpr size_t bytesPerThread = sizeof(std::byte);
    constexpr size_t numThreads = allocSize / bytesPerThread;
    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE_BOOL(numThreads % groupSizeX == 0);
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernelAddConstant, static_cast<uint32_t>(numThreads), 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Group size : (" << groupSizeX << ", " << groupSizeY << ", " << groupSizeZ << ")" << std::endl;
    }
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernelMemcpySrcToDst, groupSizeX, groupSizeY, groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernelAddConstant, groupSizeX, groupSizeY, groupSizeZ));

    // Start capturing commands
    ze_graph_handle_t virtualGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.graphCreate(context, &virtualGraph, nullptr));
    SUCCESS_OR_TERMINATE(graphApi.commandListBeginCaptureIntoGraph(cmdList, virtualGraph, nullptr));

    // Encode buffers initialization
    const std::byte srcInitialValue{0xA};
    const std::byte dstInitialValue{0x5};
    const int valueToIncrement{-3};
    constexpr int deltaValue = 2;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, srcBuffer, &srcInitialValue, sizeof(srcInitialValue), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, dstBuffer, &dstInitialValue, sizeof(dstInitialValue), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, incrementedBuffer, &valueToIncrement, sizeof(valueToIncrement), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Prepare contiguous dispatch traits
    for (uint32_t i{0U}; i < kernelsNum; ++i) {
        dispatchTraits[i] = {
            .groupCountX = static_cast<uint32_t>(numThreads) / groupSizeX,
            .groupCountY = 1u,
            .groupCountZ = 1u,
        };
    }

    LevelZeroBlackBoxTests::printGroupCount(dispatchTraits[0]);
    SUCCESS_OR_TERMINATE_BOOL(dispatchTraits[0].groupCountX * groupSizeX == allocSize);

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMemcpySrcToDst, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMemcpySrcToDst, 1, sizeof(srcBuffer), &srcBuffer));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddConstant, 0, sizeof(incrementedBuffer), &incrementedBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddConstant, 1, sizeof(int), &deltaValue));

    ze_kernel_handle_t pKernelHandles[] = {kernelMemcpySrcToDst, kernelAddConstant};
    *kernelsNumBuff = kernelsNum;
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchMultipleKernelsIndirect(cmdList,
                                                                          kernelsNum,
                                                                          pKernelHandles,
                                                                          kernelsNumBuff,
                                                                          dispatchTraits,
                                                                          eventCopied,
                                                                          0U,
                                                                          nullptr));

    // Encode reading data back
    auto dstOut = std::vector<std::byte>(allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstOut.data(), dstBuffer, allocSize, nullptr, 1, &eventCopied));

    auto incrementedOut = std::vector<std::byte>(allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, incrementedOut.data(), incrementedBuffer, allocSize, nullptr, 1, &eventCopied));

    SUCCESS_OR_TERMINATE(graphApi.commandListEndGraphCapture(cmdList, nullptr, nullptr));
    ze_executable_graph_handle_t physicalGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.commandListInstantiateGraph(virtualGraph, &physicalGraph, nullptr));

    // Dispatch and wait
    SUCCESS_OR_TERMINATE(graphApi.commandListAppendGraph(cmdList, physicalGraph, nullptr, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));

    // Validate
    if (aubMode == false) {
        auto expectedDst = std::vector<std::byte>(allocSize, srcInitialValue);
        validRet = LevelZeroBlackBoxTests::validate(dstOut.data(), expectedDst.data(), allocSize);
        if (!validRet) {
            std::cerr << "Data mismatches found!\n";
            std::cerr << "copiedOutData == " << static_cast<void *>(dstOut.data()) << "\n";
            std::cerr << "expectedData == " << static_cast<void *>(expectedDst.data()) << std::endl;
        }

        constexpr std::byte incrementedValue{0xFF};
        auto expectedIncremented = std::vector<std::byte>(allocSize, incrementedValue);
        bool validRet2 = LevelZeroBlackBoxTests::validate(incrementedOut.data(), expectedIncremented.data(), allocSize);
        if (!validRet2) {
            std::cerr << "Data mismatches found!\n";
            std::cerr << "incrementedData == " << static_cast<void *>(incrementedOut.data()) << "\n";
            std::cerr << "expectedData == " << static_cast<void *>(expectedIncremented.data()) << std::endl;
        }
        validRet &= validRet2; // Combine results
    }

    dumpGraphToDotIfEnabled(graphApi, virtualGraph, __func__, dumpGraph);

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dispatchTraits));
    SUCCESS_OR_TERMINATE(zeMemFree(context, kernelsNumBuff));

    SUCCESS_OR_TERMINATE(zeMemFree(context, incrementedBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));

    SUCCESS_OR_TERMINATE(zeEventDestroy(eventCopied));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));

    SUCCESS_OR_TERMINATE(graphApi.graphDestroy(virtualGraph));
    SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph));
    return validRet;
}

bool testMultipleGraphExecution(GraphApi &graphApi,
                                ze_context_handle_t &context,
                                ze_device_handle_t &device,
                                TestKernelsContainer &testKernels,
                                bool aubMode,
                                bool dumpGraph) {
    bool validRet = true;

    constexpr size_t allocSize = 4096;
    constexpr size_t elemCount = allocSize / sizeof(uint32_t);

    uint32_t initialValue = 1;
    uint32_t loopCount = 3;
    uint32_t addValue = 5;

    uint32_t expectedValue = initialValue + loopCount * addValue;

    ze_kernel_handle_t kernelAddConstant = testKernels["add_constant"];

    ze_command_list_handle_t cmdList;
    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, 0, cmdList);
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &buffer));
    for (size_t i = 0; i < elemCount; i++) {
        reinterpret_cast<uint32_t *>(buffer)[i] = initialValue;
    }

    ze_graph_handle_t virtualGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.graphCreate(context, &virtualGraph, nullptr));
    SUCCESS_OR_TERMINATE(graphApi.commandListBeginCaptureIntoGraph(cmdList, virtualGraph, nullptr));

    uint32_t groupSizeX = 64;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernelAddConstant, groupSizeX, groupSizeY, groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddConstant, 0, sizeof(buffer), &buffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddConstant, 1, sizeof(addValue), &addValue));
    ze_group_count_t groupCount = {static_cast<uint32_t>(elemCount / groupSizeX), 1, 1};
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernelAddConstant, &groupCount, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListEndGraphCapture(cmdList, nullptr, nullptr));
    ze_executable_graph_handle_t physicalGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.commandListInstantiateGraph(virtualGraph, &physicalGraph, nullptr));

    // Dispatch and wait
    for (uint32_t i = 0; i < loopCount; i++) {
        SUCCESS_OR_TERMINATE(graphApi.commandListAppendGraph(cmdList, physicalGraph, nullptr, nullptr, 0, nullptr));
    }
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));

    if (aubMode == false) {
        validRet = LevelZeroBlackBoxTests::validateToValue(expectedValue, buffer, elemCount);
    }
    dumpGraphToDotIfEnabled(graphApi, virtualGraph, __func__, dumpGraph);

    SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph));
    SUCCESS_OR_TERMINATE(graphApi.graphDestroy(virtualGraph));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    return validRet;
}

bool testExternalGraphCbEvents(GraphApi &graphApi,
                               ze_context_handle_t &context,
                               ze_device_handle_t &device,
                               TestKernelsContainer &testKernels,
                               bool aubMode,
                               bool dumpGraph) {
    bool validRet = true;

    constexpr size_t allocSize = 4096;
    constexpr size_t elemCount = allocSize / sizeof(uint32_t);

    uint32_t initialValue = 1;
    uint32_t addValue = 5;

    uint32_t expectedValueForFirstExecution = initialValue + addValue;
    uint32_t expectedValueForSecondExecution = expectedValueForFirstExecution + addValue;

    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_handle_t eventCb = nullptr;
    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_GRAPH_EXTERNAL_EVENT;
    counterBasedDesc.flags |= ZEX_COUNTER_BASED_EVENT_FLAG_HOST_VISIBLE;
    counterBasedDesc.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device,
                                                     eventPool, 0u,
                                                     true, &counterBasedDesc, LevelZeroBlackBoxTests::zexCounterBasedEventCreate2Func,
                                                     1, &eventCb, ZE_EVENT_SCOPE_FLAG_HOST, 0u);

    ze_kernel_handle_t kernelAddConstant = testKernels["add_constant"];

    ze_command_list_handle_t cmdList;
    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_FLAG_IN_ORDER, cmdList);
    void *buffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &buffer));
    for (size_t i = 0; i < elemCount; i++) {
        reinterpret_cast<uint32_t *>(buffer)[i] = initialValue;
    }

    ze_graph_handle_t virtualGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.graphCreate(context, &virtualGraph, nullptr));
    SUCCESS_OR_TERMINATE(graphApi.commandListBeginCaptureIntoGraph(cmdList, virtualGraph, nullptr));

    uint32_t groupSizeX = 64;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernelAddConstant, groupSizeX, groupSizeY, groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddConstant, 0, sizeof(buffer), &buffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddConstant, 1, sizeof(addValue), &addValue));
    ze_group_count_t groupCount = {static_cast<uint32_t>(elemCount / groupSizeX), 1, 1};
    // attach external event to append operation
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernelAddConstant, &groupCount, eventCb, 0, nullptr));

    SUCCESS_OR_TERMINATE(graphApi.commandListEndGraphCapture(cmdList, nullptr, nullptr));

    // create two physical graphs from the same virtual graph
    ze_executable_graph_handle_t physicalGraph = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.commandListInstantiateGraph(virtualGraph, &physicalGraph, nullptr));
    ze_executable_graph_handle_t physicalGraph2 = nullptr;
    SUCCESS_OR_TERMINATE(graphApi.commandListInstantiateGraph(virtualGraph, &physicalGraph2, nullptr));

    // Dispatch and wait physicalGraph 1
    SUCCESS_OR_TERMINATE(graphApi.commandListAppendGraph(cmdList, physicalGraph, nullptr, nullptr, 0, nullptr));
    // synchronize using event
    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(eventCb, std::numeric_limits<uint64_t>::max()));

    // verify data
    if (aubMode == false) {
        validRet = LevelZeroBlackBoxTests::validateToValue(expectedValueForFirstExecution, buffer, elemCount);
    }

    // Dispatch and wait physicalGraph 2
    SUCCESS_OR_TERMINATE(graphApi.commandListAppendGraph(cmdList, physicalGraph2, nullptr, nullptr, 0, nullptr));
    // synchronize using the same event, but different executable graph
    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(eventCb, std::numeric_limits<uint64_t>::max()));

    // verify data
    if (aubMode == false) {
        validRet = LevelZeroBlackBoxTests::validateToValue(expectedValueForSecondExecution, buffer, elemCount);
    }

    // Final sync to ensure all operations are done
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));

    dumpGraphToDotIfEnabled(graphApi, virtualGraph, __func__, dumpGraph);

    SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph));
    SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph2));
    SUCCESS_OR_TERMINATE(graphApi.graphDestroy(virtualGraph));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeEventDestroy(eventCb));
    return validRet;
}

bool testMultipleLevelGraph(GraphApi &graphApi,
                            ze_context_handle_t &context,
                            ze_device_handle_t &device,
                            TestKernelsContainer &testKernels,
                            bool aubMode,
                            bool dumpGraph,
                            bool immediate) {
    bool validRet = true;

    constexpr size_t allocSize = 512;
    constexpr size_t elemCount = allocSize / sizeof(uint32_t);

    uint32_t initialValue = 1;
    uint32_t addValue1 = 5;
    uint32_t mulValue1 = 2;
    uint32_t mulValue2 = 1;
    uint32_t addValue2 = 3;
    uint32_t mulValue3 = 4;

    uint32_t expectedValue = (((((initialValue + addValue1) * mulValue1) * mulValue2) + addValue2) * mulValue3);

    // order of kernels: add (init + add1) => mul (result * mul1) => mul (result * mul2) => add (result + add2) => mul(result * mul3)
    // graph sequence root(add1) => fork1(mul1) => fork2(mul2) => return fork1 => return root => root(add2) => fork1(mul3) => return root

    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_handle_t eventCb = nullptr;
    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE;
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device,
                                                     eventPool, 0u,
                                                     true, &counterBasedDesc, LevelZeroBlackBoxTests::zexCounterBasedEventCreate2Func,
                                                     1, &eventCb, 0u, 0u);

    ze_kernel_handle_t kernelAddDst = testKernels["add_constant_output"];
    ze_kernel_handle_t kernelMulDst = testKernels["mul_constant_output"];

    ze_command_list_handle_t cmdListRoot, cmdListFork1, cmdListFork2;
    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_FLAG_IN_ORDER, cmdListRoot);
    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_FLAG_IN_ORDER, cmdListFork1);
    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_FLAG_IN_ORDER, cmdListFork2);

    void *srcBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &srcBuffer));
    for (size_t i = 0; i < elemCount; i++) {
        reinterpret_cast<uint32_t *>(srcBuffer)[i] = initialValue;
    }
    void *stage1Buffer = nullptr; // results for add1
    void *stage2Buffer = nullptr; // results for mul1
    void *stage3Buffer = nullptr; // results for mul2
    void *stage4Buffer = nullptr; // results for add2
    void *finalBuffer = nullptr;  // results for mul3

    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &stage1Buffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &stage2Buffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &stage3Buffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &stage4Buffer));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, allocSize, &finalBuffer));

    ze_graph_handle_t virtualGraph = nullptr;
    if (immediate == false) {
        SUCCESS_OR_TERMINATE(graphApi.graphCreate(context, &virtualGraph, nullptr));
        SUCCESS_OR_TERMINATE(graphApi.commandListBeginCaptureIntoGraph(cmdListRoot, virtualGraph, nullptr));
    }

    uint32_t groupSizeX = std::min(64u, static_cast<uint32_t>(elemCount));
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    ze_group_count_t groupCount = {static_cast<uint32_t>(elemCount / groupSizeX), 1, 1};

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernelAddDst, groupSizeX, groupSizeY, groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernelMulDst, groupSizeX, groupSizeY, groupSizeZ));

    // set add kernel for root
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddDst, 0, sizeof(srcBuffer), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddDst, 1, sizeof(stage1Buffer), &stage1Buffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddDst, 2, sizeof(addValue1), &addValue1));

    // attach event to append operation to signal to fork 1
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdListRoot, kernelAddDst, &groupCount, eventCb, 0, nullptr));

    // set mul kernel for fork 1
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMulDst, 0, sizeof(stage1Buffer), &stage1Buffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMulDst, 1, sizeof(stage2Buffer), &stage2Buffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMulDst, 2, sizeof(mulValue1), &mulValue1));

    // wait for signal from root and reuse event to carry signal into fork 2
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdListFork1, kernelMulDst, &groupCount, eventCb, 1, &eventCb));

    // set mul kernel for fork 2
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMulDst, 0, sizeof(stage2Buffer), &stage2Buffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMulDst, 1, sizeof(stage3Buffer), &stage3Buffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMulDst, 2, sizeof(mulValue2), &mulValue2));

    // wait for signal from fork 1 and reuse event to carry signal into fork 2
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdListFork2, kernelMulDst, &groupCount, eventCb, 1, &eventCb));

    // join to fork1
    SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdListFork1, 1, &eventCb));
    SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdListFork1, eventCb));

    // set add kernel for root
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddDst, 0, sizeof(stage3Buffer), &stage3Buffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddDst, 1, sizeof(stage4Buffer), &stage4Buffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelAddDst, 2, sizeof(addValue2), &addValue2));

    // join to root and attach event to append operation to signal to fork 1
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdListRoot, kernelAddDst, &groupCount, eventCb, 1, &eventCb));

    // set mul kernel for fork 1
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMulDst, 0, sizeof(stage4Buffer), &stage4Buffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMulDst, 1, sizeof(finalBuffer), &finalBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernelMulDst, 2, sizeof(mulValue3), &mulValue3));

    // wait for signal from root and reuse event to carry signal into root again
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdListFork1, kernelMulDst, &groupCount, eventCb, 1, &eventCb));

    // join to root
    SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdListRoot, 1, &eventCb));

    ze_executable_graph_handle_t physicalGraph = nullptr;
    if (immediate == false) {
        // create physical graphs from the same virtual graph
        SUCCESS_OR_TERMINATE(graphApi.commandListEndGraphCapture(cmdListRoot, nullptr, nullptr));
        SUCCESS_OR_TERMINATE(graphApi.commandListInstantiateGraph(virtualGraph, &physicalGraph, nullptr));
    }

    if (immediate == false) {
        // Dispatch and wait physicalGraph
        SUCCESS_OR_TERMINATE(graphApi.commandListAppendGraph(cmdListRoot, physicalGraph, nullptr, nullptr, 0, nullptr));
    }
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdListRoot, std::numeric_limits<uint64_t>::max()));

    // verify data
    if (aubMode == false) {
        validRet = LevelZeroBlackBoxTests::validateToValue(expectedValue, finalBuffer, elemCount);
    }

    if (immediate == false) {
        dumpGraphToDotIfEnabled(graphApi, virtualGraph, __func__, dumpGraph);
        SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph));
        SUCCESS_OR_TERMINATE(graphApi.graphDestroy(virtualGraph));
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, stage1Buffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, stage2Buffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, stage3Buffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, stage4Buffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, finalBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListRoot));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListFork1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListFork2));
    SUCCESS_OR_TERMINATE(zeEventDestroy(eventCb));
    return validRet;
}

int main(int argc, char *argv[]) {
    constexpr uint32_t bitNumberTestStandardMemoryCopy = 0u;
    constexpr uint32_t bitNumberTestStandardMemoryCopyMultigraph = 1u;
    constexpr uint32_t bitNumberTestAppendLaunchKernel = 2u;
    constexpr uint32_t bitNumberTestAppendLaunchKernelIndirect = 3u;
    constexpr uint32_t bitNumberTestAppendLaunchMultipleKernelsIndirect = 4u;
    constexpr uint32_t bitNumberTestMultipleExecution = 5u;
    constexpr uint32_t bitNumberTestExternalCbEvents = 6u;
    constexpr uint32_t bitNumberTestMultiLevelGraph = 7u;

    constexpr uint32_t defaultTestMask = std::numeric_limits<uint32_t>::max() & ~(1u << bitNumberTestMultiLevelGraph);
    LevelZeroBlackBoxTests::TestBitMask testMask = LevelZeroBlackBoxTests::getTestMask(argc, argv, defaultTestMask);
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    bool dumpGraph = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-d", "--dump_graph");

    const std::string blackBoxName("Zello Graph");

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);

    auto device0 = devices[0];
    ze_record_replay_graph_exp_properties_t graphProperties = {ZE_STRUCTURE_TYPE_RECORD_REPLAY_GRAPH_EXP_PROPERTIES};
    ze_device_properties_t device0Properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device0Properties.pNext = &graphProperties;
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device0, &device0Properties));
    LevelZeroBlackBoxTests::printDeviceProperties(device0Properties);

    std::vector<ze_driver_extension_properties_t> extensionVector;
    ze_driver_extension_properties_t graphExtension{};
    std::string graphExtensionString = "ZE_experimental_record_replay_graph";
    strncpy(graphExtension.name, graphExtensionString.c_str(), graphExtensionString.size());
    graphExtension.version = ZE_RECORD_REPLAY_GRAPH_EXP_VERSION_1_0;
    extensionVector.push_back(graphExtension);
    bool graphExtensionPresent = LevelZeroBlackBoxTests::checkExtensionIsPresent(driverHandle, extensionVector);
    if (!graphExtensionPresent) {
        std::cerr << "Graph extension not present" << std::endl;
        return 1;
    }
    if (false == !!(graphProperties.graphFlags & ZE_RECORD_REPLAY_GRAPH_EXP_FLAG_IMMUTABLE_GRAPH)) {
        std::cerr << "Device not supporting graph" << std::endl;
        return 1;
    }
    auto graphApi = loadGraphApi(driverHandle);
    if (false == graphApi.valid()) {
        std::cerr << "Graph API not available" << std::endl;
        return 1;
    }

    TestKernelsContainer kernelsMap;

    ze_module_handle_t moduleTestKernels;
    LevelZeroBlackBoxTests::createModuleFromSpirV(context, device0, LevelZeroBlackBoxTests::openCLKernelsSource, moduleTestKernels);
    LevelZeroBlackBoxTests::createKernelWithName(moduleTestKernels, "add_constant", kernelsMap["add_constant"]);
    LevelZeroBlackBoxTests::createKernelWithName(moduleTestKernels, "memcpy_bytes", kernelsMap["memcpy_bytes"]);
    LevelZeroBlackBoxTests::createKernelWithName(moduleTestKernels, "add_constant_output", kernelsMap["add_constant_output"]);
    LevelZeroBlackBoxTests::createKernelWithName(moduleTestKernels, "mul_constant_output", kernelsMap["mul_constant_output"]);

    bool boxPass = true;
    bool casePass = true;
    std::string currentTest;
    if (testMask.test(bitNumberTestStandardMemoryCopy)) {
        currentTest = "Standard Memory Copy";
        casePass = testAppendMemoryCopy(graphApi, context, device0, aubMode, dumpGraph);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    if (testMask.test(bitNumberTestStandardMemoryCopyMultigraph)) {
        currentTest = "Standard Memory Copy - multigraph";
        casePass = testMultiGraph(graphApi, context, device0, aubMode, dumpGraph);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    if (testMask.test(bitNumberTestAppendLaunchKernel)) {
        currentTest = "AppendLaunchKernel";
        casePass = testAppendLaunchKernel(graphApi, context, device0, kernelsMap, false, aubMode, dumpGraph);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    if (testMask.test(bitNumberTestAppendLaunchKernelIndirect)) {
        currentTest = "AppendLaunchKernelIndirect";
        casePass = testAppendLaunchKernel(graphApi, context, device0, kernelsMap, true, aubMode, dumpGraph);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    if (testMask.test(bitNumberTestAppendLaunchMultipleKernelsIndirect)) {
        currentTest = "AppendLaunchMultipleKernelsIndirect";
        casePass = testAppendLaunchMultipleKernelsIndirect(graphApi, context, device0, kernelsMap, aubMode, dumpGraph);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    if (testMask.test(bitNumberTestMultipleExecution)) {
        currentTest = "Multiple Graph Execution";
        casePass = testMultipleGraphExecution(graphApi, context, device0, kernelsMap, aubMode, dumpGraph);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    if (testMask.test(bitNumberTestExternalCbEvents)) {
        LevelZeroBlackBoxTests::loadCounterBasedEventCreateFunction(driverHandle);
        currentTest = "External Graph CB Events";
        casePass = testExternalGraphCbEvents(graphApi, context, device0, kernelsMap, aubMode, dumpGraph);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    if (testMask.test(bitNumberTestMultiLevelGraph)) {
        LevelZeroBlackBoxTests::loadCounterBasedEventCreateFunction(driverHandle);
        auto testTitle = "Multiple Level Graph";
        auto getCaseName = [&testTitle](bool immediate) -> std::string {
            std::ostringstream caseName;
            caseName << testTitle;
            caseName << " immediate execution: " << std::boolalpha << immediate;
            caseName << ".";
            return caseName.str();
        };
        bool immediate = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-i", "--immediate");
        currentTest = getCaseName(immediate);
        casePass = testMultipleLevelGraph(graphApi, context, device0, kernelsMap, aubMode, dumpGraph, immediate);
        LevelZeroBlackBoxTests::printResult(aubMode, casePass, blackBoxName, currentTest);
        boxPass &= casePass;
    }

    for (auto kernel : kernelsMap) {
        SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel.second));
    }
    SUCCESS_OR_TERMINATE(zeModuleDestroy(moduleTestKernels));

    int mainRetCode = aubMode ? 0 : (boxPass ? 0 : 1);
    std::string finalStatus = (mainRetCode != 0) ? " FAILED" : " SUCCESS";
    std::cerr << blackBoxName << finalStatus << std::endl;
    return mainRetCode;
}

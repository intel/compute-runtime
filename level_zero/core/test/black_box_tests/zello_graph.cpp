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

inline void createModuleFromSpirV(ze_context_handle_t context, ze_device_handle_t device, const char *kernelSrc, ze_module_handle_t &module) {
    // SpirV for a kernel
    std::string buildLog;
    auto moduleBinary = LevelZeroBlackBoxTests::compileToSpirV(kernelSrc, "", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == moduleBinary.size()));

    ze_module_desc_t moduleDesc = {
        .stype = ZE_STRUCTURE_TYPE_MODULE_DESC,
        .pNext = nullptr,
        .format = ZE_MODULE_FORMAT_IL_SPIRV,
        .inputSize = moduleBinary.size(),
        .pInputModule = reinterpret_cast<const uint8_t *>(moduleBinary.data()),
    };
    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));
}

inline void createKernelWithName(ze_module_handle_t module, const char *kernelName, ze_kernel_handle_t &kernel) {
    ze_kernel_desc_t kernelDesc = {
        .stype = ZE_STRUCTURE_TYPE_KERNEL_DESC,
        .pNext = nullptr,
        .flags = 0,
        .pKernelName = kernelName,
    };
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
}

inline void createEventPool(ze_context_handle_t context, ze_device_handle_t device, ze_event_pool_handle_t &eventPool) {
    ze_event_pool_desc_t eventPoolDesc{
        .stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        .pNext = nullptr,
        .flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        .count = 1,
    };
    SUCCESS_OR_TERMINATE(zeEventPoolCreate(context, &eventPoolDesc, 1, &device, &eventPool));
}

inline void createEventHostCoherent(ze_event_pool_handle_t eventPool, ze_event_handle_t &newEventHandle) {
    ze_event_desc_t eventDesc{
        .stype = ZE_STRUCTURE_TYPE_EVENT_DESC,
        .pNext = nullptr,
        .index = 0,
        .signal = ZE_EVENT_SCOPE_FLAG_HOST,
        .wait = ZE_EVENT_SCOPE_FLAG_HOST,

    };
    SUCCESS_OR_TERMINATE(zeEventCreate(eventPool, &eventDesc, &newEventHandle));
}

inline void createImmediateCmdlistWithMode(ze_context_handle_t context,
                                           ze_device_handle_t device,
                                           ze_command_queue_mode_t mode,
                                           ze_command_list_handle_t &cmdList) {
    ze_command_queue_desc_t cmdQueueDesc{
        .stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .pNext = nullptr,
        .ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false),
        .index = 0,
        .flags = 0,
        .mode = mode,
        .priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
    };
    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
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

void testAppendLaunchKernel(ze_driver_handle_t driver,
                            ze_context_handle_t &context,
                            ze_device_handle_t &device,
                            bool areDispatchTraitsIndirect,
                            bool &validRet) {
    auto graphApi = loadGraphApi(driver);
    if (false == graphApi.valid()) {
        std::cerr << "Graph API not available" << std::endl;
        validRet = false;
        return;
    }

    ze_module_handle_t module;
    createModuleFromSpirV(context, device, LevelZeroBlackBoxTests::memcpyBytesTestKernelSrc, module);

    ze_kernel_handle_t kernel;
    createKernelWithName(module, "memcpy_bytes", kernel);

    ze_event_pool_handle_t eventPool = nullptr;
    createEventPool(context, device, eventPool);

    ze_event_handle_t eventCopied = nullptr;
    createEventHostCoherent(eventPool, eventCopied);

    ze_command_list_handle_t cmdList;
    createImmediateCmdlistWithMode(context, device, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, cmdList);

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
    auto srcInitData = std::make_unique<std::byte[]>(allocSize);
    std::memset(srcInitData.get(), 0xa, allocSize);
    auto dstInitData = std::make_unique<std::byte[]>(allocSize);
    std::memset(dstInitData.get(), 0x5, allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, srcBuffer, srcInitData.get(), allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, dstInitData.get(), allocSize, nullptr, 0, nullptr));
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
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, -1));

    // Validate
    validRet = LevelZeroBlackBoxTests::validate(outputData.get(), srcInitData.get(), allocSize);
    if (!validRet) {
        std::cerr << "Data mismatches found!\n";
        std::cerr << "srcInitData == " << static_cast<void *>(srcInitData.get()) << "\n";
        std::cerr << "outputData == " << static_cast<void *>(outputData.get()) << std::endl;
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, interimBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    SUCCESS_OR_TERMINATE(zeEventDestroy(eventCopied));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));

    SUCCESS_OR_TERMINATE(graphApi.graphDestroy(virtualGraph));
    SUCCESS_OR_TERMINATE(graphApi.executableGraphDestroy(physicalGraph));
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

    currentTest = "AppendLaunchKernel";
    testAppendLaunchKernel(driverHandle, context, device0, false, outputValidationSuccessful);
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

    currentTest = "AppendLaunchKernelIndirect";
    testAppendLaunchKernel(driverHandle, context, device0, true, outputValidationSuccessful);
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>

void testAppendMemoryCopy(ze_context_handle_t &context, ze_device_handle_t &device, bool &useSyncCmdQ, bool &validRet,
                          ze_command_list_handle_t &sharedCmdList, ze_event_handle_t &sharedEvent, ze_event_handle_t &sharedEvent2) {
    const size_t allocSize = 4096;
    char *heapBuffer = new char[allocSize];
    void *zeBuffer = nullptr;
    char stackBuffer[allocSize];
    ze_command_list_handle_t cmdList;
    ze_event_pool_handle_t eventPool, eventPool2;
    ze_event_handle_t event, event2;
    eventPool = eventPool2 = nullptr;
    event = event2 = nullptr;

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

    if (sharedCmdList == nullptr) {
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
        cmdQueueDesc.index = 0;
        LevelZeroBlackBoxTests::selectQueueMode(cmdQueueDesc, useSyncCmdQ);

        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        cmdList = sharedCmdList;
    }

    if (!useSyncCmdQ) {
        if (sharedEvent == nullptr) {
            LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr, 1, &event, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        } else {
            event = sharedEvent;
        }
        if (sharedEvent2 == nullptr) {
            LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool2, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr, 1, &event2, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        } else {
            event2 = sharedEvent2;
        }
    }
    // Copy from heap to device-allocated memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeBuffer, heapBuffer, allocSize,
                                                       useSyncCmdQ ? nullptr : event, 0, nullptr));
    // Copy from device-allocated memory to stack
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer, zeBuffer, allocSize,
                                                       useSyncCmdQ ? nullptr : event2,
                                                       useSyncCmdQ ? 0 : 1,
                                                       useSyncCmdQ ? nullptr : &event));

    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event2, std::numeric_limits<uint64_t>::max()));
    }
    // Validate stack and ze buffers have the original data from heapBuffer
    validRet = LevelZeroBlackBoxTests::validate(heapBuffer, stackBuffer, allocSize);

    if (!validRet) {
        std::cerr << "Data mismatches found!\n";
        std::cerr << "heapBuffer == " << static_cast<void *>(heapBuffer) << "\n";
        std::cerr << "stackBuffer == " << static_cast<void *>(stackBuffer) << std::endl;
    }

    delete[] heapBuffer;
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));
    if (sharedCmdList == nullptr) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }
    if (sharedEvent == nullptr) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    }
    if (sharedEvent2 == nullptr) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event2));
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool2));
    }
}

void testAppendMemoryCopyRegion(ze_context_handle_t &context, ze_device_handle_t &device, bool useSyncCmdQ, bool &validRet,
                                ze_command_list_handle_t &sharedCmdList, ze_event_handle_t &sharedEvent, ze_event_handle_t &sharedEvent2) {
    validRet = true;
    ze_command_list_handle_t cmdList;
    ze_event_pool_handle_t eventPool, eventPool2;
    ze_event_handle_t event, event2;
    eventPool = eventPool2 = nullptr;
    event = event2 = nullptr;

    if (sharedCmdList == nullptr) {
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
        cmdQueueDesc.index = 0;
        LevelZeroBlackBoxTests::selectQueueMode(cmdQueueDesc, useSyncCmdQ);

        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        cmdList = sharedCmdList;
    }

    void *dstBuffer = nullptr;
    uint32_t dstWidth = LevelZeroBlackBoxTests::verbose ? 16 : 1024; // width of the dst 2D buffer in bytes
    uint32_t dstHeight = LevelZeroBlackBoxTests::verbose ? 32 : 512; // height of the dst 2D buffer in bytes
    uint32_t dstOriginX = LevelZeroBlackBoxTests::verbose ? 8 : 128; // Offset in bytes
    uint32_t dstOriginY = LevelZeroBlackBoxTests::verbose ? 8 : 144; // Offset in rows
    uint32_t dstSize = dstHeight * dstWidth;                         // Size of the dst buffer

    void *srcBuffer = nullptr;
    uint32_t srcWidth = LevelZeroBlackBoxTests::verbose ? 24 : 256;  // width of the src 2D buffer in bytes
    uint32_t srcHeight = LevelZeroBlackBoxTests::verbose ? 16 : 384; // height of the src 2D buffer in bytes
    uint32_t srcOriginX = LevelZeroBlackBoxTests::verbose ? 4 : 64;  // Offset in bytes
    uint32_t srcOriginY = LevelZeroBlackBoxTests::verbose ? 4 : 128; // Offset in rows
    uint32_t srcSize = srcHeight * srcWidth;                         // Size of the src buffer

    uint32_t width = LevelZeroBlackBoxTests::verbose ? 8 : 144;  // width of the region to copy
    uint32_t height = LevelZeroBlackBoxTests::verbose ? 12 : 96; // height of the region to copy
    const ze_copy_region_t dstRegion = {dstOriginX, dstOriginY, 0, width, height, 0};
    const ze_copy_region_t srcRegion = {srcOriginX, srcOriginY, 0, width, height, 0};

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;

    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc,
                                          srcSize, 1, device, &srcBuffer));

    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc,
                                          dstSize, 1, device, &dstBuffer));

    // Initialize buffers
    uint8_t *stackBuffer = new uint8_t[srcSize];
    for (uint32_t i = 0; i < srcHeight; i++) {
        for (uint32_t j = 0; j < srcWidth; j++) {
            stackBuffer[i * srcWidth + j] = static_cast<uint8_t>(i * srcWidth + j);
        }
    }

    if (!useSyncCmdQ) {
        // Create Event Pool and kernel launch event
        if (sharedEvent == nullptr) {
            LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr, 1, &event, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        } else {
            event = sharedEvent;
        }
        if (sharedEvent2 == nullptr) {
            LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool2, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr, 1, &event2, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        } else {
            event2 = sharedEvent2;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, srcBuffer, stackBuffer,
                                                       srcSize,
                                                       useSyncCmdQ ? nullptr : event,
                                                       0, nullptr));

    int value = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, dstBuffer, reinterpret_cast<void *>(&value),
                                                       sizeof(value), dstSize, useSyncCmdQ ? nullptr : event2,
                                                       0, nullptr));

    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event2, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostReset(event));
        SUCCESS_OR_TERMINATE(zeEventHostReset(event2));
    }

    // Perform the copy
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopyRegion(cmdList, dstBuffer, &dstRegion, dstWidth, 0,
                                                             srcBuffer, &srcRegion, srcWidth, 0,
                                                             useSyncCmdQ ? nullptr : event,
                                                             0, nullptr));

    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
    }

    uint8_t *dstBufferChar = reinterpret_cast<uint8_t *>(dstBuffer);
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "stackBuffer\n";
        for (uint32_t i = 0; i < srcHeight; i++) {
            for (uint32_t j = 0; j < srcWidth; j++) {
                std::cout << std::setw(3) << std::dec
                          << static_cast<unsigned int>(stackBuffer[i * srcWidth + j]) << " ";
            }
            std::cout << "\n";
        }

        std::cout << "dstBuffer\n";
        for (uint32_t i = 0; i < dstHeight; i++) {
            for (uint32_t j = 0; j < dstWidth; j++) {
                std::cout << std::setw(3) << std::dec
                          << static_cast<unsigned int>(dstBufferChar[i * dstWidth + j]) << " ";
            }
            std::cout << "\n";
        }
    }

    uint32_t dstOffset = dstOriginX + dstOriginY * dstWidth;
    uint32_t srcOffset = srcOriginX + srcOriginY * srcWidth;
    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {
            uint8_t dstVal = dstBufferChar[dstOffset + (i * dstWidth) + j];
            uint8_t srcVal = stackBuffer[srcOffset + (i * srcWidth) + j];
            if (dstVal != srcVal) {
                validRet = false;
            }
        }
    }

    delete[] stackBuffer;
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    if (sharedCmdList == nullptr) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }
    if (sharedEvent == nullptr) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    }
    if (sharedEvent2 == nullptr) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event2));
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool2));
    }
}

void testAppendGpuKernel(ze_context_handle_t &context, ze_device_handle_t &device, bool useSyncCmdQ, bool &validRet,
                         ze_command_list_handle_t &sharedCmdList, ze_event_handle_t &sharedEvent, ze_event_handle_t &sharedEvent2) {
    constexpr size_t allocSize = 4096;
    constexpr size_t bytesPerThread = sizeof(char);
    constexpr size_t numThreads = allocSize / bytesPerThread;
    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    ze_command_list_handle_t cmdList;
    ze_event_pool_handle_t eventPool, eventPool2;
    ze_event_handle_t event, event2;
    eventPool = eventPool2 = nullptr;
    event = event2 = nullptr;
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    std::string buildLog;
    auto moduleBinary = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::openCLKernelsSource, "", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == moduleBinary.size()));

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(moduleBinary.data());
    moduleDesc.inputSize = moduleBinary.size();
    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "memcpy_bytes";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, numThreads, 1U, 1U, &groupSizeX,
                                                  &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE_BOOL(numThreads % groupSizeX == 0);
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Group size : (" << groupSizeX << ", " << groupSizeY << ", " << groupSizeZ
                  << ")" << std::endl;
    }
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    if (sharedCmdList == nullptr) {
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
        cmdQueueDesc.index = 0;
        LevelZeroBlackBoxTests::selectQueueMode(cmdQueueDesc, useSyncCmdQ);

        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        cmdList = sharedCmdList;
    }

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;

    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc,
                                          allocSize, 1, device, &srcBuffer));

    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc,
                                          allocSize, 1, device, &dstBuffer));

    uint8_t initDataSrc[allocSize];
    memset(initDataSrc, 7, sizeof(initDataSrc));
    uint8_t initDataDst[allocSize];
    memset(initDataDst, 3, sizeof(initDataDst));

    if (!useSyncCmdQ) {
        // Create Event Pool and kernel launch event
        if (sharedEvent == nullptr) {
            LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr, 1, &event, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        } else {
            event = sharedEvent;
        }
        if (sharedEvent2 == nullptr) {
            LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool2, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr, 1, &event2, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        } else {
            event2 = sharedEvent2;
        }
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, srcBuffer, initDataSrc,
                                                       sizeof(initDataSrc),
                                                       useSyncCmdQ ? nullptr : event,
                                                       0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, initDataDst,
                                                       sizeof(initDataDst),
                                                       useSyncCmdQ ? nullptr : event2,
                                                       0, nullptr));

    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event2, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostReset(event));
        SUCCESS_OR_TERMINATE(zeEventHostReset(event2));
    }

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcBuffer), &srcBuffer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = numThreads / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;
    LevelZeroBlackBoxTests::printGroupCount(dispatchTraits);

    SUCCESS_OR_TERMINATE_BOOL(dispatchTraits.groupCountX * groupSizeX == allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                         useSyncCmdQ ? nullptr : event, 0, nullptr));
    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
    }

    uint8_t readBackData[allocSize];
    memset(readBackData, 2, sizeof(readBackData));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, readBackData, dstBuffer,
                                                       sizeof(readBackData),
                                                       useSyncCmdQ ? nullptr : event2,
                                                       0, nullptr));
    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event2, std::numeric_limits<uint64_t>::max()));
    }

    validRet = LevelZeroBlackBoxTests::validate(initDataSrc, readBackData, sizeof(readBackData));

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));

    if (sharedCmdList == nullptr) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }

    if (sharedEvent == nullptr) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    }
    if (sharedEvent2 == nullptr) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event2));
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool2));
    }

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName("Zello Command List Immediate");
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool useSyncQueue = LevelZeroBlackBoxTests::isSyncQueueEnabled(argc, argv);
    bool commandListShared = LevelZeroBlackBoxTests::isCommandListShared(argc, argv);
    bool commandListCoexist = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-o", "--coexists");
    bool eventPoolShared = !LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-n", "--nopoolshared");
    if (eventPoolShared) {
        std::cout << "Event pool shared between tests" << std::endl;
    }
    if (commandListCoexist) {
        std::cout << "Command List coexists between tests" << std::endl;
        commandListShared = false;
    }

    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device0 = devices[0];

    ze_device_properties_t device0Properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device0, &device0Properties));
    LevelZeroBlackBoxTests::printDeviceProperties(device0Properties);

    bool outputValidationSuccessful = false;

    ze_event_pool_handle_t eventPool = nullptr, eventPool2 = nullptr;
    ze_event_handle_t event = nullptr, event2 = nullptr;

    if (!useSyncQueue && eventPoolShared) {
        // Create Event Pool and kernel launch event
        LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device0, eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr, 1, &event, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device0, eventPool2, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr, 1, &event2, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
    }

    ze_command_list_handle_t cmdList = nullptr;
    ze_command_list_handle_t cmdListShared = nullptr;
    if (commandListShared) {
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device0, false);
        cmdQueueDesc.index = 0;
        LevelZeroBlackBoxTests::selectQueueMode(cmdQueueDesc, useSyncQueue);
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device0, &cmdQueueDesc, &cmdListShared));
        cmdList = cmdListShared;
    }

    ze_command_list_handle_t cmdListStandardMemoryCopy = nullptr;
    ze_command_list_handle_t cmdListMemoryCopyRegion = nullptr;
    ze_command_list_handle_t cmdListLaunchGpuKernel = nullptr;
    if (commandListCoexist) {
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device0, false);
        cmdQueueDesc.index = 0;
        LevelZeroBlackBoxTests::selectQueueMode(cmdQueueDesc, useSyncQueue);

        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device0, &cmdQueueDesc, &cmdListStandardMemoryCopy));
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device0, &cmdQueueDesc, &cmdListMemoryCopyRegion));
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device0, &cmdQueueDesc, &cmdListLaunchGpuKernel));

        cmdList = cmdListStandardMemoryCopy;
    }

    std::string currentTest;
    currentTest = "Standard Memory Copy";
    testAppendMemoryCopy(context, device0, useSyncQueue, outputValidationSuccessful, cmdList, event, event2);
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

    if (outputValidationSuccessful || aubMode) {
        if (commandListCoexist) {
            cmdList = cmdListMemoryCopyRegion;
        }
        if (event != nullptr) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(event));
            SUCCESS_OR_TERMINATE(zeEventHostReset(event2));
        }
        currentTest = "Memory Copy Region";
        testAppendMemoryCopyRegion(context, device0, useSyncQueue, outputValidationSuccessful, cmdList, event, event2);
        LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
    }

    if (outputValidationSuccessful || aubMode) {
        if (commandListCoexist) {
            cmdList = cmdListLaunchGpuKernel;
        }
        if (event != nullptr) {
            SUCCESS_OR_TERMINATE(zeEventHostReset(event));
            SUCCESS_OR_TERMINATE(zeEventHostReset(event2));
        }
        currentTest = "Launch GPU Kernel";
        testAppendGpuKernel(context, device0, useSyncQueue, outputValidationSuccessful, cmdList, event, event2);
        LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
    }

    if (commandListShared) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListShared));
    }
    if (commandListCoexist) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListStandardMemoryCopy));
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListMemoryCopyRegion));
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListLaunchGpuKernel));
    }

    if (event != nullptr) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));

        SUCCESS_OR_TERMINATE(zeEventDestroy(event2));
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool2));
    }

    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

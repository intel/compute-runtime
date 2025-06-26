/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>

void createImmediateCommandList(ze_device_handle_t &device,
                                ze_context_handle_t &context,
                                uint32_t queueGroupOrdinal,
                                bool syncMode,
                                ze_command_list_handle_t &cmdList) {
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = queueGroupOrdinal;
    cmdQueueDesc.index = 0;
    LevelZeroBlackBoxTests::selectQueueMode(cmdQueueDesc, syncMode);
    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
}

void testCopyBetweenHostMemAndDeviceMem(ze_context_handle_t &context, ze_device_handle_t &device, bool syncMode, int32_t copyQueueGroup, bool &validRet, bool useEventBasedSync) {
    const size_t allocSize = 4096 + 7; // +7 to brake alignment and make it harder
    char *hostBuffer = nullptr;
    void *deviceBuffer = nullptr;
    char *stackBuffer = new char[allocSize];
    ze_command_list_handle_t cmdList;
    const bool isEventsUsed = useEventBasedSync && !syncMode;

    createImmediateCommandList(device, context, copyQueueGroup, syncMode, cmdList);

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, (void **)(&hostBuffer)));

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &deviceBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        hostBuffer[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer, 0, allocSize);

    // Create Events for synchronization
    ze_event_pool_handle_t eventPoolDevice, eventPoolHost;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> deviceEvents(numEvents), hostEvents(numEvents);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPoolDevice,
                                                     0, false, nullptr, nullptr,
                                                     numEvents, deviceEvents.data(),
                                                     ZE_EVENT_SCOPE_FLAG_SUBDEVICE,
                                                     0);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPoolHost,
                                                     ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr,
                                                     numEvents, hostEvents.data(),
                                                     ZE_EVENT_SCOPE_FLAG_HOST,
                                                     0);

    // Copy from host-allocated to device-allocated memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, deviceBuffer, hostBuffer, allocSize,
                                                       syncMode ? nullptr : deviceEvents[0],
                                                       0, nullptr));

    // Copy from device-allocated memory to stack
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer, deviceBuffer, allocSize,
                                                       isEventsUsed ? hostEvents[0] : nullptr,
                                                       syncMode ? 0 : 1,
                                                       syncMode ? nullptr : &deviceEvents[0]));

    if (!syncMode) {
        if (isEventsUsed) {
            // If Async mode, use event for sync
            SUCCESS_OR_TERMINATE(zeEventHostSynchronize(hostEvents[0], std::numeric_limits<uint64_t>::max()));
        } else {
            SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));
        }
    }

    // Validate stack and ze deviceBuffers have the original data from hostBuffer
    validRet = LevelZeroBlackBoxTests::validate(hostBuffer, stackBuffer, allocSize);

    delete[] stackBuffer;

    for (auto event : hostEvents) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    for (auto event : deviceEvents) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }

    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPoolHost));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPoolDevice));
    SUCCESS_OR_TERMINATE(zeMemFree(context, hostBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, deviceBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
}

void executeGpuKernelAndValidate(ze_context_handle_t &context, ze_device_handle_t &device, bool syncMode, bool &outputValidationSuccessful, bool useEventBasedSync) {
    ze_command_list_handle_t cmdList;

    uint32_t computeOrdinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    createImmediateCommandList(device, context, computeOrdinal, syncMode, cmdList);
    const auto isEventsUsed = useEventBasedSync && !syncMode;

    // Create two shared buffers
    constexpr size_t allocSize = 4096;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    void *srcBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &srcBuffer));

    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &dstBuffer));

    // Initialize memory
    constexpr uint8_t val = 55;
    memset(srcBuffer, val, allocSize);
    memset(dstBuffer, 0, allocSize);

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    std::ifstream file("copy_buffer_to_buffer.spv", std::ios::binary);

    ze_event_pool_handle_t eventPoolHost;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> hostEvents(numEvents);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPoolHost,
                                                     ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr,
                                                     numEvents, hostEvents.data(),
                                                     ZE_EVENT_SCOPE_FLAG_HOST,
                                                     0);

    if (file.is_open()) {
        file.seekg(0, file.end);
        auto length = file.tellg();
        file.seekg(0, file.beg);

        std::unique_ptr<char[]> spirvInput(new char[length]);
        file.read(spirvInput.get(), length);

        ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        ze_module_build_log_handle_t buildlog;
        moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(spirvInput.get());
        moduleDesc.inputSize = length;
        moduleDesc.pBuildFlags = "";

        if (zeModuleCreate(context, device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
            size_t szLog = 0;
            zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

            char *strLog = (char *)malloc(szLog);
            zeModuleBuildLogGetString(buildlog, &szLog, strLog);
            LevelZeroBlackBoxTests::printBuildLog(strLog);

            free(strLog);
            SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
            std::cerr << "\nZello Immediate Results validation FAILED. Module creation error."
                      << std::endl;
            SUCCESS_OR_TERMINATE_BOOL(false);
        }
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

        ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
        kernelDesc.pKernelName = "CopyBufferToBufferBytes";
        SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

        uint32_t groupSizeX = 32u;
        uint32_t groupSizeY = 1u;
        uint32_t groupSizeZ = 1u;
        SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

        uint32_t offset = 0;
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(dstBuffer), &dstBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(srcBuffer), &srcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(uint32_t), &offset));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 3, sizeof(uint32_t), &offset));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 4, sizeof(uint32_t), &offset));

        ze_group_count_t dispatchTraits;
        dispatchTraits.groupCountX = allocSize / groupSizeX;
        dispatchTraits.groupCountY = 1u;
        dispatchTraits.groupCountZ = 1u;

        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                             isEventsUsed ? hostEvents[0] : nullptr, 0, nullptr));
        file.close();
    } else {
        // Perform a GPU copy
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, srcBuffer, allocSize,
                                                           isEventsUsed ? hostEvents[0] : nullptr, 0, nullptr));
    }

    if (!syncMode) {
        std::chrono::high_resolution_clock::time_point start, end;
        start = std::chrono::high_resolution_clock::now();
        if (isEventsUsed) {
            // If Async mode, use event for sync
            SUCCESS_OR_TERMINATE(zeEventHostSynchronize(hostEvents[0], std::numeric_limits<uint64_t>::max()));
        } else {
            SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));
        }
        end = std::chrono::high_resolution_clock::now();
        std::cout << "Time to synchronize : " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }

    // Validate
    outputValidationSuccessful = LevelZeroBlackBoxTests::validate(srcBuffer, dstBuffer, allocSize);

    // Cleanup
    for (auto event : hostEvents) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPoolHost));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Immediate";

    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    int useEventBasedSync = LevelZeroBlackBoxTests::getParamValue(argc, argv, "-e", "--useEventsBasedSync", 1);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    bool outputValidationSuccessful = true;
    if (outputValidationSuccessful || aubMode) {
        // Sync mode with Compute queue
        std::cout << "Test case: Sync mode compute queue with Kernel launch \n";
        executeGpuKernelAndValidate(context, device, true, outputValidationSuccessful, useEventBasedSync);
    }
    if (outputValidationSuccessful || aubMode) {
        // Async mode with Compute queue
        std::cout << "\nTest case: Async mode compute queue with Kernel launch \n";
        executeGpuKernelAndValidate(context, device, false, outputValidationSuccessful, useEventBasedSync);
    }

    // Find copy queue in root device, if not found, try subdevices
    uint32_t copyQueueGroup = 0;
    bool copyQueueFound = false;
    auto copyQueueDev = devices[0];
    for (auto &rd : devices) {
        copyQueueGroup = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(rd);
        if (copyQueueGroup != std::numeric_limits<uint32_t>::max()) {
            copyQueueFound = true;
            copyQueueDev = rd;
            if (LevelZeroBlackBoxTests::verbose) {
                std::cout << "\nCopy queue group found in root device\n";
            }
            break;
        }
    }

    if (!copyQueueFound) {
        if (LevelZeroBlackBoxTests::verbose) {
            std::cout << "\nNo Copy queue group found in root device. Checking subdevices now...\n";
        }
        copyQueueGroup = 0;
        for (auto &rd : devices) {
            uint32_t subDevCount = 0;
            auto subdevs = LevelZeroBlackBoxTests::zelloGetSubDevices(rd, subDevCount);

            if (!subDevCount) {
                continue;
            }

            // Find subdev that has a copy engine. If not skip tests
            for (auto &sd : subdevs) {
                copyQueueGroup = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(sd);
                if (copyQueueGroup != std::numeric_limits<uint32_t>::max()) {
                    copyQueueFound = true;
                    copyQueueDev = sd;
                    break;
                }
            }

            if (copyQueueFound) {
                if (LevelZeroBlackBoxTests::verbose) {
                    std::cout << "\nCopy queue group found in sub device\n";
                }
                break;
            }
        }
    }

    if (!copyQueueFound) {
        std::cout << "No Copy queue group found. Skipping further test runs\n";
    } else {
        if (outputValidationSuccessful || aubMode) {
            // Sync mode with Copy queue
            std::cout << "\nTest case: Sync mode copy queue for memory copy\n";
            testCopyBetweenHostMemAndDeviceMem(context, copyQueueDev, true, copyQueueGroup, outputValidationSuccessful, useEventBasedSync);
        }
        if (outputValidationSuccessful || aubMode) {
            // Async mode with Copy queue
            std::cout << "\nTest case: Async mode copy queue for memory copy\n";
            testCopyBetweenHostMemAndDeviceMem(context, copyQueueDev, false, copyQueueGroup, outputValidationSuccessful, useEventBasedSync);
        }
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <fstream>
#include <iomanip>
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
    selectQueueMode(cmdQueueDesc, syncMode);
    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
}

void testCopyBetweenHostMemAndDeviceMem(ze_context_handle_t &context, ze_device_handle_t &device, bool syncMode, int32_t copyQueueGroup, bool &validRet) {
    const size_t allocSize = 4096 + 7; // +7 to brake alignment and make it harder
    char *hostBuffer = nullptr;
    void *deviceBuffer = nullptr;
    char *stackBuffer = new char[allocSize];
    ze_command_list_handle_t cmdList;

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
    createEventPoolAndEvents(context, device, eventPoolDevice,
                             (ze_event_pool_flag_t)(0),
                             numEvents, deviceEvents.data(),
                             ZE_EVENT_SCOPE_FLAG_SUBDEVICE,
                             (ze_event_scope_flag_t)0);
    createEventPoolAndEvents(context, device, eventPoolHost,
                             (ze_event_pool_flag_t)(ZE_EVENT_POOL_FLAG_HOST_VISIBLE),
                             numEvents, hostEvents.data(),
                             ZE_EVENT_SCOPE_FLAG_HOST,
                             (ze_event_scope_flag_t)0);

    // Copy from host-allocated to device-allocated memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, deviceBuffer, hostBuffer, allocSize,
                                                       syncMode ? nullptr : deviceEvents[0],
                                                       0, nullptr));

    // Copy from device-allocated memory to stack
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer, deviceBuffer, allocSize,
                                                       syncMode ? nullptr : hostEvents[0],
                                                       syncMode ? 0 : 1,
                                                       syncMode ? nullptr : &deviceEvents[0]));

    if (!syncMode) {
        // If Async mode, use event for sync
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(hostEvents[0], std::numeric_limits<uint64_t>::max()));
    }

    // Validate stack and xe deviceBuffers have the original data from hostBuffer
    validRet = (0 == memcmp(hostBuffer, stackBuffer, allocSize));

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

void executeGpuKernelAndValidate(ze_context_handle_t &context, ze_device_handle_t &device, bool syncMode, bool &outputValidationSuccessful) {
    ze_command_list_handle_t cmdList;

    uint32_t computeOrdinal = getCommandQueueOrdinal(device);
    createImmediateCommandList(device, context, computeOrdinal, syncMode, cmdList);

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
    createEventPoolAndEvents(context, device, eventPoolHost,
                             (ze_event_pool_flag_t)(ZE_EVENT_POOL_FLAG_HOST_VISIBLE),
                             numEvents, hostEvents.data(),
                             ZE_EVENT_SCOPE_FLAG_HOST,
                             (ze_event_scope_flag_t)0);

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
            std::cout << "Build log:" << strLog << std::endl;

            free(strLog);
            SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
            std::cout << "\nZello Immediate Results validation FAILED. Module creation error."
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
                                                             syncMode ? nullptr : hostEvents[0], 0, nullptr));
        file.close();
    } else {
        // Perform a GPU copy
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, srcBuffer, allocSize,
                                                           syncMode ? nullptr : hostEvents[0], 0, nullptr));
    }

    if (!syncMode) {
        // If Async mode, use event for sync
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(hostEvents[0], std::numeric_limits<uint64_t>::max()));
    }

    // Validate
    outputValidationSuccessful = true;
    if (memcmp(dstBuffer, srcBuffer, allocSize)) {
        outputValidationSuccessful = false;
        uint8_t *srcCharBuffer = static_cast<uint8_t *>(srcBuffer);
        uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);
        for (size_t i = 0; i < allocSize; i++) {
            if (srcCharBuffer[i] != dstCharBuffer[i]) {
                std::cout << "srcBuffer[" << i << "] = " << static_cast<unsigned int>(srcCharBuffer[i]) << " not equal to "
                          << "dstBuffer[" << i << "] = " << static_cast<unsigned int>(dstCharBuffer[i]) << "\n";
                break;
            }
        }
    }

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

    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    bool outputValidationSuccessful = true;
    if (outputValidationSuccessful || aubMode) {
        //Sync mode with Compute queue
        std::cout << "Test case: Sync mode compute queue with Kernel launch \n";
        executeGpuKernelAndValidate(context, device, true, outputValidationSuccessful);
    }
    if (outputValidationSuccessful || aubMode) {
        //Async mode with Compute queue
        std::cout << "\nTest case: Async mode compute queue with Kernel launch \n";
        executeGpuKernelAndValidate(context, device, false, outputValidationSuccessful);
    }

    // Find copy queue in root device, if not found, try subdevices
    int32_t copyQueueGroup = 0;
    bool copyQueueFound = false;
    auto copyQueueDev = devices[0];
    for (auto &rd : devices) {
        copyQueueGroup = getCopyOnlyCommandQueueOrdinal(rd);
        if (copyQueueGroup >= 0) {
            copyQueueFound = true;
            copyQueueDev = rd;
            if (verbose) {
                std::cout << "\nCopy queue group found in root device\n";
            }
            break;
        }
    }

    if (!copyQueueFound) {
        if (verbose) {
            std::cout << "\nNo Copy queue group found in root device. Checking subdevices now...\n";
        }
        copyQueueGroup = 0;
        for (auto &rd : devices) {
            int subDevCount = 0;
            auto subdevs = zelloGetSubDevices(rd, subDevCount);

            if (!subDevCount) {
                continue;
            }

            // Find subdev that has a copy engine. If not skip tests
            for (auto &sd : subdevs) {
                copyQueueGroup = getCopyOnlyCommandQueueOrdinal(sd);
                if (copyQueueGroup >= 0) {
                    copyQueueFound = true;
                    copyQueueDev = sd;
                    break;
                }
            }

            if (copyQueueFound) {
                if (verbose) {
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
            //Sync mode with Copy queue
            std::cout << "\nTest case: Sync mode copy queue for memory copy\n";
            testCopyBetweenHostMemAndDeviceMem(context, copyQueueDev, true, copyQueueGroup, outputValidationSuccessful);
        }
        if (outputValidationSuccessful || aubMode) {
            //Async mode with Copy queue
            std::cout << "\nTest case: Async mode copy queue for memory copy\n";
            testCopyBetweenHostMemAndDeviceMem(context, copyQueueDev, false, copyQueueGroup, outputValidationSuccessful);
        }
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

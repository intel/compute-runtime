/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>

uint32_t numCmdListsToCreate = 100;

void stressCommandList(ze_context_handle_t &context, ze_device_handle_t &device, bool useImmediateCmdList, bool &outputValidationSuccessful) {
    std::vector<ze_command_list_handle_t> cmdLists;

    // Create a command queue for regular command lists
    ze_command_queue_handle_t cmdQueue = nullptr;
    if (!useImmediateCmdList) {
        ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        queueDesc.ordinal = 0;
        queueDesc.index = 0;
        queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
        SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &queueDesc, &cmdQueue));
    }

    // Create shared buffers
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

    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open copy_buffer_to_buffer.spv - kernel file required for this test" << std::endl;
        outputValidationSuccessful = false;
        SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
        SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
        if (cmdQueue) {
            SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
        }
        return;
    }

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
        std::cerr << "\nZello Stress CommandList validation FAILED. Module creation error." << std::endl;
        outputValidationSuccessful = false;
        SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
        SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
        if (cmdQueue) {
            SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
        }
        file.close();
        return;
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

    std::cout << "Creating " << numCmdListsToCreate << " command lists..." << std::endl;
    std::cout << "Each command list will execute a simple kernel." << std::endl;

    std::chrono::high_resolution_clock::time_point start, end;
    start = std::chrono::high_resolution_clock::now();

    // Create many command lists - each allocates resources from pools
    for (uint32_t i = 0; i < numCmdListsToCreate; i++) {
        ze_command_list_handle_t cmdList;

        if (useImmediateCmdList) {
            ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
            queueDesc.ordinal = 0;
            queueDesc.index = 0;
            queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS; // Changed to ASYNCHRONOUS
            SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &queueDesc, &cmdList));
        } else {
            ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
            SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));
        }

        // Append a kernel launch to each command list
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                             nullptr, 0, nullptr));

        if (!useImmediateCmdList) {
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        }

        cmdLists.push_back(cmdList);

        // Print progress
        uint32_t progressInterval = numCmdListsToCreate / 10;
        if (progressInterval == 0) {
            progressInterval = 1;
        }
        if ((i + 1) % progressInterval == 0 || (i + 1) == numCmdListsToCreate) {
            std::cout << "  Created " << (i + 1) << " / " << numCmdListsToCreate << " command lists" << std::endl;
        }
    }

    end = std::chrono::high_resolution_clock::now();
    auto creationTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    auto creationTimeMs = creationTimeNs / 1000000.0;

    std::cout << "\nCommand List Creation Results:" << std::endl;
    std::cout << "  Total command lists created: " << numCmdListsToCreate << std::endl;
    std::cout << "  Total time: " << creationTimeMs << " ms" << std::endl;
    std::cout << "  Average time per command list: " << (creationTimeNs / numCmdListsToCreate) << " ns" << std::endl;

    // Execute and validate
    std::cout << "\nExecuting command lists..." << std::endl;

    if (!useImmediateCmdList) {
        start = std::chrono::high_resolution_clock::now();
        std::cout << "  Submitting " << cmdLists.size() << " command list(s)..." << std::endl;
        for (size_t i = 0; i < cmdLists.size(); i++) {
            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdLists[i], nullptr));
        }
        std::cout << "  Synchronizing command queue..." << std::endl;
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
        std::cout << "  Synchronization complete." << std::endl;
        end = std::chrono::high_resolution_clock::now();

        auto execTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        auto execTimeMs = execTimeNs / 1000000.0;
        std::cout << "  Execution time: " << execTimeMs << " ms" << std::endl;
    } else {
        // For immediate command lists in async mode, synchronize all of them
        start = std::chrono::high_resolution_clock::now();
        for (auto cmdList : cmdLists) {
            SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));
        }
        end = std::chrono::high_resolution_clock::now();

        auto execTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        auto execTimeMs = execTimeNs / 1000000.0;
        std::cout << "  Synchronization time: " << execTimeMs << " ms" << std::endl;
    }

    // Validate final result
    std::cout << "\nValidating final kernel execution result..." << std::endl;
    outputValidationSuccessful = LevelZeroBlackBoxTests::validate(srcBuffer, dstBuffer, allocSize);

    // Cleanup - destroy all command lists
    std::cout << "Destroying " << cmdLists.size() << " command lists..." << std::endl;
    for (auto cmdList : cmdLists) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    if (cmdQueue) {
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    }
    file.close();
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Stress CommandList";

    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-h" || std::string(argv[i]) == "--help") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n";
            std::cout << "Options:\n";
            std::cout << "  -n, --num-cmdlists <number>  Number of command lists to create (default: 100, max: 10000)\n";
            std::cout << "  --verbose                    Enable verbose output\n";
            std::cout << "  -h, --help                   Show this help message\n";
            return 0;
        }
        if (std::string(argv[i]) == "-n" || std::string(argv[i]) == "--num-cmdlists") {
            if (i + 1 < argc) {
                numCmdListsToCreate = std::stoi(argv[i + 1]);
                if (numCmdListsToCreate > 10000) {
                    std::cerr << "Warning: Number of command lists limited to 10000 (requested: " << numCmdListsToCreate << ")" << std::endl;
                    numCmdListsToCreate = 10000;
                }
                i++;
            }
        }
    }

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    bool outputValidationSuccessful = true;

    std::cout << "\nThis test stresses command list creation by creating "
              << numCmdListsToCreate << " command lists." << std::endl;

    if (outputValidationSuccessful || aubMode) {
        // Regular command lists test
        std::cout << "\nTest Mode: REGULAR COMMAND LISTS";
        std::cout << "\n================================" << std::endl;
        stressCommandList(context, device, false, outputValidationSuccessful);
    }

    if (outputValidationSuccessful || aubMode) {
        // Immediate command lists test
        std::cout << "\nTest Mode: IMMEDIATE COMMAND LISTS";
        std::cout << "\n==================================" << std::endl;
        stressCommandList(context, device, true, outputValidationSuccessful);
    }

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
}

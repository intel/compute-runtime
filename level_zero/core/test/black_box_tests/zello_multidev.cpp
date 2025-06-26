/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Multidev";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    // Set-up
    constexpr size_t allocSize = 4096;
    constexpr size_t bytesPerThread = sizeof(char);
    constexpr size_t numThreads = allocSize / bytesPerThread;
    std::vector<ze_module_handle_t> module;
    std::vector<ze_device_handle_t> devices;
    std::vector<std::string> deviceNames;
    std::vector<ze_kernel_handle_t> kernel;
    std::vector<ze_command_queue_handle_t> cmdQueue;
    std::vector<ze_command_list_handle_t> cmdList;
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;
    bool outputValidationSuccessful = false;

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    uint32_t deviceCount = (uint32_t)devices.size();

    // Get subdevices for each device and add to total count of devices
    for (uint32_t i = 0; i < deviceCount; i++) {
        uint32_t count = 0;
        SUCCESS_OR_TERMINATE(zeDeviceGetSubDevices(devices[i], &count, nullptr));

        deviceCount += count;
        devices.resize(deviceCount);

        SUCCESS_OR_TERMINATE(zeDeviceGetSubDevices(devices[i], &count,
                                                   devices.data() + (deviceCount - count)));
    }

    deviceNames.resize(devices.size());

    for (uint32_t i = 0; i < deviceCount; i++) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        SUCCESS_OR_TERMINATE(zeDeviceGetProperties(devices[i], &deviceProperties));
        LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

        deviceNames[i].assign(deviceProperties.name, strlen(deviceProperties.name));

        uint32_t cachePropertiesCount = 0;
        SUCCESS_OR_TERMINATE(zeDeviceGetCacheProperties(devices[i], &cachePropertiesCount, nullptr));

        std::vector<ze_device_cache_properties_t> cacheProperties;
        cacheProperties.resize(cachePropertiesCount);
        SUCCESS_OR_TERMINATE(zeDeviceGetCacheProperties(devices[i], &cachePropertiesCount, cacheProperties.data()));

        for (uint32_t cacheIndex = 0; cacheIndex < cachePropertiesCount; cacheIndex++) {
            LevelZeroBlackBoxTests::printCacheProperties(cacheIndex, cacheProperties[cacheIndex]);
        }

        ze_device_p2p_properties_t deviceP2PProperties = {ZE_STRUCTURE_TYPE_DEVICE_P2P_PROPERTIES};
        for (uint32_t j = 0; j < deviceCount; j++) {
            if (j == i)
                continue;
            SUCCESS_OR_TERMINATE(zeDeviceGetP2PProperties(devices[i], devices[j], &deviceP2PProperties));
            ze_bool_t canAccessPeer = false;
            SUCCESS_OR_TERMINATE(zeDeviceCanAccessPeer(devices[i], devices[j], &canAccessPeer));
            LevelZeroBlackBoxTests::printP2PProperties(deviceP2PProperties, canAccessPeer, i, j);
            if (canAccessPeer == false) {
                std::cerr << "Device " << i << " cannot access " << j << "\n";
                std::terminate();
            }
        }
    }

    module.resize(deviceCount);
    cmdQueue.resize(deviceCount);
    cmdList.resize(deviceCount);
    kernel.resize(deviceCount);

    std::string buildLog;
    auto moduleBinary = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::memcpyBytesTestKernelSrc, "", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == moduleBinary.size()));

    // init everything
    for (uint32_t i = 0; i < deviceCount; i++) {
        std::cout << "Creating objects for device " << i << " " << deviceNames[i] << "\n";
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(devices[i], false);
        cmdQueueDesc.index = 0;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, devices[i], &cmdQueueDesc, &cmdQueue[i]));

        ze_command_list_desc_t cmdListDesc = {};
        cmdListDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
        cmdListDesc.pNext = nullptr;
        cmdListDesc.flags = 0;
        SUCCESS_OR_TERMINATE(zeCommandListCreate(context, devices[i], &cmdListDesc, &cmdList[i]));

        ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(moduleBinary.data());
        moduleDesc.inputSize = moduleBinary.size();
        SUCCESS_OR_TERMINATE(zeModuleCreate(context, devices[i], &moduleDesc, &module[i], nullptr));

        ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
        kernelDesc.pKernelName = "memcpy_bytes";
        SUCCESS_OR_TERMINATE(zeKernelCreate(module[i], &kernelDesc, &kernel[i]));
    }

    // iterate over devices and launch the kernel
    for (uint32_t i = 0; i < deviceCount; i++) {
        std::cout << "Launching kernels for device " << i << " " << deviceNames[i] << "\n";
        uint32_t groupSizeX = 32u;
        uint32_t groupSizeY = 1u;
        uint32_t groupSizeZ = 1u;
        SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel[i], numThreads, 1U, 1U,
                                                      &groupSizeX, &groupSizeY, &groupSizeZ));
        SUCCESS_OR_TERMINATE_BOOL(numThreads % groupSizeX == 0);
        if (LevelZeroBlackBoxTests::verbose) {
            std::cout << "Group size : (" << groupSizeX << ", " << groupSizeY << ", " << groupSizeZ
                      << ")" << std::endl;
        }
        SUCCESS_OR_TERMINATE(
            zeKernelSetGroupSize(kernel[i], groupSizeX, groupSizeY, groupSizeZ));

        // Alloc buffers
        srcBuffer = nullptr;
        dstBuffer = nullptr;

        ze_device_mem_alloc_desc_t deviceDesc = {};
        deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
        deviceDesc.ordinal = i;
        deviceDesc.flags = 0;
        deviceDesc.pNext = nullptr;

        ze_host_mem_alloc_desc_t hostDesc = {};
        hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
        hostDesc.pNext = nullptr;
        hostDesc.flags = 0;

        SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc,
                                              allocSize, 1, devices[i], &srcBuffer));
        SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc,
                                              allocSize, 1, devices[i], &dstBuffer));

        // Init data and copy to device
        uint8_t initDataSrc[allocSize];
        memset(initDataSrc, 7, sizeof(initDataSrc));
        uint8_t initDataDst[allocSize];
        memset(initDataDst, 3, sizeof(initDataDst));
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(
            cmdList[i], srcBuffer, initDataSrc, sizeof(initDataSrc), nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(
            cmdList[i], dstBuffer, initDataDst, sizeof(initDataDst), nullptr, 0, nullptr));

        // copying of data must finish before running the user kernel
        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList[i], nullptr, 0, nullptr));

        // set kernel args and get ready to dispatch
        SUCCESS_OR_TERMINATE(
            zeKernelSetArgumentValue(kernel[i], 0, sizeof(dstBuffer), &dstBuffer));
        SUCCESS_OR_TERMINATE(
            zeKernelSetArgumentValue(kernel[i], 1, sizeof(srcBuffer), &srcBuffer));

        ze_group_count_t dispatchTraits;
        dispatchTraits.groupCountX = numThreads / groupSizeX;
        dispatchTraits.groupCountY = 1u;
        dispatchTraits.groupCountZ = 1u;
        LevelZeroBlackBoxTests::printGroupCount(dispatchTraits);

        SUCCESS_OR_TERMINATE_BOOL(dispatchTraits.groupCountX * groupSizeX == allocSize);
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(
            cmdList[i], kernel[i], &dispatchTraits, nullptr, 0, nullptr));

        // barrier to complete kernel
        uint8_t readBackData[allocSize];
        memset(readBackData, 2, sizeof(readBackData));
        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList[i], nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(
            cmdList[i], readBackData, dstBuffer, sizeof(readBackData), nullptr, 0, nullptr));

        // Dispatch and wait
        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList[i]));
        SUCCESS_OR_TERMINATE(
            zeCommandQueueExecuteCommandLists(cmdQueue[i], 1, &cmdList[i], nullptr));
        auto synchronizationResult = zeCommandQueueSynchronize(cmdQueue[i], std::numeric_limits<uint64_t>::max());
        SUCCESS_OR_WARNING(synchronizationResult);

        // Validate
        outputValidationSuccessful = LevelZeroBlackBoxTests::validate(initDataSrc, readBackData, allocSize);

        // Release Mem
        SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
        SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));

        // Break immediately if output validation is false
        if (!outputValidationSuccessful) {
            break;
        }
    }

    for (uint32_t i = 0; i < deviceCount; i++) {
        std::cout << "Freeing objects for device " << i << " " << deviceNames[i] << "\n";
        SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel[i]));
        SUCCESS_OR_TERMINATE(zeModuleDestroy(module[i]));
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList[i]));
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue[i]));
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

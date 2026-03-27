/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <cstring>
#include <iostream>

bool testUsableMemoryProperties(ze_device_handle_t &device) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_usablemem_size_ext_properties_t usableMemProps = {ZE_STRUCTURE_TYPE_DEVICE_USABLEMEM_SIZE_EXT_PROPERTIES};
    usableMemProps.pNext = nullptr;
    usableMemProps.currUsableMemSize = 0;

    deviceProperties.pNext = &usableMemProps;

    ze_result_t result = zeDeviceGetProperties(device, &deviceProperties);
    if (result != ZE_RESULT_SUCCESS) {
        std::cerr << "zeDeviceGetProperties failed with error: " << result << std::endl;
        return false;
    }

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Device Usable Memory Properties:" << std::endl;
        std::cout << "  Current Usable Memory Size: " << usableMemProps.currUsableMemSize << " bytes" << std::endl;
        std::cout << "  Current Usable Memory Size: " << (usableMemProps.currUsableMemSize / static_cast<double>(MemoryConstants::gigaByte)) << " GB" << std::endl;
    }

    // Validate that the usable memory is within reasonable bounds
    uint32_t memPropertiesCount = 0;
    result = zeDeviceGetMemoryProperties(device, &memPropertiesCount, nullptr);
    if (result != ZE_RESULT_SUCCESS || memPropertiesCount == 0) {
        std::cerr << "Failed to get memory properties count" << std::endl;
        return false;
    }

    std::vector<ze_device_memory_properties_t> memPropertiesArray(memPropertiesCount);
    for (uint32_t i = 0; i < memPropertiesCount; i++) {
        memPropertiesArray[i].stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES;
        memPropertiesArray[i].pNext = nullptr;
    }
    result = zeDeviceGetMemoryProperties(device, &memPropertiesCount, memPropertiesArray.data());
    if (result != ZE_RESULT_SUCCESS) {
        std::cerr << "Failed to get memory properties" << std::endl;
        return false;
    }

    // Calculate total device memory
    uint64_t totalDeviceMemory = 0;
    for (uint32_t i = 0; i < memPropertiesCount; i++) {
        totalDeviceMemory += memPropertiesArray[i].totalSize;
    }

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "  Total Device Memory: " << totalDeviceMemory << " bytes" << std::endl;
        std::cout << "  Total Device Memory: " << (totalDeviceMemory / static_cast<double>(MemoryConstants::gigaByte)) << " GB" << std::endl;
    }

    // Usable memory should be <= total memory
    if (usableMemProps.currUsableMemSize > totalDeviceMemory) {
        std::cerr << "Usable memory (" << usableMemProps.currUsableMemSize
                  << ") exceeds total device memory (" << totalDeviceMemory << ")" << std::endl;
        return false;
    }

    std::cout << "Usable memory property validation successful" << std::endl;
    return true;
}

bool testUsableMemoryAfterAllocation(ze_context_handle_t &context, ze_device_handle_t &device) {
    // Get initial usable memory
    ze_device_properties_t deviceProperties1 = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_usablemem_size_ext_properties_t usableMemProps1 = {ZE_STRUCTURE_TYPE_DEVICE_USABLEMEM_SIZE_EXT_PROPERTIES};
    deviceProperties1.pNext = &usableMemProps1;
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties1));
    uint64_t initialUsableMemory = usableMemProps1.currUsableMemSize;

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "\nTesting usable memory after allocation:" << std::endl;
        std::cout << "  Initial usable memory: " << initialUsableMemory << " bytes" << std::endl;
        std::cout << "  Initial usable memory: " << (initialUsableMemory / static_cast<double>(MemoryConstants::gigaByte)) << " GB" << std::endl;
    }

    // Allocate some device memory
    constexpr size_t allocSize = MemoryConstants::gigaByte; // 1 GB
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    void *deviceBuffer = nullptr;
    ze_result_t result = zeMemAllocDevice(context, &deviceDesc, allocSize, 1, device, &deviceBuffer);
    if (result != ZE_RESULT_SUCCESS) {
        if (LevelZeroBlackBoxTests::verbose) {
            std::cout << " Memory allocation failed " << std::endl;
            return false;
        }
    }

    SUCCESS_OR_TERMINATE(zeContextMakeMemoryResident(context, device, deviceBuffer, allocSize));

    // Get usable memory after allocation
    ze_device_properties_t deviceProperties2 = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_usablemem_size_ext_properties_t usableMemProps2 = {ZE_STRUCTURE_TYPE_DEVICE_USABLEMEM_SIZE_EXT_PROPERTIES};
    deviceProperties2.pNext = &usableMemProps2;
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties2));
    uint64_t usableMemoryAfterAlloc = usableMemProps2.currUsableMemSize;

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "  Memory consumed: " << (initialUsableMemory - usableMemoryAfterAlloc) << " bytes" << std::endl;
        std::cout << "  Usable memory after allocation: " << usableMemoryAfterAlloc << " bytes" << std::endl;
        std::cout << "  Usable memory after allocation: " << (usableMemoryAfterAlloc / static_cast<double>(MemoryConstants::gigaByte)) << " GB" << std::endl;
    }

    // Usable memory should have decreased (or at least not increased significantly)
    if (usableMemoryAfterAlloc > initialUsableMemory + allocSize) {
        std::cerr << "Unexpected increase in usable memory after allocation" << std::endl;
        SUCCESS_OR_TERMINATE(zeMemFree(context, deviceBuffer));
        return false;
    }

    // Free the memory
    SUCCESS_OR_TERMINATE(zeMemFree(context, deviceBuffer));

    // Get usable memory after freeing
    ze_device_properties_t deviceProperties3 = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_usablemem_size_ext_properties_t usableMemProps3 = {ZE_STRUCTURE_TYPE_DEVICE_USABLEMEM_SIZE_EXT_PROPERTIES};
    deviceProperties3.pNext = &usableMemProps3;
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties3));
    uint64_t usableMemoryAfterFree = usableMemProps3.currUsableMemSize;

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "  Usable memory after freeing: " << usableMemoryAfterFree << " bytes" << std::endl;
        std::cout << "  Usable memory after freeing: " << (usableMemoryAfterFree / static_cast<double>(MemoryConstants::gigaByte)) << " GB" << std::endl;
    }

    std::cout << "Usable memory allocation test successful" << std::endl;
    return true;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Usable Memory Properties";
    bool outputValidationSuccessful = true;
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    // Check if the usable memory extension is supported
    uint32_t extensionCount = 0;
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionProperties(driverHandle, &extensionCount, nullptr));
    std::vector<ze_driver_extension_properties_t> extensions(extensionCount);
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionProperties(driverHandle, &extensionCount, extensions.data()));

    bool usableMemExtensionSupported = false;
    for (const auto &extension : extensions) {
        if (strcmp(extension.name, ZE_DEVICE_USABLEMEM_SIZE_PROPERTIES_EXT_NAME) == 0) {
            usableMemExtensionSupported = true;
            if (LevelZeroBlackBoxTests::verbose) {
                std::cout << "Found extension: " << extension.name
                          << " version: " << ZE_MAJOR_VERSION(extension.version) << "."
                          << ZE_MINOR_VERSION(extension.version) << std::endl;
            }
            break;
        }
    }

    if (!usableMemExtensionSupported) {
        std::cout << "Usable memory extension not supported, skipping tests" << std::endl;
        return 0;
    }

    // Test 1: Basic usable memory properties
    std::string currentTest = "Basic usable memory properties";
    outputValidationSuccessful = testUsableMemoryProperties(device);
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

    // Test 2: Usable memory after allocation
    if (outputValidationSuccessful || aubMode) {
        currentTest = "Usable memory after allocation";
        outputValidationSuccessful = testUsableMemoryAfterAllocation(context, device);
        LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
    }

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

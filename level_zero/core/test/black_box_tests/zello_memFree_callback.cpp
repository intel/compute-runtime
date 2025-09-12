/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <cstring>
#include <memory>

// Global variable to track callback execution
bool callbackExecuted = false;
void *callbackUserData = nullptr;

// Memory free callback function
void memoryFreeCallback(void *pUserData) {
    std::cout << "Memory free callback invoked!" << std::endl;
    std::cout << "User data: " << static_cast<const char *>(pUserData) << std::endl;
    callbackExecuted = true;
    callbackUserData = pUserData;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Memory Free Callback";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    std::cout << "=== " << blackBoxName << " Demo ===" << std::endl;

    ze_context_handle_t contextDefault = nullptr;
    ze_driver_handle_t driver;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(contextDefault, driver);
    auto device = devices[0];
    ze_context_handle_t context = nullptr;
    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    contextDesc.pNext = nullptr;
    contextDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeContextCreate(driver, &contextDesc, &context));

    // Check if experimental callback extension is available
    void *funcPtr = nullptr;
    ze_result_t result = zeDriverGetExtensionFunctionAddress(driver, "zexMemFreeRegisterCallbackExt", &funcPtr);
    if (result != ZE_RESULT_SUCCESS || funcPtr == nullptr) {
        std::cerr << "ERROR: zexMemFreeRegisterCallbackExt extension not available!" << std::endl;
        zeContextDestroy(context);
        return 1;
    }

    using zexMemFreeRegisterCallbackExtFunc = ze_result_t (*)(ze_context_handle_t, zex_memory_free_callback_ext_desc_t *, void *);
    auto zexMemFreeRegisterCallbackExtPtr = reinterpret_cast<zexMemFreeRegisterCallbackExtFunc>(funcPtr);
    std::cout << "Memory free callback extension available" << std::endl;

    // Allocate memory
    constexpr size_t allocSize = 1024;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    void *ptr = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &ptr));
    std::cout << "Allocated " << allocSize << " bytes at address: " << ptr << std::endl;

    // Initialize the memory with a pattern
    memset(ptr, 0xAB, allocSize);
    std::cout << "Memory initialized with pattern 0xAB" << std::endl;

    // Setup callback descriptor
    const char *userData = "Hello from callback!";
    zex_memory_free_callback_ext_desc_t callbackDesc = {};
    callbackDesc.stype = ZEX_STRUCTURE_TYPE_MEMORY_FREE_CALLBACK_EXT_DESC;
    callbackDesc.pNext = nullptr;
    callbackDesc.pfnCallback = memoryFreeCallback;
    callbackDesc.pUserData = const_cast<char *>(userData);

    // Register the callback
    std::cout << "Registering memory free callback..." << std::endl;
    SUCCESS_OR_TERMINATE(zexMemFreeRegisterCallbackExtPtr(context, &callbackDesc, ptr));
    std::cout << "Memory free callback registered successfully" << std::endl;

    // Verify memory contents before freeing
    std::cout << "Verifying memory contents..." << std::endl;
    uint8_t *bytePtr = static_cast<uint8_t *>(ptr);
    bool memoryValid = true;
    for (size_t i = 0; i < 16; i++) { // Check first 16 bytes
        if (bytePtr[i] != 0xAB) {
            memoryValid = false;
            break;
        }
    }
    std::cout << "Memory contents " << (memoryValid ? "valid" : "invalid") << std::endl;

    // Free the memory - this should trigger the callback
    std::cout << "Freeing memory (this should trigger the callback)..." << std::endl;
    SUCCESS_OR_TERMINATE(zeMemFree(context, ptr));
    std::cout << "Memory freed" << std::endl;

    // Check if callback was executed
    if (callbackExecuted) {
        std::cout << "SUCCESS: Callback was executed!" << std::endl;
        if (callbackUserData == userData) {
            std::cout << "SUCCESS: User data matches!" << std::endl;
        } else {
            std::cout << "WARNING: User data mismatch!" << std::endl;
        }
    } else {
        std::cout << "WARNING: Callback was not executed!" << std::endl;
    }

    // Allocate and free memory without callback for comparison
    std::cout << "\nTesting memory allocation without callback..." << std::endl;
    void *ptr2 = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &ptr2));
    std::cout << "Allocated second buffer at: " << ptr2 << std::endl;

    // Reset callback state
    callbackExecuted = false;
    callbackUserData = nullptr;

    SUCCESS_OR_TERMINATE(zeMemFree(context, ptr2));
    std::cout << "Freed second buffer" << std::endl;

    if (!callbackExecuted) {
        std::cout << "SUCCESS: No callback executed for unregistered memory (as expected)" << std::endl;
    } else {
        std::cout << "UNEXPECTED: Callback was executed for unregistered memory!" << std::endl;
    }

    // Test with device memory allocation
    std::cout << "\nTesting device memory allocation with callback..." << std::endl;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    void *devicePtr = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, 1, device, &devicePtr));
    std::cout << "Allocated device memory at: " << devicePtr << std::endl;

    // Register callback for device memory
    const char *deviceUserData = "Device memory callback!";
    zex_memory_free_callback_ext_desc_t deviceCallbackDesc = {};
    deviceCallbackDesc.stype = ZEX_STRUCTURE_TYPE_MEMORY_FREE_CALLBACK_EXT_DESC;
    deviceCallbackDesc.pNext = nullptr;
    deviceCallbackDesc.pfnCallback = memoryFreeCallback;
    deviceCallbackDesc.pUserData = const_cast<char *>(deviceUserData);

    SUCCESS_OR_TERMINATE(zexMemFreeRegisterCallbackExtPtr(context, &deviceCallbackDesc, devicePtr));
    std::cout << "Device memory callback registered" << std::endl;

    // Reset callback state
    callbackExecuted = false;
    callbackUserData = nullptr;

    // Free device memory
    SUCCESS_OR_TERMINATE(zeMemFree(context, devicePtr));
    std::cout << "Device memory freed" << std::endl;

    if (callbackExecuted) {
        std::cout << "SUCCESS: Device memory callback executed!" << std::endl;
    } else {
        std::cout << "WARNING: Device memory callback was not executed!" << std::endl;
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
    std::cout << "Context destroyed" << std::endl;

    std::cout << "\n=== " << blackBoxName << " Demo Complete ===" << std::endl;

    // Final validation
    bool outputValidationSuccessful = callbackExecuted;
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);

    return outputValidationSuccessful ? 0 : 1;
}

/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

typedef ze_result_t (*pFnzexDriverImportExternalPointer)(ze_driver_handle_t, void *, size_t);
typedef ze_result_t (*pFnzexDriverReleaseImportedPointer)(ze_driver_handle_t, void *);
typedef ze_result_t (*pFnzexDriverGetHostPointerBaseAddress)(ze_driver_handle_t, void *, void **);

void executeGpuKernelAndValidate(ze_driver_handle_t &driverHandle, ze_context_handle_t &context, ze_device_handle_t &device, bool &outputValidationSuccessful) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_command_list_handle_t cmdList;

    cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));

    // Create memory
    constexpr size_t allocSize = 65536;
    uint8_t *srcBuffer = new uint8_t[allocSize];
    uint8_t *dstBuffer = new uint8_t[allocSize];

    // Initialize memory
    constexpr uint8_t val = 55;
    memset(srcBuffer, val, allocSize);
    memset(dstBuffer, 0, allocSize);

    // Get pointers to host pointer interfaces
    pFnzexDriverImportExternalPointer zexDriverImportExternalPointer = nullptr;
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverImportExternalPointer", reinterpret_cast<void **>(&zexDriverImportExternalPointer)));

    pFnzexDriverReleaseImportedPointer zexDriverReleaseImportedPointer = nullptr;
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverReleaseImportedPointer", reinterpret_cast<void **>(&zexDriverReleaseImportedPointer)));

    pFnzexDriverGetHostPointerBaseAddress zexDriverGetHostPointerBaseAddress = nullptr;
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexDriverGetHostPointerBaseAddress", reinterpret_cast<void **>(&zexDriverGetHostPointerBaseAddress)));

    //Import memory
    SUCCESS_OR_TERMINATE(zexDriverImportExternalPointer(driverHandle, srcBuffer, allocSize));
    SUCCESS_OR_TERMINATE(zexDriverImportExternalPointer(driverHandle, dstBuffer, allocSize));

    // Perform a GPU copy
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, srcBuffer, allocSize, nullptr, 0, nullptr));

    // Close list and submit for execution
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

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
    SUCCESS_OR_TERMINATE(zexDriverReleaseImportedPointer(driverHandle, srcBuffer));
    SUCCESS_OR_TERMINATE(zexDriverReleaseImportedPointer(driverHandle, dstBuffer));
    delete[] dstBuffer;
    delete[] srcBuffer;
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Host Pointer";
    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);

    ze_context_handle_t context = {};
    ze_driver_handle_t driverHandle = {};
    auto devices = zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    bool outputValidationSuccessful;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    executeGpuKernelAndValidate(driverHandle, context, device, outputValidationSuccessful);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <iostream>

struct DevObjects {
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    void *srcBuffer;
    void *dstBuffer;
    uint8_t *readBackData;
};

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello P2P Copy";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    // Set-up
    size_t allocSize = 4096;
    if (LevelZeroBlackBoxTests::verbose) {
        allocSize = 8;
    }
    std::vector<DevObjects> devObjects;

    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    bool outputValidationSuccessful = false;

    uint32_t deviceCount = static_cast<uint32_t>(devices.size());

    // Resize arrays based on returned device count
    devObjects.resize(devices.size());

    for (uint32_t i = 0; i < deviceCount; i++) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        SUCCESS_OR_TERMINATE(zeDeviceGetProperties(devices[i], &deviceProperties));
        LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

        ze_device_p2p_properties_t deviceP2PProperties{};
        ze_device_p2p_bandwidth_exp_properties_t expP2Pproperties{};
        expP2Pproperties.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES;
        deviceP2PProperties.pNext = &expP2Pproperties;
        for (uint32_t j = 0; j < deviceCount; j++) {
            if (j == i)
                continue;
            SUCCESS_OR_TERMINATE(zeDeviceGetP2PProperties(devices[i], devices[j], &deviceP2PProperties));
            ze_bool_t canAccessPeer = false;
            SUCCESS_OR_TERMINATE(zeDeviceCanAccessPeer(devices[i], devices[j], &canAccessPeer));
            if (LevelZeroBlackBoxTests::verbose) {
                LevelZeroBlackBoxTests::printP2PProperties(deviceP2PProperties, canAccessPeer, i, j);
            }
            if (canAccessPeer == false) {
                std::cerr << "Device " << i << " cannot access " << j << "\n";
                std::terminate();
            }
        }
    }

    // Initialize objects for each device
    for (uint32_t i = 0; i < deviceCount; i++) {

        devObjects[i].readBackData = new uint8_t[allocSize]();

        devObjects[i].cmdQueue = LevelZeroBlackBoxTests::createCommandQueue(context, devices[i], nullptr, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, false);

        SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, devices[i], devObjects[i].cmdList, false));

        ze_device_mem_alloc_desc_t deviceDesc = {};
        deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
        deviceDesc.ordinal = i;
        deviceDesc.flags = 0;
        deviceDesc.pNext = nullptr;

        SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, 1, devices[i], &devObjects[i].srcBuffer));
        SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, 1, devices[i], &devObjects[i].dstBuffer));
    }

    for (uint32_t i = 0; i < deviceCount; i++) {
        uint32_t targetDev = 0;
        if (i == 0) {
            targetDev = deviceCount - 1;
        } else {
            targetDev = i - 1;
        }

        // Set the memory of a peer
        uint8_t value = i + 10;
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(
            devObjects[i].cmdList, devObjects[targetDev].dstBuffer, reinterpret_cast<void *>(&value),
            sizeof(value), allocSize, nullptr, 0, nullptr));

        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(devObjects[i].cmdList,
                                                        nullptr, 0, nullptr));

        // Copy the memory peer's to heap memory
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(
            devObjects[i].cmdList, devObjects[i].readBackData, devObjects[targetDev].dstBuffer,
            allocSize, nullptr, 0, nullptr));

        // Dispatch and wait
        SUCCESS_OR_TERMINATE(zeCommandListClose(devObjects[i].cmdList));
        SUCCESS_OR_TERMINATE(
            zeCommandQueueExecuteCommandLists(devObjects[i].cmdQueue, 1,
                                              &devObjects[i].cmdList, nullptr));
    }

    // Sync on host
    for (uint32_t i = 0; i < deviceCount; i++) {
        auto synchronizationResult = zeCommandQueueSynchronize(devObjects[i].cmdQueue, std::numeric_limits<uint64_t>::max());
        SUCCESS_OR_WARNING(synchronizationResult);
    }

    // validate and cleanup
    for (uint32_t i = 0; i < deviceCount; i++) {
        uint8_t value = i + 10;

        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(devObjects[i].cmdQueue));
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(devObjects[i].cmdList));
        SUCCESS_OR_TERMINATE(zeMemFree(context, devObjects[i].srcBuffer));
        SUCCESS_OR_TERMINATE(zeMemFree(context, devObjects[i].dstBuffer));

        // Validate
        outputValidationSuccessful = LevelZeroBlackBoxTests::validateToValue<uint8_t>(value, devObjects[i].readBackData, allocSize);
        delete[] devObjects[i].readBackData;
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

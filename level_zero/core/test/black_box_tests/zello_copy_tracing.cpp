/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zet_api.h>

#include "zello_common.h"

#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>

struct UserTracerData {
    uint32_t tracerData;
};

UserTracerData tracerData0 = {};

struct UserInstanceData {
    std::clock_t startTime;
    uint32_t allocCount;
};

uint32_t initCount;
uint32_t initPrologCount;
uint32_t initEpilogCount;

struct TmpInitParams {
    ze_init_flag_t flags;
};

TmpInitParams initParams;

void setInitParams(ze_init_flag_t flags) {
    initParams.flags = flags;
    initCount++;
}

void checkInitParams(ze_init_params_t *traceParams, TmpInitParams *checkParams) {
    SUCCESS_OR_WARNING_BOOL(*(traceParams->pflags) == checkParams->flags);
}

void onEnterInit(
    ze_init_params_t *tracerParams,
    ze_result_t result,
    void *traceUserData,
    void **tracerInstanceUserData) {

    checkInitParams(tracerParams, &initParams);
    UserInstanceData *instanceData = new UserInstanceData;
    instanceData->startTime = clock();
    instanceData->allocCount = initCount;
    *tracerInstanceUserData = reinterpret_cast<void *>(instanceData);
    initPrologCount++;
}

void onExitInit(
    ze_init_params_t *tracerParams,
    ze_result_t result,
    void *traceUserData,
    void **tracerInstanceUserData) {

    clock_t endTime = clock();
    SUCCESS_OR_WARNING_BOOL(result == ZE_RESULT_SUCCESS);
    UserInstanceData *instanceData = reinterpret_cast<UserInstanceData *>(*tracerInstanceUserData);
    SUCCESS_OR_WARNING_BOOL(instanceData->allocCount = initCount);
    float time = 1000.f * (endTime - instanceData->startTime) / CLOCKS_PER_SEC;
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "zeInit event " << instanceData->allocCount << " " << time << std::endl;
    }
    delete instanceData;
    checkInitParams(tracerParams, &initParams);
    initEpilogCount++;
}

uint32_t driverGetCount;
uint32_t driverGetPrologCount;
uint32_t driverGetEpilogCount;

struct TmpDriverGetParams {
    uint32_t *count;
    ze_driver_handle_t *drivers;
};

TmpDriverGetParams driverGetParams;

void setDriverGetParams(uint32_t *count, ze_driver_handle_t *drivers) {
    driverGetParams.count = count;
    driverGetParams.drivers = drivers;
    driverGetCount++;
}

void checkDriverGetParams(ze_driver_get_params_t *traceParams, TmpDriverGetParams *checkParams) {
    SUCCESS_OR_WARNING_BOOL(*(traceParams->ppCount) == checkParams->count);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->pphDrivers) == checkParams->drivers);
}

void onEnterDriverGet(
    ze_driver_get_params_t *tracerParams,
    ze_result_t result,
    void *traceUserData,
    void **tracerInstanceUserData) {

    checkDriverGetParams(tracerParams, &driverGetParams);
    UserInstanceData *instanceData = new UserInstanceData;
    instanceData->startTime = clock();
    instanceData->allocCount = initCount;
    *tracerInstanceUserData = reinterpret_cast<void *>(instanceData);
    driverGetPrologCount++;
}

void onExitDriverGet(
    ze_driver_get_params_t *tracerParams,
    ze_result_t result,
    void *traceUserData,
    void **tracerInstanceUserData) {

    clock_t endTime = clock();
    SUCCESS_OR_WARNING_BOOL(result == ZE_RESULT_SUCCESS);
    UserInstanceData *instanceData = reinterpret_cast<UserInstanceData *>(*tracerInstanceUserData);
    SUCCESS_OR_WARNING_BOOL(instanceData->allocCount = initCount);
    float time = 1000.f * (endTime - instanceData->startTime) / CLOCKS_PER_SEC;
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "zeDriverGet event " << instanceData->allocCount << " " << time << std::endl;
    }
    delete instanceData;
    checkDriverGetParams(tracerParams, &driverGetParams);
    driverGetEpilogCount++;
}

uint32_t memAllocDeviceCount;
uint32_t memAllocDevicePrologCount;
uint32_t memAllocDeviceEpilogCount;

struct TmpMemAllocDeviceParams {
    ze_context_handle_t context;
    ze_device_mem_alloc_desc_t *deviceDesc;
    size_t size;
    size_t alignment;
    ze_device_handle_t device;
    void *buffer;
};

TmpMemAllocDeviceParams allocMemDeviceParams;

void setMemAllocDeviceParams(ze_context_handle_t context, ze_device_mem_alloc_desc_t *deviceDesc,
                             size_t allocSize, size_t alignment, ze_device_handle_t device, void *buffer) {
    allocMemDeviceParams.context = context;
    allocMemDeviceParams.deviceDesc = deviceDesc;
    allocMemDeviceParams.size = allocSize;
    allocMemDeviceParams.alignment = alignment;
    allocMemDeviceParams.device = device;
    allocMemDeviceParams.buffer = buffer;
    memAllocDeviceCount++;
}

void checkMemAllocDeviceParams(ze_mem_alloc_device_params_t *traceParams, TmpMemAllocDeviceParams *checkParams) {
    SUCCESS_OR_WARNING_BOOL(*(traceParams->phContext) == checkParams->context);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->pdevice_desc) == checkParams->deviceDesc);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->psize) == checkParams->size);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->palignment) == checkParams->alignment);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->phDevice) == checkParams->device);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->ppptr) == checkParams->buffer);
}

void onEnterMemAllocDevice(
    ze_mem_alloc_device_params_t *tracerParams,
    ze_result_t result,
    void *traceUserData,
    void **tracerInstanceUserData) {

    checkMemAllocDeviceParams(tracerParams, &allocMemDeviceParams);
    UserInstanceData *instanceData = new UserInstanceData;
    instanceData->startTime = clock();
    instanceData->allocCount = memAllocDeviceCount;
    *tracerInstanceUserData = reinterpret_cast<void *>(instanceData);
    memAllocDevicePrologCount++;
}

void onExitMemAllocDevice(
    ze_mem_alloc_device_params_t *tracerParams,
    ze_result_t result,
    void *traceUserData,
    void **tracerInstanceUserData) {

    clock_t endTime = clock();
    SUCCESS_OR_WARNING_BOOL(result == ZE_RESULT_SUCCESS);
    UserInstanceData *instanceData = reinterpret_cast<UserInstanceData *>(*tracerInstanceUserData);
    SUCCESS_OR_WARNING_BOOL(instanceData->allocCount == memAllocDeviceCount);
    float time = 1000.f * (endTime - instanceData->startTime) / CLOCKS_PER_SEC;
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "zeDriverAllocDeviceMem event " << instanceData->allocCount << " " << time << std::endl;
    }
    delete instanceData;
    checkMemAllocDeviceParams(tracerParams, &allocMemDeviceParams);
    memAllocDeviceEpilogCount++;
}

uint32_t memAllocHostCount;
uint32_t memAllocHostPrologCount;
uint32_t memAllocHostEpilogCount;

struct TmpMemAllocHostParams {
    ze_context_handle_t context;
    ze_host_mem_alloc_desc_t *hostDesc;
    size_t size;
    size_t alignment;
    void *buffer;
};

TmpMemAllocHostParams memAllocHostParams;

void setMemAllocHostParams(ze_context_handle_t context, ze_host_mem_alloc_desc_t *hostDesc,
                           size_t allocSize, size_t alignment, void *buffer) {
    memAllocHostParams.context = context;
    memAllocHostParams.hostDesc = hostDesc;
    memAllocHostParams.size = allocSize;
    memAllocHostParams.alignment = alignment;
    memAllocHostParams.buffer = buffer;
    memAllocHostCount++;
}

void checkMemAllocHostParams(ze_mem_alloc_host_params_t *traceParams, TmpMemAllocHostParams *checkParams) {
    SUCCESS_OR_WARNING_BOOL(*(traceParams->phContext) == checkParams->context);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->phost_desc) == checkParams->hostDesc);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->psize) == checkParams->size);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->palignment) == checkParams->alignment);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->ppptr) == checkParams->buffer);
}

void onEnterMemAllocHost(
    ze_mem_alloc_host_params_t *tracerParams,
    ze_result_t result,
    void *traceUserData,
    void **tracerInstanceUserData) {

    checkMemAllocHostParams(tracerParams, &memAllocHostParams);
    UserInstanceData *instanceData = new UserInstanceData;
    instanceData->startTime = clock();
    instanceData->allocCount = memAllocHostCount;
    *tracerInstanceUserData = reinterpret_cast<void *>(instanceData);
    memAllocHostPrologCount++;
}

void onExitMemAllocHost(
    ze_mem_alloc_host_params_t *tracerParams,
    ze_result_t result,
    void *traceUserData,
    void **tracerInstanceUserData) {

    clock_t endTime = clock();
    SUCCESS_OR_WARNING_BOOL(result == ZE_RESULT_SUCCESS);
    UserInstanceData *instanceData = reinterpret_cast<UserInstanceData *>(*tracerInstanceUserData);
    SUCCESS_OR_WARNING_BOOL(instanceData->allocCount == memAllocHostCount);
    float time = 1000.f * (endTime - instanceData->startTime) / CLOCKS_PER_SEC;
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "zeMemAllocHost event " << instanceData->allocCount << " " << time << std::endl;
    }
    delete instanceData;
    checkMemAllocHostParams(tracerParams, &memAllocHostParams);
    memAllocHostEpilogCount++;
}

uint32_t memAllocSharedCount = 0;
uint32_t memAllocSharedPrologCount = 0;
uint32_t memAllocSharedEpilogCount = 0;

struct TmpMemAllocSharedParams {
    ze_context_handle_t context;
    ze_device_mem_alloc_desc_t *deviceDesc;
    ze_host_mem_alloc_desc_t *hostDesc;
    size_t size;
    size_t alignment;
    ze_device_handle_t device;
    void *buffer;
};

TmpMemAllocSharedParams memAllocSharedParams;

void setMemAllocSharedParams(ze_context_handle_t context, ze_device_mem_alloc_desc_t *deviceDesc,
                             ze_host_mem_alloc_desc_t *hostDesc, size_t allocSize, size_t alignment,
                             ze_device_handle_t device, void *buffer) {
    memAllocSharedParams.context = context;
    memAllocSharedParams.deviceDesc = deviceDesc;
    memAllocSharedParams.hostDesc = hostDesc;
    memAllocSharedParams.size = allocSize;
    memAllocSharedParams.alignment = alignment;
    memAllocSharedParams.device = device;
    memAllocSharedParams.buffer = buffer;
    memAllocSharedCount++;
}

void checkMemAllocShared(ze_mem_alloc_shared_params_t *traceParams, TmpMemAllocSharedParams *checkParams) {
    SUCCESS_OR_WARNING_BOOL(*(traceParams->phContext) == checkParams->context);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->pdevice_desc) == checkParams->deviceDesc);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->phost_desc) == checkParams->hostDesc);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->psize) == checkParams->size);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->palignment) == checkParams->alignment);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->phDevice) == checkParams->device);
    SUCCESS_OR_WARNING_BOOL(*(traceParams->ppptr) == checkParams->buffer);
}

void onEnterMemAllocShared(
    ze_mem_alloc_shared_params_t *tracerParams,
    ze_result_t result,
    void *traceUserData,
    void **tracerInstanceUserData) {

    checkMemAllocShared(tracerParams, &memAllocSharedParams);
    UserInstanceData *instanceData = new UserInstanceData;
    instanceData->startTime = clock();
    instanceData->allocCount = memAllocSharedCount;
    *tracerInstanceUserData = reinterpret_cast<void *>(instanceData);
    memAllocSharedPrologCount++;
}

void onExitMemAllocShared(
    ze_mem_alloc_shared_params_t *tracerParams,
    ze_result_t result,
    void *traceUserData,
    void **tracerInstanceUserData) {

    clock_t endTime = clock();
    SUCCESS_OR_WARNING_BOOL(result == ZE_RESULT_SUCCESS);
    UserInstanceData *instanceData = reinterpret_cast<UserInstanceData *>(*tracerInstanceUserData);
    SUCCESS_OR_WARNING_BOOL(instanceData->allocCount == memAllocSharedCount);
    float time = 1000.f * (endTime - instanceData->startTime) / CLOCKS_PER_SEC;
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "zeMemAllocShared event " << instanceData->allocCount << " " << time << std::endl;
    }
    delete instanceData;
    checkMemAllocShared(tracerParams, &memAllocSharedParams);
    memAllocSharedEpilogCount++;
}

void testAppendMemoryCopy0(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet,
                           ze_device_dditable_t &deviceDdiTable,
                           ze_command_queue_dditable_t cmdQueueDdiTable,
                           ze_command_list_dditable_t &cmdListDdiTable,
                           ze_mem_dditable_t &memDdiTable) {
    const size_t allocSize = 4096 + 7; // +7 to brake alignment and make it harder
    char *heapBuffer = new char[allocSize];
    void *zeBuffer = nullptr;
    char stackBuffer[allocSize];

    // Create command queue
    uint32_t numQueueGroups = 0;
    SUCCESS_OR_TERMINATE(deviceDdiTable.pfnGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
    if (numQueueGroups == 0) {
        std::cerr << "No queue groups found!\n";
        std::terminate();
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
    SUCCESS_OR_TERMINATE(deviceDdiTable.pfnGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                                          queueProperties.data()));

    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {
        ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        nullptr,
        0,
        0,
        ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY,
        ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            cmdQueueDesc.ordinal = i;
            break;
        }
    }
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_handle_t cmdList;
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC, nullptr};
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnCreate(context, device, &cmdListDesc, &cmdList));

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};

    setMemAllocDeviceParams(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer);
    SUCCESS_OR_TERMINATE(memDdiTable.pfnAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer, 0, allocSize);

    // Copy from heap to device-allocated memory
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnAppendMemoryCopy(cmdList, zeBuffer, heapBuffer, allocSize,
                                                             nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Copy from device-allocated memory to stack
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnAppendMemoryCopy(cmdList, stackBuffer, zeBuffer, allocSize,
                                                             nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnClose(cmdList));
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate stack and ze buffers have the original data from heapBuffer
    validRet = LevelZeroBlackBoxTests::validate(heapBuffer, stackBuffer, allocSize);

    delete[] heapBuffer;
    SUCCESS_OR_TERMINATE(memDdiTable.pfnFree(context, zeBuffer));
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnDestroy(cmdList));
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnDestroy(cmdQueue));
}

void testAppendMemoryCopy1(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet,
                           ze_device_dditable_t &deviceDdiTable,
                           ze_command_queue_dditable_t cmdQueueDdiTable,
                           ze_command_list_dditable_t cmdListDdiTable,
                           ze_mem_dditable_t &memDdiTable) {
    const size_t allocSize = 4096 + 7; // +7 to brake alignment and make it harder
    char *hostBuffer;
    void *zeBuffer = nullptr;
    char stackBuffer[allocSize];

    // Create command queue
    uint32_t numQueueGroups = 0;
    SUCCESS_OR_TERMINATE(deviceDdiTable.pfnGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
    if (numQueueGroups == 0) {
        std::cerr << "No queue groups found!\n";
        std::terminate();
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
    SUCCESS_OR_TERMINATE(deviceDdiTable.pfnGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                                          queueProperties.data()));

    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {
        ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        nullptr,
        0,
        0,
        ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY,
        ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            cmdQueueDesc.ordinal = i;
            break;
        }
    }
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_handle_t cmdList;
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC, nullptr};
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnCreate(context, device, &cmdListDesc, &cmdList));

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    setMemAllocHostParams(context, &hostDesc, allocSize, 1, (void **)(&hostBuffer));
    SUCCESS_OR_TERMINATE(memDdiTable.pfnAllocHost(context, &hostDesc, allocSize, 1, (void **)(&hostBuffer)));

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};

    setMemAllocDeviceParams(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer);
    SUCCESS_OR_TERMINATE(memDdiTable.pfnAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        hostBuffer[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer, 0, allocSize);

    // Copy from host-allocated to device-allocated memory
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnAppendMemoryCopy(cmdList, zeBuffer, hostBuffer, allocSize,
                                                             nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Copy from device-allocated memory to stack
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnAppendMemoryCopy(cmdList, stackBuffer, zeBuffer, allocSize,
                                                             nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnClose(cmdList));
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate stack and ze buffers have the original data from hostBuffer
    validRet = LevelZeroBlackBoxTests::validate(hostBuffer, stackBuffer, allocSize);

    SUCCESS_OR_TERMINATE(memDdiTable.pfnFree(context, hostBuffer));
    SUCCESS_OR_TERMINATE(memDdiTable.pfnFree(context, zeBuffer));
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnDestroy(cmdList));
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnDestroy(cmdQueue));
}

void testAppendMemoryCopy2(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet,
                           ze_device_dditable_t &deviceDdiTable,
                           ze_command_queue_dditable_t cmdQueueDdiTable,
                           ze_command_list_dditable_t cmdListDdiTable,
                           ze_mem_dditable_t &memDdiTable) {
    validRet = true;

    // Create command queue
    uint32_t numQueueGroups = 0;
    SUCCESS_OR_TERMINATE(deviceDdiTable.pfnGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
    if (numQueueGroups == 0) {
        std::cerr << "No queue groups found!\n";
        std::terminate();
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
    SUCCESS_OR_TERMINATE(deviceDdiTable.pfnGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                                          queueProperties.data()));

    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {
        ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        nullptr,
        0,
        0,
        ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY,
        ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            cmdQueueDesc.ordinal = i;
            break;
        }
    }
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_handle_t cmdList;
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC, nullptr};
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnCreate(context, device, &cmdListDesc, &cmdList));

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

    ze_device_mem_alloc_desc_t deviceDesc0 = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ze_host_mem_alloc_desc_t hostDesc0 = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    setMemAllocSharedParams(context, &deviceDesc0, &hostDesc0, srcSize, 1, device, &srcBuffer);
    SUCCESS_OR_TERMINATE(memDdiTable.pfnAllocShared(context, &deviceDesc0, &hostDesc0, srcSize, 1, device, &srcBuffer));

    ze_device_mem_alloc_desc_t deviceDesc1 = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ze_host_mem_alloc_desc_t hostDesc1 = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    setMemAllocSharedParams(context, &deviceDesc1, &hostDesc1, dstSize, 1, device, &dstBuffer);
    SUCCESS_OR_TERMINATE(memDdiTable.pfnAllocShared(context, &deviceDesc1, &hostDesc1, dstSize, 1, device, &dstBuffer));

    // Initialize buffers
    uint8_t *stackBuffer = new uint8_t[srcSize];
    for (uint32_t i = 0; i < srcHeight; i++) {
        for (uint32_t j = 0; j < srcWidth; j++) {
            stackBuffer[i * srcWidth + j] = static_cast<uint8_t>(i * srcWidth + j);
        }
    }
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnAppendMemoryCopy(cmdList, srcBuffer, stackBuffer, srcSize,
                                                             nullptr, 0, nullptr));

    int value = 0;
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnAppendMemoryFill(cmdList, dstBuffer, reinterpret_cast<void *>(&value),
                                                             sizeof(value), dstSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Perform the copy
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnAppendMemoryCopyRegion(cmdList, dstBuffer, &dstRegion, dstWidth, 0,
                                                                   srcBuffer, &srcRegion, srcWidth, 0,
                                                                   nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnClose(cmdList));
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    uint8_t *dstBufferChar = reinterpret_cast<uint8_t *>(dstBuffer);
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "stackBuffer\n";
        for (uint32_t i = 0; i < srcHeight; i++) {
            for (uint32_t j = 0; j < srcWidth; j++) {
                std::cout << std::setw(3) << std::dec << static_cast<unsigned int>(stackBuffer[i * srcWidth + j]) << " ";
            }
            std::cout << "\n";
        }

        std::cout << "dstBuffer\n";
        for (uint32_t i = 0; i < dstHeight; i++) {
            for (uint32_t j = 0; j < dstWidth; j++) {
                std::cout << std::setw(3) << std::dec << static_cast<unsigned int>(dstBufferChar[i * dstWidth + j]) << " ";
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
    SUCCESS_OR_TERMINATE(memDdiTable.pfnFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(memDdiTable.pfnFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(cmdListDdiTable.pfnDestroy(cmdList));
    SUCCESS_OR_TERMINATE(cmdQueueDdiTable.pfnDestroy(cmdQueue));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Copy Tracing";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    LevelZeroBlackBoxTests::setEnvironmentVariable("ZET_ENABLE_API_TRACING_EXP", "1");

    ze_api_version_t apiVersion = ZE_API_VERSION_CURRENT;

    ze_global_dditable_t globalDdiTable;
    SUCCESS_OR_TERMINATE(zeGetGlobalProcAddrTable(apiVersion, &globalDdiTable));

    ze_driver_dditable_t driverDdiTable;
    SUCCESS_OR_TERMINATE(zeGetDriverProcAddrTable(apiVersion, &driverDdiTable));

    ze_device_dditable_t deviceDdiTable;
    SUCCESS_OR_TERMINATE(zeGetDeviceProcAddrTable(apiVersion, &deviceDdiTable));

    ze_context_dditable_t contextDdiTable;
    SUCCESS_OR_TERMINATE(zeGetContextProcAddrTable(apiVersion, &contextDdiTable));

    ze_command_queue_dditable_t cmdQueueDdiTable;
    SUCCESS_OR_TERMINATE(zeGetCommandQueueProcAddrTable(apiVersion, &cmdQueueDdiTable));

    ze_command_list_dditable_t cmdListDdiTable;
    SUCCESS_OR_TERMINATE(zeGetCommandListProcAddrTable(apiVersion, &cmdListDdiTable));

    ze_mem_dditable_t memDdiTable;
    SUCCESS_OR_TERMINATE(zeGetMemProcAddrTable(apiVersion, &memDdiTable));

    SUCCESS_OR_TERMINATE(globalDdiTable.pfnInit(ZE_INIT_FLAG_GPU_ONLY));

    uint32_t driverCount = 0;
    SUCCESS_OR_TERMINATE(driverDdiTable.pfnGet(&driverCount, nullptr));
    if (driverCount == 0) {
        std::cerr << "No driver handle found!" << std::endl;
        std::terminate();
    }

    ze_driver_handle_t driver;
    driverCount = 1;
    SUCCESS_OR_TERMINATE(driverDdiTable.pfnGet(&driverCount, &driver));

    uint32_t deviceCount = 0;
    SUCCESS_OR_TERMINATE(deviceDdiTable.pfnGet(driver, &deviceCount, nullptr));
    if (deviceCount == 0) {
        std::cerr << "No device found!" << std::endl;
        std::terminate();
    }

    ze_device_handle_t device;
    deviceCount = 1;
    SUCCESS_OR_TERMINATE(deviceDdiTable.pfnGet(driver, &deviceCount, &device));

    ze_context_handle_t context;
    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    SUCCESS_OR_TERMINATE(contextDdiTable.pfnCreate(driver, &contextDesc, &context));

    zet_tracer_exp_desc_t tracerDesc = {ZET_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, &tracerData0};
    zet_tracer_exp_handle_t tracer;
    SUCCESS_OR_TERMINATE(zetTracerExpCreate(context, &tracerDesc, &tracer));

    ze_callbacks_t prologCbs = {};
    prologCbs.Global.pfnInitCb = onEnterInit;
    prologCbs.Driver.pfnGetCb = onEnterDriverGet;
    prologCbs.Mem.pfnAllocDeviceCb = onEnterMemAllocDevice;
    prologCbs.Mem.pfnAllocHostCb = onEnterMemAllocHost;
    prologCbs.Mem.pfnAllocSharedCb = onEnterMemAllocShared;
    SUCCESS_OR_TERMINATE(zetTracerExpSetPrologues(tracer, &prologCbs));

    ze_callbacks_t epilogCbs = {};
    epilogCbs.Global.pfnInitCb = onExitInit;
    epilogCbs.Driver.pfnGetCb = onExitDriverGet;
    epilogCbs.Mem.pfnAllocDeviceCb = onExitMemAllocDevice;
    epilogCbs.Mem.pfnAllocHostCb = onExitMemAllocHost;
    epilogCbs.Mem.pfnAllocSharedCb = onExitMemAllocShared;
    SUCCESS_OR_TERMINATE(zetTracerExpSetEpilogues(tracer, &epilogCbs));

    SUCCESS_OR_TERMINATE(zetTracerExpSetEnabled(tracer, true));

    setInitParams(ZE_INIT_FLAG_GPU_ONLY);
    SUCCESS_OR_TERMINATE(globalDdiTable.pfnInit(ZE_INIT_FLAG_GPU_ONLY));

    ze_driver_handle_t driverTest;
    setDriverGetParams(&driverCount, &driverTest);
    SUCCESS_OR_TERMINATE(driverDdiTable.pfnGet(&driverCount, &driverTest));

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(deviceDdiTable.pfnGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    bool outputValidationSuccessful;
    testAppendMemoryCopy0(context, device, outputValidationSuccessful,
                          deviceDdiTable, cmdQueueDdiTable, cmdListDdiTable, memDdiTable);
    if (outputValidationSuccessful || aubMode) {
        testAppendMemoryCopy1(context, device, outputValidationSuccessful,
                              deviceDdiTable, cmdQueueDdiTable, cmdListDdiTable, memDdiTable);
    }
    if (outputValidationSuccessful || aubMode) {
        testAppendMemoryCopy2(context, device, outputValidationSuccessful,
                              deviceDdiTable, cmdQueueDdiTable, cmdListDdiTable, memDdiTable);
    }

    /* tear down tracing environemt and test epilog/prolg counts */
    SUCCESS_OR_TERMINATE(zetTracerExpSetEnabled(tracer, false));
    SUCCESS_OR_TERMINATE(zetTracerExpDestroy(tracer));

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "initCount: " << initCount
                  << " initPrologCount: " << initPrologCount
                  << " initEpilogCount: " << initEpilogCount
                  << std::endl;

        std::cout << "driverGetCount: " << driverGetCount
                  << " driverGetPrologCount: " << driverGetPrologCount
                  << " driverGetEpilogCount: " << driverGetEpilogCount
                  << std::endl;

        std::cout << "memAllocDeviceCount: " << memAllocDeviceCount
                  << " memAllocDevicePrologCount: " << memAllocDevicePrologCount
                  << " memAllocDeviceEpilogCount: " << memAllocDeviceEpilogCount
                  << std::endl;

        std::cout << "memAllocHostCount: " << memAllocHostCount
                  << " memAllocHostPrologCount: " << memAllocHostPrologCount
                  << " memAllocHostEpilogCount: " << memAllocHostEpilogCount
                  << std::endl;

        std::cout << "memAllocSharedCount: " << memAllocSharedCount
                  << " memAllocSharedPrologCount: " << memAllocSharedPrologCount
                  << " memAllocSharedEpilogCount: " << memAllocSharedEpilogCount
                  << std::endl;
    }

    SUCCESS_OR_TERMINATE_BOOL((initCount == initPrologCount) &&
                              (initCount == initEpilogCount));
    SUCCESS_OR_TERMINATE_BOOL((driverGetCount == driverGetPrologCount) &&
                              (driverGetCount == driverGetEpilogCount));
    SUCCESS_OR_TERMINATE_BOOL((memAllocDeviceCount == memAllocDevicePrologCount) &&
                              (memAllocDeviceCount == memAllocDeviceEpilogCount));
    SUCCESS_OR_TERMINATE_BOOL((memAllocHostCount == memAllocHostPrologCount) &&
                              (memAllocHostCount == memAllocHostEpilogCount));
    SUCCESS_OR_TERMINATE_BOOL((memAllocSharedCount == memAllocSharedPrologCount) &&
                              (memAllocSharedCount == memAllocSharedEpilogCount));

    SUCCESS_OR_TERMINATE(contextDdiTable.pfnDestroy(context));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);

    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

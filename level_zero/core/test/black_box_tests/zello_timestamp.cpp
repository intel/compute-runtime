/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <functional>
#include <map>

inline std::vector<uint8_t> loadBinaryFile(const std::string &filePath) {
    std::ifstream stream(filePath, std::ios::binary);
    if (!stream.good()) {
        std::cerr << "Failed to load binary file: " << filePath << " " << strerror(errno) << "\n";
        return {};
    }

    stream.seekg(0, stream.end);
    const size_t length = static_cast<size_t>(stream.tellg());
    stream.seekg(0, stream.beg);

    std::vector<uint8_t> binaryFile(length);
    stream.read(reinterpret_cast<char *>(binaryFile.data()), length);
    return binaryFile;
}

void createCmdQueueAndCmdList(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_command_queue_handle_t &cmdQueue,
                              ze_command_list_handle_t &cmdList) {
    // Create command queue
    uint32_t numQueueGroups = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
    if (numQueueGroups == 0) {
        std::cout << "No queue groups found!\n";
        std::terminate();
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
    SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                                queueProperties.data()));

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            cmdQueueDesc.ordinal = i;
            break;
        }
    }
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    // Create command list
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.commandQueueGroupOrdinal = cmdQueueDesc.ordinal;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));
}

bool testWriteGlobalTimestamp(int argc, char *argv[],
                              ze_context_handle_t &context,
                              ze_driver_handle_t &driver,
                              ze_device_handle_t &device) {
    constexpr size_t allocSize = 4096;
    constexpr size_t tsAllocSize = 64;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    uint64_t tsStartResult = 0, tsEndResult = 0;

    void *dstBuffer;
    void *globalTsStart, *globalTsEnd;

    // Create commandQueue and cmdList
    createCmdQueueAndCmdList(context, device, cmdQueue, cmdList);

    // Alloc buffers
    dstBuffer = nullptr;
    globalTsStart = nullptr;
    globalTsEnd = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, 1, device, &dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, tsAllocSize, 1, device, &globalTsStart));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, tsAllocSize, 1, device, &globalTsEnd));

    // Init data and copy to device
    uint8_t initDataDst[allocSize];
    memset(initDataDst, 3, sizeof(initDataDst));

    SUCCESS_OR_TERMINATE(zeCommandListAppendWriteGlobalTimestamp(cmdList, (uint64_t *)globalTsStart, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, initDataDst,
                                                       sizeof(initDataDst), nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendWriteGlobalTimestamp(cmdList, (uint64_t *)globalTsEnd, nullptr, 0, nullptr));

    // Copy back timestamp data
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, &tsStartResult, globalTsStart, sizeof(tsStartResult),
                                                       nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, &tsEndResult, globalTsEnd, sizeof(tsEndResult),
                                                       nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    ze_device_properties_t devProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &devProperties));

    uint64_t commandDuration = tsEndResult - tsStartResult;
    uint64_t timerResolution = devProperties.timerResolution;
    std::cout << "Global timestamp statistics: \n"
              << std::fixed
              << " Command start : " << std::dec << tsStartResult << " cycles\n"
              << " Command end : " << std::dec << tsEndResult << " cycles\n"
              << " Command duration : " << std::dec << commandDuration << " cycles, " << commandDuration * timerResolution << " ns\n";

    // Tear down

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, globalTsStart));
    SUCCESS_OR_TERMINATE(zeMemFree(context, globalTsEnd));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    return true;
}

bool testKernelTimestampHostQuery(int argc, char *argv[],
                                  ze_context_handle_t &context,
                                  ze_driver_handle_t &driver,
                                  ze_device_handle_t &device) {

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    // Create commandQueue and cmdList
    createCmdQueueAndCmdList(context, device, cmdQueue, cmdList);

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

    // Create kernel
    auto spirvModule = loadBinaryFile("copy_buffer_to_buffer.spv");
    if (spirvModule.size() == 0) {
        return false;
    }

    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(spirvModule.data());
    moduleDesc.inputSize = spirvModule.size();
    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));

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

    ze_event_pool_handle_t eventPool;
    ze_event_handle_t kernelTsEvent;
    createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, 1, &kernelTsEvent, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits, kernelTsEvent, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    ze_kernel_timestamp_result_t kernelTsResults;
    SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(kernelTsEvent, &kernelTsResults));

    ze_device_properties_t devProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &devProperties));

    uint64_t timerResolution = devProperties.timerResolution;
    uint64_t kernelDuration = kernelTsResults.context.kernelEnd - kernelTsResults.context.kernelStart;
    std::cout << "Kernel timestamp statistics: \n"
              << std::fixed
              << " Global start : " << std::dec << kernelTsResults.global.kernelStart << " cycles\n"
              << " Kernel start: " << std::dec << kernelTsResults.context.kernelStart << " cycles\n"
              << " Kernel end: " << std::dec << kernelTsResults.context.kernelEnd << " cycles\n"
              << " Global end: " << std::dec << kernelTsResults.global.kernelEnd << " cycles\n"
              << " timerResolution clock: " << std::dec << timerResolution << " cycles/s\n"
              << " Kernel duration : " << std::dec << kernelDuration << " cycles, " << kernelDuration * (1000000000.0 / static_cast<double>(timerResolution)) << " ns\n";

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeEventDestroy(kernelTsEvent));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    return true;
}

bool testKernelTimestampAppendQuery(ze_context_handle_t &context,
                                    ze_device_handle_t &device,
                                    ze_device_properties_t devProperties) {

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    // Create commandQueue and cmdList
    createCmdQueueAndCmdList(context, device, cmdQueue, cmdList);

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

    void *timestampBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, sizeof(ze_kernel_timestamp_result_t), 1, &timestampBuffer));

    // Initialize memory
    constexpr uint8_t val = 55;
    memset(srcBuffer, val, allocSize);
    memset(dstBuffer, 0, allocSize);
    memset(timestampBuffer, 0, sizeof(ze_kernel_timestamp_result_t));

    // Create kernel
    auto spirvModule = loadBinaryFile("copy_buffer_to_buffer.spv");
    if (spirvModule.size() == 0) {
        return false;
    }

    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(spirvModule.data());
    moduleDesc.inputSize = spirvModule.size();
    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));

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

    ze_event_pool_handle_t eventPool;
    ze_event_handle_t kernelTsEvent;
    createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, 1, &kernelTsEvent, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits, kernelTsEvent, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0u, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendQueryKernelTimestamps(cmdList, 1u, &kernelTsEvent, timestampBuffer, nullptr, nullptr, 0u, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    ze_kernel_timestamp_result_t *kernelTsResults = reinterpret_cast<ze_kernel_timestamp_result_t *>(timestampBuffer);

    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &devProperties));

    uint64_t timerResolution = devProperties.timerResolution;
    uint64_t kernelDuration = kernelTsResults->context.kernelEnd - kernelTsResults->context.kernelStart;
    if (devProperties.stype == ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2) {
        std::cout << "Kernel timestamp statistics (V1.2 and later): \n"
                  << std::fixed
                  << " Global start : " << std::dec << kernelTsResults->global.kernelStart << " cycles\n"
                  << " Kernel start: " << std::dec << kernelTsResults->context.kernelStart << " cycles\n"
                  << " Kernel end: " << std::dec << kernelTsResults->context.kernelEnd << " cycles\n"
                  << " Global end: " << std::dec << kernelTsResults->global.kernelEnd << " cycles\n"
                  << " timerResolution clock: " << std::dec << timerResolution << " cycles/s\n"
                  << " Kernel duration : " << std::dec << kernelDuration << " cycles, " << kernelDuration * (1000000000.0 / static_cast<double>(timerResolution)) << " ns\n";
    } else {
        std::cout << "Kernel timestamp statistics (prior to V1.2): \n"
                  << std::fixed
                  << " Global start : " << std::dec << kernelTsResults->global.kernelStart << " cycles\n"
                  << " Kernel start: " << std::dec << kernelTsResults->context.kernelStart << " cycles\n"
                  << " Kernel end: " << std::dec << kernelTsResults->context.kernelEnd << " cycles\n"
                  << " Global end: " << std::dec << kernelTsResults->global.kernelEnd << " cycles\n"
                  << " timerResolution: " << std::dec << timerResolution << " ns\n"
                  << " Kernel duration : " << std::dec << kernelDuration << " cycles, " << kernelDuration * timerResolution << " ns\n";
    }
    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, timestampBuffer));
    SUCCESS_OR_TERMINATE(zeEventDestroy(kernelTsEvent));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    return true;
}

bool testKernelTimestampAppendQueryWithDeviceProperties(int argc, char *argv[],
                                                        ze_context_handle_t &context,
                                                        ze_driver_handle_t &driver,
                                                        ze_device_handle_t &device) {
    bool result;
    std::string currentTest;
    bool aubMode = isAubMode(argc, argv);

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    currentTest = "Test Append Write of Global Timestamp: Default Device Properties Structure";
    deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    result = testKernelTimestampAppendQuery(context, device, deviceProperties);

    if (result || aubMode) {
        currentTest = "Test Append Write of Global Timestamp: V1.2 (and later) Device Properties Structure";
        deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2};
        result = testKernelTimestampAppendQuery(context, device, deviceProperties);
    }

    return result;
}

bool testKernelTimestampMapToHostTimescale(int argc, char *argv[],
                                           ze_context_handle_t &context,
                                           ze_driver_handle_t &driver,
                                           ze_device_handle_t &device) {

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;
    void *timestampBuffer = nullptr;
    ze_event_pool_handle_t eventPool;
    ze_event_handle_t kernelTsEvent;

    bool runTillDeviceTsOverflows = isParamEnabled(argc, argv, "-o", "--runTillOverflow");

    // Create commandQueue and cmdList
    createCmdQueueAndCmdList(context, device, cmdQueue, cmdList);

    auto appendKernel = [&module, &kernel, &srcBuffer, &dstBuffer, &timestampBuffer,
                         &context, &device, &eventPool, &kernelTsEvent](ze_command_list_handle_t &cmdList) {
        // Create two shared buffers
        constexpr size_t allocSize = 4096 * 51200;
        ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
        deviceDesc.ordinal = 0;

        ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
        hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

        SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &srcBuffer));
        SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &dstBuffer));
        SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, sizeof(ze_kernel_timestamp_result_t), 1, &timestampBuffer));

        // Initialize memory
        constexpr uint8_t val = 55;
        memset(srcBuffer, val, allocSize);
        memset(dstBuffer, 0, allocSize);
        memset(timestampBuffer, 0, sizeof(ze_kernel_timestamp_result_t));

        // Create kernel
        auto spirvModule = loadBinaryFile("copy_buffer_to_buffer.spv");
        if (spirvModule.size() == 0) {
            return false;
        }

        ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(spirvModule.data());
        moduleDesc.inputSize = spirvModule.size();
        SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));

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

        createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, 1, &kernelTsEvent, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits, kernelTsEvent, 0, nullptr));

        return true;
    };

    appendKernel(cmdList);
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0u, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendQueryKernelTimestamps(cmdList, 1u, &kernelTsEvent, timestampBuffer, nullptr, nullptr, 0u, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    ze_device_properties_t devProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &devProperties));

    const uint64_t kernelTimestampMaxValueInCycles = (1ull << devProperties.kernelTimestampValidBits) - 1;
    const uint64_t timestampMaxValueInCycles = (1ull << devProperties.timestampValidBits) - 1;
    std::cout << "KernelTimestampMaxValueInCycles: " << kernelTimestampMaxValueInCycles << " | validBits : " << devProperties.kernelTimestampValidBits << "\n";
    std::cout << "TimestampMaxValueInCycles: " << timestampMaxValueInCycles << " | validBits : " << devProperties.timestampValidBits << "\n";
    std::cout << "timerResolution: " << devProperties.timerResolution << "\n";

    auto convertDeviceTsToNanoseconds = [&devProperties, &kernelTimestampMaxValueInCycles](uint64_t deviceTs) {
        return static_cast<uint64_t>((deviceTs & kernelTimestampMaxValueInCycles) * devProperties.timerResolution);
    };

    auto getDuration = [](uint64_t startTs, uint64_t endTs, uint32_t timestampValidBits) {
        const uint64_t maxValue = (1ull << timestampValidBits) - 1;

        if (startTs > endTs) {
            // Resolve overflows
            return endTs + (maxValue - startTs);
        } else {
            return endTs - startTs;
        }
    };

    uint64_t unusedHostTs, referenceDeviceTs;

    SUCCESS_OR_TERMINATE(zeDeviceGetGlobalTimestamps(device, &unusedHostTs, &referenceDeviceTs));
    std::cout << "ReferenceDeviceTs: " << referenceDeviceTs << "\n";

    for (uint32_t i = 0, iter = 0; i < 10; i++) {
        uint64_t submitHostTs, submitDeviceTs;
        // Get host and device timestamp before submission
        SUCCESS_OR_TERMINATE(zeDeviceGetGlobalTimestamps(device, &submitHostTs, &submitDeviceTs));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
        ze_kernel_timestamp_result_t *kernelTsResults = reinterpret_cast<ze_kernel_timestamp_result_t *>(timestampBuffer);

        // High Level Approach:
        // startTimeStamp = (submitHostTs - submitDeviceTs) + kernelDeviceTsStart
        // deviceDuration = kernelDeviceTsEnd - kernelDeviceTsStart
        // endTimeStamp = startTimeStamp + deviceDuration

        // Get offset between Device and Host timestamps
        int64_t tsOffset = submitHostTs - convertDeviceTsToNanoseconds(submitDeviceTs);

        // Add the offset to the kernel timestamp to find the start timestamp on the CPU timescale
        uint64_t startTimeStamp = static_cast<uint64_t>(convertDeviceTsToNanoseconds(kernelTsResults->global.kernelStart) + tsOffset);
        if (startTimeStamp < submitHostTs) {
            tsOffset += static_cast<uint64_t>(convertDeviceTsToNanoseconds(kernelTimestampMaxValueInCycles));
            startTimeStamp = static_cast<uint64_t>(convertDeviceTsToNanoseconds(kernelTsResults->global.kernelStart) + tsOffset);
        }

        // Get the kernel timestamp duration
        uint64_t deviceDuration = getDuration(kernelTsResults->global.kernelStart, kernelTsResults->global.kernelEnd, devProperties.kernelTimestampValidBits);
        uint64_t deviceDurationNs = static_cast<uint64_t>(convertDeviceTsToNanoseconds(deviceDuration));

        // Add the duration to the startTimeStamp to get the endTimeStamp
        uint64_t endTimeStamp = startTimeStamp + deviceDurationNs;
        std::cout << "[" << iter << "]";
        std::cout << " | kernel[start,end]: [" << kernelTsResults->global.kernelStart << ", " << kernelTsResults->global.kernelEnd << "]";
        std::cout << " | submit[host,device]: [" << submitHostTs << ", " << submitDeviceTs << "]";
        std::cout << " | deviceTsOnHostTimescale[start, end] : [" << startTimeStamp << ", " << endTimeStamp << " ] \n";
        ++iter;
        if (runTillDeviceTsOverflows) {
            i = 0;
            if (referenceDeviceTs > submitDeviceTs) {
                runTillDeviceTsOverflows = false;
            }
        }
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, timestampBuffer));
    SUCCESS_OR_TERMINATE(zeEventDestroy(kernelTsEvent));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    return true;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName("Zello Timestamp");
    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);
    using testFunction = std::function<bool(int, char *[],
                                            ze_context_handle_t &,
                                            ze_driver_handle_t &,
                                            ze_device_handle_t &)>;

    std::map<std::string, testFunction> supportedTests{};
    supportedTests["testKernelTimestampMapToHostTimescale"] = testKernelTimestampMapToHostTimescale;
    supportedTests["testKernelTimestampAppendQueryWithDeviceProperties"] = testKernelTimestampAppendQueryWithDeviceProperties;
    supportedTests["testWriteGlobalTimestamp"] = testWriteGlobalTimestamp;
    supportedTests["testKernelTimestampHostQuery"] = testKernelTimestampHostQuery;

    const char *defaultString = "testKernelTimestampAppendQueryWithDeviceProperties";
    const char *test = getParamValue(argc, argv, "-t", "--test", defaultString);
    bool result = false;
    if (supportedTests.find(test) != supportedTests.end()) {
        ze_context_handle_t context = nullptr;
        ze_driver_handle_t driverHandle = nullptr;
        auto devices = zelloInitContextAndGetDevices(context, driverHandle);
        auto device = devices[0];

        result = supportedTests[test](argc, argv, context, driverHandle, device);
        SUCCESS_OR_TERMINATE(zeContextDestroy(context));
        printResult(aubMode, result, blackBoxName, test);
    }
    result = aubMode ? true : result;
    return result ? 0 : 1;
}

/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>

static bool inverseOrder = false;

void executeKernelAndValidate(ze_context_handle_t &context, ze_device_handle_t &device, bool &outputValidationSuccessful) {
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_command_list_handle_t cmdList1;
    ze_command_list_handle_t cmdList2;

    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList1));
    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList2));

    constexpr size_t allocSize = 4096;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 4096, 0, &dstBuffer));

    memset(dstBuffer, 0, allocSize);

    std::string buildLog;
    auto spirV = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::atomicIncSrc, "", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_build_log_handle_t buildlog;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = spirV.data();
    moduleDesc.inputSize = spirV.size();
    moduleDesc.pBuildFlags = "";

    if (zeModuleCreate(context, device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
        size_t szLog = 0;
        zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

        char *strLog = (char *)malloc(szLog);
        zeModuleBuildLogGetString(buildlog, &szLog, strLog);
        LevelZeroBlackBoxTests::printBuildLog(strLog);

        free(strLog);
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
        std::cerr << "\nModule creation error." << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }

    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "testKernel";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
    ze_kernel_properties_t kernProps{ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernProps));
    LevelZeroBlackBoxTests::printKernelProperties(kernProps, kernelDesc.pKernelName);

    uint32_t groupSizeX = 1u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, 1U, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));
    std::cout << " groupSizeX: " << groupSizeX << " groupSizeY: " << groupSizeY << " groupSizeZ: " << groupSizeZ << std::endl;

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    // Create Event Pool and kernel launch event
    ze_event_pool_handle_t eventPool;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> events(numEvents);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool,
                                                     ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, false, nullptr, nullptr,
                                                     numEvents, events.data(),
                                                     ZE_EVENT_SCOPE_FLAG_DEVICE,
                                                     0);

    auto buffer = reinterpret_cast<uint32_t *>(dstBuffer);
    auto offsetBuffer = &(buffer[1]);

    int loopIters = 2;
    int i = inverseOrder ? 1 : 0;
    do {
        if (i == 0) {
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
            SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList1, kernel, &dispatchTraits,
                                                                 events[0], 0, nullptr));
        }

        if (i == 1) {
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &offsetBuffer));
            SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList2, kernel, &dispatchTraits,
                                                                 events[1], 1, &events[0]));
        }

        i = inverseOrder ? i - 1 : i + 1;
    } while (loopIters--);

    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList1, std::numeric_limits<uint64_t>::max()));
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList2, std::numeric_limits<uint64_t>::max()));

    // Validate
    outputValidationSuccessful = true;

    if (buffer[0] != 1) {
        outputValidationSuccessful = false;
        std::cout << "dstBuffer[0] = " << std::dec << buffer[0] << "\n";
    }

    if (buffer[1] != 1) {
        outputValidationSuccessful = false;
        std::cout << "dstBuffer[1] = " << std::dec << buffer[0] << "\n";
    }

    for (uint32_t i = 0; i < numEvents; i++) {
        ze_kernel_timestamp_result_t kernelTsResults;
        SUCCESS_OR_TERMINATE(zeEventQueryKernelTimestamp(events[i], &kernelTsResults));

        uint64_t kernelDuration = kernelTsResults.context.kernelEnd - kernelTsResults.context.kernelStart;
        std::cout << "\nKernel timestamps for event[" << i << "] \n"
                  << std::fixed
                  << " Global start : " << std::dec << kernelTsResults.global.kernelStart << "\n"
                  << " Kernel start: " << std::dec << kernelTsResults.context.kernelStart << "\n"
                  << " Kernel end: " << std::dec << kernelTsResults.context.kernelEnd << "\n"
                  << " Global end: " << std::dec << kernelTsResults.global.kernelEnd << "\n"
                  << " Kernel duration : " << std::dec << kernelDuration << " cycles\n";
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList2));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Atomic Inc";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    bool outputValidationSuccessful = false;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    inverseOrder = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "", "--inverse");

    executeKernelAndValidate(context, device, outputValidationSuccessful);

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

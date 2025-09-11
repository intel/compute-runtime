/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <iostream>

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Dynamic Link";
    bool outputValidationSuccessful = true;
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    bool circularDep = LevelZeroBlackBoxTests::isCircularDepTest(argc, argv);
    int numModules = 2;

    char *exportModuleSrcValue = const_cast<char *>(LevelZeroBlackBoxTests::DynamicLink::exportModuleSrc);
    char *importModuleSrcValue = const_cast<char *>(LevelZeroBlackBoxTests::DynamicLink::importModuleSrc);
    ze_module_handle_t exportModule2 = {};
    if (circularDep) {
        exportModuleSrcValue = const_cast<char *>(LevelZeroBlackBoxTests::DynamicLink::exportModuleSrcCircDep);
        importModuleSrcValue = const_cast<char *>(LevelZeroBlackBoxTests::DynamicLink::importModuleSrcCircDep);
        numModules = 3;
    }
    // Setup
    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.pNext = nullptr;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;

    void *resultBuffer;

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         sizeof(int), 1, device, &resultBuffer));

    // Build Import/Export SPIRVs & Modules

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "reading export module for spirv\n";
    }
    std::string buildLog;
    auto exportBinaryModule = LevelZeroBlackBoxTests::compileToSpirV(exportModuleSrcValue, "", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == exportBinaryModule.size()));

    ze_module_handle_t exportModule;
    ze_module_desc_t exportModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    exportModuleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    exportModuleDesc.pInputModule = reinterpret_cast<const uint8_t *>(exportBinaryModule.data());
    exportModuleDesc.inputSize = exportBinaryModule.size();

    // -library-compliation is required for the non-kernel functions to be listed as exported by the Intel Graphics Compiler
    exportModuleDesc.pBuildFlags = "-library-compilation";

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "building export module\n";
    }

    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &exportModuleDesc, &exportModule, nullptr));

    if (circularDep) {
        if (LevelZeroBlackBoxTests::verbose) {
            std::cout << "reading export module2 for spirv\n";
        }
        auto exportBinaryModule2 = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::DynamicLink::exportModuleSrc2CircDep, "", buildLog);
        LevelZeroBlackBoxTests::printBuildLog(buildLog);
        SUCCESS_OR_TERMINATE((0 == exportBinaryModule2.size()));

        ze_module_desc_t exportModuleDesc2 = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        exportModuleDesc2.format = ZE_MODULE_FORMAT_IL_SPIRV;
        exportModuleDesc2.pInputModule = reinterpret_cast<const uint8_t *>(exportBinaryModule2.data());
        exportModuleDesc2.inputSize = exportBinaryModule2.size();

        // -library-compliation is required for the non-kernel functions to be listed as exported by the Intel Graphics Compiler
        exportModuleDesc2.pBuildFlags = "-library-compilation";

        if (LevelZeroBlackBoxTests::verbose) {
            std::cout << "building export module\n";
        }

        SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &exportModuleDesc2, &exportModule2, nullptr));
    }

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "reading import module for spirv\n";
    }
    auto importBinaryModule = LevelZeroBlackBoxTests::compileToSpirV(importModuleSrcValue, "", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == importBinaryModule.size()));

    ze_module_handle_t importModule;
    ze_module_desc_t importModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    importModuleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    importModuleDesc.pInputModule = reinterpret_cast<const uint8_t *>(importBinaryModule.data());
    importModuleDesc.inputSize = importBinaryModule.size();
    if (circularDep) {
        importModuleDesc.pBuildFlags = "-library-compilation";
    }
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "building import module\n";
    }
    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &importModuleDesc, &importModule, nullptr));

    // Dynamically linking the two Modules to resolve the symbols

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Dynamically linking modules\n";
    }

    ze_module_build_log_handle_t dynLinkLog;

    if (circularDep) {
        ze_module_handle_t modulesToLink[] = {importModule, exportModule, exportModule2};
        SUCCESS_OR_TERMINATE(zeModuleDynamicLink(numModules, modulesToLink, &dynLinkLog));
    } else {
        ze_module_handle_t modulesToLink[] = {importModule, exportModule};
        SUCCESS_OR_TERMINATE(zeModuleDynamicLink(numModules, modulesToLink, &dynLinkLog));
    }

    size_t buildLogSize;
    SUCCESS_OR_TERMINATE(zeModuleBuildLogGetString(dynLinkLog, &buildLogSize, nullptr));
    char *logBuffer = new char[buildLogSize]();
    SUCCESS_OR_TERMINATE(zeModuleBuildLogGetString(dynLinkLog, &buildLogSize, logBuffer));

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Dynamically linked modules\n";
        std::cout << logBuffer << "\n";
    }

    // Create Kernel to call

    ze_kernel_handle_t importKernel;
    ze_kernel_desc_t importKernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    importKernelDesc.pKernelName = "call_library_funcs";
    SUCCESS_OR_TERMINATE(zeKernelCreate(importModule, &importKernelDesc, &importKernel));

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(importKernel, 1, 1, 1));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(importKernel, 0, sizeof(resultBuffer), &resultBuffer));

    // Create Command Queue and List

    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_handle_t cmdList;
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    // Append call to Kernel

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, importKernel, &dispatchTraits,
                                                         nullptr, 0, nullptr));

    // Execute the Kernel in the Import module which calls the Export Module's functions

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "execute kernel in import module\n";
    }
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "sync results from kernel\n";
    }
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate results
    int expectedResult = (((1 + 2) * 2) - 1);
    if (circularDep) {
        expectedResult = (((((1 + 2) + 2) + 5) * 2) - 1);
    }

    if (expectedResult != *(int *)resultBuffer) {
        std::cout << "Result:" << *(int *)resultBuffer << " invalid\n";
        outputValidationSuccessful = false;
    } else if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Result Buffer is correct with a value of:" << *(int *)resultBuffer << "\n";
    }

    // Cleanup
    delete[] logBuffer;
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(dynLinkLog));
    SUCCESS_OR_TERMINATE(zeMemFree(context, resultBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(importKernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(importModule));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(exportModule));
    if (circularDep) {
        SUCCESS_OR_TERMINATE(zeModuleDestroy(exportModule2));
    }

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

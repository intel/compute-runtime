/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <iostream>

struct Target {
    ze_driver_handle_t drv = {};
    ze_context_handle_t ctx = {};
    ze_device_handle_t dev = {};
};

std::vector<uint8_t> getNativeBinary(ze_module_handle_t module) {
    size_t moduleSize = 0U;
    std::vector<uint8_t> binary;
    SUCCESS_OR_TERMINATE(zeModuleGetNativeBinary(module, &moduleSize, nullptr));
    binary.resize(moduleSize);
    SUCCESS_OR_TERMINATE(zeModuleGetNativeBinary(module, &moduleSize, binary.data()));
    return binary;
}

std::string readBuildLog(ze_module_build_log_handle_t buildLog) {
    size_t buildLogSize = 0U;
    SUCCESS_OR_TERMINATE(zeModuleBuildLogGetString(buildLog, &buildLogSize, nullptr));
    std::vector<char> buildLogBuffer(buildLogSize, 0);
    SUCCESS_OR_TERMINATE(zeModuleBuildLogGetString(buildLog, &buildLogSize, buildLogBuffer.data()));
    std::string ret;
    ret.assign(buildLogBuffer.data(), buildLogBuffer.size());
    return ret;
}

std::vector<uint8_t> spirvToNativeBinary(const Target &target, const std::vector<uint8_t> &spirv, const char *buildOptions = nullptr) {
    ze_module_handle_t tempModule;
    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = spirv.data();
    moduleDesc.inputSize = spirv.size();
    moduleDesc.pBuildFlags = buildOptions;

    ze_module_build_log_handle_t buildLog = {};
    auto err = zeModuleCreate(target.ctx, target.dev, &moduleDesc, &tempModule, &buildLog);
    auto buildLogStr = readBuildLog(buildLog);
    if (false == buildLogStr.empty()) {
        std::cout << buildLogStr << std::endl;
    }
    zeModuleBuildLogDestroy(buildLog);
    SUCCESS_OR_TERMINATE(err);
    auto bin = getNativeBinary(tempModule);
    return bin;
}

std::vector<uint8_t> oclcToNativeBinary(const Target &target, const std::string &src, const char *frontendBuildOptions = nullptr, const char *backendBuildOptions = nullptr) {
    std::string frontendBuildLog;
    auto spirv = LevelZeroBlackBoxTests::compileToSpirV(src, frontendBuildOptions, frontendBuildLog);
    if (false == frontendBuildLog.empty()) {
        std::cout << frontendBuildLog << std::endl;
    }
    SUCCESS_OR_TERMINATE((0 == spirv.size()));
    return spirvToNativeBinary(target, spirv, backendBuildOptions);
}

bool testModulesPackage(ze_module_handle_t packageModule, const Target &target) {
    bool outputValidationSuccessful = true;

    void *resultBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(target.ctx, &deviceDesc, &hostDesc,
                         sizeof(int), 1, target.dev, &resultBuffer));

    // Create Kernel to call
    ze_kernel_handle_t importKernel;
    ze_kernel_desc_t importKernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    importKernelDesc.pKernelName = "call_library_funcs";
    SUCCESS_OR_TERMINATE(zeKernelCreate(packageModule, &importKernelDesc, &importKernel));

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(importKernel, 1, 1, 1));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(importKernel, 0, sizeof(resultBuffer), &resultBuffer));

    // Create Command Queue and List

    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, .mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS};
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(target.ctx, target.dev, &cmdQueueDesc, &cmdQueue));

    ze_command_list_handle_t cmdList;
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    SUCCESS_OR_TERMINATE(zeCommandListCreate(target.ctx, target.dev, &cmdListDesc, &cmdList));

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

    if (expectedResult != *(int *)resultBuffer) {
        std::cout << "Result:" << *(int *)resultBuffer << " invalid\n";
        outputValidationSuccessful = false;
    } else if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Result Buffer is correct with a value of:" << *(int *)resultBuffer << "\n";
    }

    SUCCESS_OR_TERMINATE(zeMemFree(target.ctx, resultBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(importKernel));

    return outputValidationSuccessful;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Modules Package";
    bool outputValidationSuccessful = true;
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    char *exportModuleSrcValue = const_cast<char *>(LevelZeroBlackBoxTests::DynamicLink::exportModuleSrc);
    char *importModuleSrcValue = const_cast<char *>(LevelZeroBlackBoxTests::DynamicLink::importModuleSrc);

    // Setup
    Target target;
    target.dev = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(target.ctx, target.drv)[0];

    // Build Import/Export binaries
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "building export module" << std::endl;
    }

    auto exportModuleBinary = oclcToNativeBinary(target, exportModuleSrcValue, "", "-library-compilation");

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "building import module\n"
                  << std::endl;
    }

    auto importModuleBinary = oclcToNativeBinary(target, importModuleSrcValue, "", "-library-compilation");

    // Creating modules package from two binary modules
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Creating modules package from two binary modules\n";
    }

    std::vector<std::vector<uint8_t> *> binaries{&importModuleBinary, &exportModuleBinary};

    ze_module_program_exp_desc_t moduleProgDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    moduleProgDesc.count = static_cast<uint32_t>(binaries.size());
    std::vector<const uint8_t *> inputModules(binaries.size(), nullptr);
    std::vector<size_t> inputModulesSizes(binaries.size(), 0);
    for (uint32_t i = 0; i < moduleProgDesc.count; ++i) {
        inputModules[i] = binaries[i]->data();
        inputModulesSizes[i] = binaries[i]->size();
    }
    moduleProgDesc.pInputModules = inputModules.data();
    moduleProgDesc.inputSizes = inputModulesSizes.data();

    ze_module_desc_t packageModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    packageModuleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    packageModuleDesc.pNext = &moduleProgDesc;

    ze_module_build_log_handle_t packageModuleLog = {};
    ze_module_handle_t packageModule = {};
    auto packageModuleLoadStatus = zeModuleCreate(target.ctx, target.dev, &packageModuleDesc, &packageModule, &packageModuleLog);
    auto packageBuildLogStr = readBuildLog(packageModuleLog);
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(packageModuleLog));

    if (false == packageBuildLogStr.empty()) {
        std::cout << packageBuildLogStr << std::endl;
    }

    SUCCESS_OR_TERMINATE(packageModuleLoadStatus);

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Succesfully loaded modules package" << std::endl;
    }

    outputValidationSuccessful = testModulesPackage(packageModule, target);

    // Native binary test

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Getting a native binary of the whole modules package" << std::endl;
    }

    auto nativeBinaryPackage = getNativeBinary(packageModule);
    SUCCESS_OR_TERMINATE_BOOL(nativeBinaryPackage.empty() == false);

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Creating a modules packages from a single native binary" << std::endl;
    }

    ze_module_desc_t binaryPackageModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    binaryPackageModuleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    binaryPackageModuleDesc.pNext = nullptr;
    binaryPackageModuleDesc.pInputModule = nativeBinaryPackage.data();
    binaryPackageModuleDesc.inputSize = nativeBinaryPackage.size();

    ze_module_build_log_handle_t binaryPackageModuleLog = {};
    ze_module_handle_t binaryPackageModule = {};
    auto binaryPackageModuleLoadStatus = zeModuleCreate(target.ctx, target.dev, &binaryPackageModuleDesc, &binaryPackageModule, &binaryPackageModuleLog);
    auto binaryPackageBuildLogStr = readBuildLog(binaryPackageModuleLog);
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(binaryPackageModuleLog));

    if (false == binaryPackageBuildLogStr.empty()) {
        std::cout << binaryPackageBuildLogStr << std::endl;
    }

    SUCCESS_OR_TERMINATE(binaryPackageModuleLoadStatus);

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Succesfully loaded modules package" << std::endl;
    }

    outputValidationSuccessful = testModulesPackage(binaryPackageModule, target) && outputValidationSuccessful;

    // Cleanup
    SUCCESS_OR_TERMINATE(zeModuleDestroy(packageModule));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

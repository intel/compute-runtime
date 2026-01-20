/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <algorithm>
#include <iostream>
#include <ranges>
#include <span>

constexpr auto progWithDummyKernel = R"OCLC(
__kernel void dummyKernel(){
}
)OCLC";

struct Target {
    ze_driver_handle_t drv = {};
    ze_context_handle_t ctx = {};
    ze_device_handle_t dev = {};
};

struct Library {
    std::vector<uint8_t> binaryWithImports;
    std::vector<uint8_t> binaryWithExport;
    std::vector<uint8_t> binaryWithDummyKernels;
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

bool testKernelsInModule(ze_module_handle_t packageModule, const Target &target) {
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

ze_module_handle_t createModuleFromNativeBinaries(const Target &target, std::span<std::span<const uint8_t>> binaries) {
    if (binaries.empty()) {
        return nullptr;
    }

    ze_module_desc_t packageModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    packageModuleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    if (binaries.size() == 1) {
        packageModuleDesc.inputSize = binaries[0].size();
        packageModuleDesc.pInputModule = binaries[0].data();
    }

    ze_module_program_exp_desc_t moduleProgDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    std::vector<const uint8_t *> additionalModulesInputs;
    std::vector<size_t> additionalModulesSizes;
    if (binaries.size() > 1) {
        auto additionalModules = binaries;

        additionalModulesInputs.reserve(additionalModules.size());
        std::ranges::transform(additionalModules, std::back_inserter(additionalModulesInputs),
                               [](const auto &bin) { return bin.data(); });
        std::ranges::transform(additionalModules, std::back_inserter(additionalModulesSizes),
                               [](const auto &bin) { return bin.size(); });

        moduleProgDesc.inputSizes = additionalModulesSizes.data();
        moduleProgDesc.pInputModules = additionalModulesInputs.data();
        moduleProgDesc.count = static_cast<uint32_t>(additionalModules.size());

        packageModuleDesc.pNext = &moduleProgDesc;
    }

    ze_module_build_log_handle_t buildLog = {};
    ze_module_handle_t module = {};
    auto success = zeModuleCreate(target.ctx, target.dev, &packageModuleDesc, &module, &buildLog);
    auto packageBuildLogStr = readBuildLog(buildLog);
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildLog));

    if (false == packageBuildLogStr.empty()) {
        std::cout << packageBuildLogStr << std::endl;
    }

    SUCCESS_OR_TERMINATE(success);
    return module;
}

bool testModulesPackageFromExt(const Target &target, const Library &library) {
    // Creating modules package from two binary modules
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Creating modules package from two binary modules\n";
    }

    std::span<const uint8_t> binaries[] = {{library.binaryWithImports}, {library.binaryWithExport}};
    ze_module_handle_t packageModule = createModuleFromNativeBinaries(target, binaries);

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Succesfully loaded modules package" << std::endl;
    }

    auto res = testKernelsInModule(packageModule, target);

    SUCCESS_OR_TERMINATE(zeModuleDestroy(packageModule));
    return res;
}

bool testModulesPackageFromBinary(const Target &target, const Library &library) {
    // Creating modules package from two binary modules
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Creating modules package from two binary modules\n";
    }

    std::vector<uint8_t> nativeBinaryPackage;
    {
        std::span<const uint8_t> binaries[] = {{library.binaryWithImports}, {library.binaryWithExport}};
        ze_module_handle_t packageModule = createModuleFromNativeBinaries(target, binaries);
        nativeBinaryPackage = getNativeBinary(packageModule);
        SUCCESS_OR_TERMINATE_BOOL(nativeBinaryPackage.empty() == false);
    }

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Creating a modules packages from a single native binary" << std::endl;
    }

    std::span<const uint8_t> binaries[] = {{nativeBinaryPackage}};
    ze_module_handle_t binaryPackageModule = createModuleFromNativeBinaries(target, binaries);

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Succesfully loaded modules package" << std::endl;
    }

    auto res = testKernelsInModule(binaryPackageModule, target);

    SUCCESS_OR_TERMINATE(zeModuleDestroy(binaryPackageModule));
    return res;
}

bool testModulesPackageLinking(const Target &target, Library &library) {
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Linking modules package with regular module\n";
    }

    std::span<const uint8_t> packageBinaries[] = {{library.binaryWithImports}, {library.binaryWithDummyKernels}};
    ze_module_handle_t packageModule = createModuleFromNativeBinaries(target, packageBinaries);

    std::span<const uint8_t> exportBinaries[] = {{library.binaryWithExport}};
    ze_module_handle_t exportsModule = createModuleFromNativeBinaries(target, exportBinaries);

    ze_module_build_log_handle_t dynLinkLog = {};
    ze_module_handle_t modulesToLink[] = {packageModule, exportsModule};
    auto linkResult = zeModuleDynamicLink(2, modulesToLink, &dynLinkLog);
    auto linkLog = readBuildLog(dynLinkLog);
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(dynLinkLog));
    if (false == linkLog.empty()) {
        std::cout << " Link log " << linkLog << std::endl;
    }
    SUCCESS_OR_TERMINATE(linkResult);

    uint32_t count = 0;
    zeModuleGetKernelNames(packageModule, &count, nullptr);
    std::vector<const char *> names(count, nullptr);
    zeModuleGetKernelNames(packageModule, &count, names.data());
    bool res = std::ranges::any_of(names, [](const auto &name) { return std::string_view("dummyKernel") == name; });
    if (false == res) {
        std::cout << "Could not find dummKernel in modules package" << std::endl;
    }
    res = testKernelsInModule(packageModule, target);
    SUCCESS_OR_TERMINATE(zeModuleDestroy(packageModule));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(exportsModule));

    return res;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Modules Package";
    bool outputValidationSuccessful = true;
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    // Setup
    Target target;
    target.dev = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(target.ctx, target.drv)[0];

    Library library;

    // Build Import/Export binaries
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "building input modules" << std::endl;
    }

    library.binaryWithExport = oclcToNativeBinary(target, LevelZeroBlackBoxTests::DynamicLink::exportModuleSrc,
                                                  "", "-library-compilation");

    library.binaryWithImports = oclcToNativeBinary(target, LevelZeroBlackBoxTests::DynamicLink::importModuleSrc,
                                                   "", "-library-compilation");

    library.binaryWithDummyKernels = oclcToNativeBinary(target, progWithDummyKernel,
                                                        "", "-library-compilation");

    outputValidationSuccessful &= testModulesPackageFromExt(target, library);
    outputValidationSuccessful &= testModulesPackageFromBinary(target, library);
    outputValidationSuccessful &= testModulesPackageLinking(target, library);

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

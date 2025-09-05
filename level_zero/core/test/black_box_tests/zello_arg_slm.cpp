/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>

constexpr std::string_view blackBoxName = "Zello Arg Slm";

void executeKernelAndValidate(ze_context_handle_t context, uint32_t deviceIdentfier,
                              bool &outputValidationSuccessful) {

    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Testing for device " << deviceIdentfier << std::endl;
    }
    ze_command_list_handle_t cmdList;
    auto device = LevelZeroBlackBoxTests::zerTranslateIdentifierToDeviceHandleFunc(deviceIdentfier);
    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &defaultIntelCommandQueueDesc, &cmdList));

    constexpr ze_group_count_t groupCounts{16, 1, 1};

    // Create output buffer
    void *dstBuffer = nullptr;
    constexpr size_t allocSize = groupCounts.groupCountX * sizeof(uint32_t) * 2;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &defaultIntelDeviceMemDesc, &defaultIntelHostMemDesc, allocSize, sizeof(uint32_t), device, &dstBuffer));

    std::string buildLog;
    auto spirV = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::slmArgKernelSrc, "", buildLog);
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

        std::vector<char> strLog(szLog + 1, 0);
        zeModuleBuildLogGetString(buildlog, &szLog, strLog.data());
        LevelZeroBlackBoxTests::printBuildLog(strLog.data());

        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
        std::cerr << std::endl
                  << blackBoxName << " Results validation FAILED. Module creation error."
                  << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "test_arg_slm";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
    ze_kernel_properties_t kernProps = {ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernProps));
    LevelZeroBlackBoxTests::printKernelProperties(kernProps, kernelDesc.pKernelName);

    for (auto groupSize : {64u, 128u, 256u}) {

        // Initialize memory
        constexpr uint8_t initValue = 77;
        zeCommandListAppendMemoryFill(cmdList, dstBuffer, &initValue, sizeof(initValue), allocSize, nullptr, 0, nullptr);

        ze_group_size_t groupSizes{groupSize, 1, 1};

        size_t localWorkSizeForUint = groupSizes.groupSizeX * 4u;

        struct InitValues {
            uint32_t initLocalIdSum;
            uint32_t initGlobalIdSum;
        };
        InitValues initValues = {2, 4};

        void *kernelArgs[] = {
            &dstBuffer,            // output buffer
            &localWorkSizeForUint, // local buffer for local ids
            &localWorkSizeForUint, // local buffer for global ids
            &initValues            // initial values for output sums
        };

        SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::zeCommandListAppendLaunchKernelWithArgumentsFunc(cmdList, kernel, groupCounts, groupSizes, kernelArgs, nullptr, nullptr, 0, nullptr));

        SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::zeDeviceSynchronizeFunc(device));

        // Validate
        outputValidationSuccessful = true;

        std::vector<uint32_t> expectedOutput(groupCounts.groupCountX * 2, 0);

        for (auto i = 0; i < static_cast<int>(groupCounts.groupCountX); ++i) {
            auto sumOfLocalIds = groupSize * (groupSize - 1) / 2;                                                          // Sum of local IDs from 0 to localWorkSize-1
            auto maxGlobalId = groupSize * (i + 1) - 1;                                                                    // max global id for this group
            auto minGlobalId = groupSize * i;                                                                              // min global id for this group
            auto sumOfGlobalIdWithinGroup = (maxGlobalId * (maxGlobalId + 1) / 2) - (minGlobalId * (minGlobalId - 1) / 2); // sum of global ids within this group

            expectedOutput[i * 2] = sumOfLocalIds + initValues.initLocalIdSum;
            expectedOutput[i * 2 + 1] = sumOfGlobalIdWithinGroup + initValues.initGlobalIdSum;
        }
        for (auto i = 0; i < static_cast<int>(expectedOutput.size()); ++i) {
            auto expectedValue = expectedOutput[i];
            auto actualValue = reinterpret_cast<uint32_t *>(dstBuffer)[i];
            if (actualValue != expectedValue) {
                std::cout << "dstBuffer[" << i << "] = "
                          << std::dec << actualValue << " not equal to "
                          << expectedValue << "\n";
                outputValidationSuccessful = false;
                break;
            }
        }
        if (!outputValidationSuccessful) {
            break;
        }
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
}

int main(int argc, char *argv[]) {
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    const char *errorMsg = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::zerGetLastErrorDescriptionFunc(&errorMsg));

    if (errorMsg != nullptr && errorMsg[0] != 0) {
        std::cerr << "Error initializing context: " << (errorMsg ? errorMsg : "Unknown error") << std::endl;
        return 1;
    }

    uint32_t deviceOrdinal = LevelZeroBlackBoxTests::zerTranslateDeviceHandleToIdentifierFunc(devices[0]);

    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::zerGetLastErrorDescriptionFunc(&errorMsg));

    if (errorMsg != nullptr && errorMsg[0] != 0) {
        std::cerr << "Error zerTranslateDeviceHandleToIdentifier: " << errorMsg << std::endl;
        return 1;
    }
    bool outputValidationSuccessful = false;
    executeKernelAndValidate(context, deviceOrdinal, outputValidationSuccessful);

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

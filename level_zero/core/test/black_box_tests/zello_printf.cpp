/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <iostream>
#include <numeric>
#ifdef WIN32
#include <windows.h>

#include <io.h>

#pragma warning(disable : 4996)
#else
#include <unistd.h>
#endif
const char *source = R"===(
__kernel void printf_kernel(char byteValue, short shortValue, int intValue, long longValue){
    printf("byte = %hhd\nshort = %hd\nint = %d\nlong = %ld", byteValue, shortValue, intValue, longValue);
}
)===";

void runPrintfKernel(ze_context_handle_t &context, ze_device_handle_t &device) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = 0;
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));

    std::string buildLog;
    auto spirV = compileToSpirV(source, "", buildLog);
    if (buildLog.size() > 0) {
        std::cout << "Build log " << buildLog;
    }
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    ze_module_handle_t module;
    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = spirV.data();
    moduleDesc.inputSize = spirV.size();
    moduleDesc.pBuildFlags = "";

    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));

    ze_kernel_handle_t kernel;
    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "printf_kernel";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    [[maybe_unused]] int8_t byteValue = std::numeric_limits<int8_t>::max();
    [[maybe_unused]] int16_t shortValue = std::numeric_limits<int16_t>::max();
    [[maybe_unused]] int32_t intValue = std::numeric_limits<int32_t>::max();
    [[maybe_unused]] int64_t longValue = std::numeric_limits<int64_t>::max();

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(byteValue), &byteValue));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(shortValue), &shortValue));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(intValue), &intValue));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 3, sizeof(longValue), &longValue));

    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, 1U, 1U, 1U));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

int main(int argc, char *argv[]) {
    verbose = isVerbose(argc, argv);
    const char *fileName = "zello_printf_output.txt";
    bool validatePrintfOutput = true;
    bool printfValidated = false;
    int stdoutFd = -1;

    ze_context_handle_t context = nullptr;
    auto devices = zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    if (validatePrintfOutput) {
        // duplicate stdout descriptor
        stdoutFd = dup(fileno(stdout));
        auto newFile = freopen(fileName, "w", stdout);
        if (newFile == nullptr) {
            std::cout << "Failed in freopen()" << std::endl;
            abort();
        }
    }

    runPrintfKernel(context, device);

    if (validatePrintfOutput) {
        std::string expectedString = "byte = 127\nshort = 32767\nint = 2147483647\nlong = 9223372036854775807";
        auto sizeOfBuffer = expectedString.size() + 1024;
        auto kernelOutput = std::make_unique<char[]>(sizeOfBuffer);
        memset(kernelOutput.get(), 0, sizeOfBuffer);
        auto kernelOutputFile = fopen(fileName, "r");
        [[maybe_unused]] auto result = fread(kernelOutput.get(), sizeof(char), sizeOfBuffer - 1, kernelOutputFile);
        fclose(kernelOutputFile);
        fflush(stdout);
        // adjust/restore stdout to previous descriptor
        dup2(stdoutFd, fileno(stdout));
        // close duplicate
        close(stdoutFd);
        std::remove(fileName);

        char *foundString = strstr(kernelOutput.get(), expectedString.c_str());
        if (foundString != nullptr) {
            printfValidated = true;
        }

        if (result > 0) {
            std::cout << "Printf output:\n"
                      << "\n------------------------------\n"
                      << kernelOutput.get() << "\n------------------------------\n"
                      << std::endl;
        } else {
            std::cout << "Printf output empty!" << std::endl;
        }
    }
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    if (validatePrintfOutput && !printfValidated) {
        std::cout << "\nZello Printf FAILED " << std::endl;
        return -1;
    }

    std::cout << "\nZello Printf PASSED " << std::endl;

    return 0;
}

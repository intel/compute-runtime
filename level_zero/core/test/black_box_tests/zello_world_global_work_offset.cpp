/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/source/helpers/string.h"

#include "level_zero/api/extensions/public/ze_exp_ext.h"

#include "zello_common.h"

#include <dlfcn.h>
#include <sstream>
#include <string.h>

extern bool verbose;
bool verbose = false;

const char *module = R"===(
__kernel void kernel_copy(__global char *dst, __global char *src){
    uint gid = get_global_id(0);
    dst[gid] = src[gid];
}
)===";

std::vector<uint8_t> compileToSpirV(const std::string &src, const std::string &options, std::string &outCompilerLog) {
    std::vector<uint8_t> ret;

    const char *mainFileName = "main.cl";
    const char *argv[] = {"ocloc", "-q", "-device", "skl", "-file", mainFileName};
    const unsigned char *sources[] = {reinterpret_cast<const unsigned char *>(src.c_str())};
    size_t sourcesLengths[] = {src.size() + 1};
    const char *sourcesNames[] = {mainFileName};
    unsigned int numOutputs = 0U;
    unsigned char **outputs = nullptr;
    size_t *ouputLengths = nullptr;
    char **outputNames = nullptr;

    int result = oclocInvoke(sizeof(argv) / sizeof(argv[0]), argv,
                             1, sources, sourcesLengths, sourcesNames,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &outputs, &ouputLengths, &outputNames);

    unsigned char *spirV = nullptr;
    size_t spirVlen = 0;
    const char *log = nullptr;
    size_t logLen = 0;
    for (unsigned int i = 0; i < numOutputs; ++i) {
        std::string spvExtension = ".spv";
        std::string logFileName = "stdout.log";
        auto nameLen = strlen(outputNames[i]);
        if ((nameLen > spvExtension.size()) && (strstr(&outputNames[i][nameLen - spvExtension.size()],
                                                       spvExtension.c_str()) != nullptr)) {
            spirV = outputs[i];
            spirVlen = ouputLengths[i];
        } else if ((nameLen >= logFileName.size()) && (strstr(outputNames[i], logFileName.c_str()) != nullptr)) {
            log = reinterpret_cast<const char *>(outputs[i]);
            logLen = ouputLengths[i];
            break;
        }
    }

    if ((result != 0) && (logLen == 0)) {
        outCompilerLog = "Unknown error, ocloc returned : " + std::to_string(result) + "\n";
        return ret;
    }

    if (logLen != 0) {
        outCompilerLog = std::string(log, logLen).c_str();
    }

    ret.assign(spirV, spirV + spirVlen);
    oclocFreeOutput(&numOutputs, &outputs, &ouputLengths, &outputNames);
    return ret;
}

typedef ze_result_t (*setGlobalWorkOffsetFunctionType)(ze_kernel_handle_t, uint32_t, uint32_t, uint32_t);

setGlobalWorkOffsetFunctionType findSymbolForSetGlobalWorkOffsetFunction(char *userPath) {
    char libPath[256];
    sprintf(libPath, "%s/libze_intel_gpu.so.1", userPath);
    void *libHandle = dlopen(libPath, RTLD_LAZY | RTLD_LOCAL);
    if (!libHandle) {
        std::cout << "libze_intel_gpu.so not found\n";
        std::terminate();
    }

    ze_result_t (*pfnSetGlobalWorkOffset)(ze_kernel_handle_t, uint32_t, uint32_t, uint32_t);
    *(void **)(&pfnSetGlobalWorkOffset) = dlsym(libHandle, "zeKernelSetGlobalOffsetExp");

    char *error;
    if ((error = dlerror()) != NULL) {
        std::cout << "Error while opening symbol: " << error << "\n";
        std::terminate();
    }

    return pfnSetGlobalWorkOffset;
}

void executeKernelAndValidate(ze_context_handle_t context,
                              ze_device_handle_t &device,
                              setGlobalWorkOffsetFunctionType pfnSetGlobalWorkOffset,
                              bool &outputValidationSuccessful) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_command_list_handle_t cmdList;

    cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));
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
    constexpr uint32_t bufferOffset = 8;
    constexpr uint8_t srcVal = 55;
    constexpr uint8_t dstVal = 77;
    memset(srcBuffer, srcVal, allocSize);
    memset(dstBuffer, 0, allocSize);

    uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);
    for (uint32_t i = 0; i < bufferOffset; i++) {
        dstCharBuffer[i] = dstVal;
    }

    std::string buildLog;
    auto spirV = compileToSpirV(module, "", buildLog);
    if (buildLog.size() > 0) {
        std::cout << "Build log " << buildLog;
    }
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
        std::cout << "Build log:" << strLog << std::endl;

        free(strLog);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "kernel_copy";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
    ze_kernel_properties_t kernProps = {ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernProps));
    std::cout << "Kernel : \n"
              << " * name : " << kernelDesc.pKernelName << "\n"
              << " * uuid.mid : " << kernProps.uuid.mid << "\n"
              << " * uuid.kid : " << kernProps.uuid.kid << "\n"
              << " * maxSubgroupSize : " << kernProps.maxSubgroupSize << "\n"
              << " * localMemSize : " << kernProps.localMemSize << "\n"
              << " * spillMemSize : " << kernProps.spillMemSize << "\n"
              << " * privateMemSize : " << kernProps.privateMemSize << "\n"
              << " * maxNumSubgroups : " << kernProps.maxNumSubgroups << "\n"
              << " * numKernelArgs : " << kernProps.numKernelArgs << "\n"
              << " * requiredSubgroupSize : " << kernProps.requiredSubgroupSize << "\n"
              << " * requiredNumSubGroups : " << kernProps.requiredNumSubGroups << "\n"
              << " * requiredGroupSizeX : " << kernProps.requiredGroupSizeX << "\n"
              << " * requiredGroupSizeY : " << kernProps.requiredGroupSizeY << "\n"
              << " * requiredGroupSizeZ : " << kernProps.requiredGroupSizeZ << "\n";

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcBuffer), &srcBuffer));

    uint32_t offsetx = bufferOffset;
    uint32_t offsety = 0;
    uint32_t offsetz = 0;
    SUCCESS_OR_TERMINATE(pfnSetGlobalWorkOffset(kernel, offsetx, offsety, offsetz));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                         nullptr, 0, nullptr));

    // Close list and submit for execution
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate
    outputValidationSuccessful = true;
    uint8_t *srcCharBuffer = static_cast<uint8_t *>(srcBuffer);
    for (size_t i = 0; i < allocSize; i++) {
        if (i < bufferOffset) {
            if (dstCharBuffer[i] != dstVal) {
                std::cout << "dstBuffer[" << i << "] = "
                          << std::dec << static_cast<unsigned int>(dstCharBuffer[i]) << " not equal to "
                          << dstVal << "\n";
                outputValidationSuccessful = false;
                break;
            }
        } else {
            if (dstCharBuffer[i] != srcCharBuffer[i]) {
                std::cout << "dstBuffer[" << i << "] = "
                          << std::dec << static_cast<unsigned int>(dstCharBuffer[i]) << " not equal to "
                          << "srcBuffer[" << i << "] = "
                          << std::dec << static_cast<unsigned int>(srcCharBuffer[i]) << "\n";
                outputValidationSuccessful = false;
                break;
            }
        }
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

int main(int argc, char *argv[]) {
    verbose = isVerbose(argc, argv);
    ze_driver_handle_t driverHandle;
    ze_context_handle_t context = nullptr;
    auto devices = zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    bool outputValidationSuccessful;

    const char *defaultPath = "/usr/local/lib/";
    char userPath[256]{};
    if (argc == 2) {
        strncpy_s(userPath, sizeof(userPath), argv[1], 256);
    } else {
        strncpy_s(userPath, sizeof(userPath), defaultPath, strlen(defaultPath));
    }

    uint32_t extensionsCount = 0;
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionProperties(driverHandle, &extensionsCount, nullptr));
    if (extensionsCount == 0) {
        std::cout << "No extensions supported on this driver\n";
        std::terminate();
    }

    std::vector<ze_driver_extension_properties_t> extensionsSupported(extensionsCount);
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionProperties(driverHandle, &extensionsCount, extensionsSupported.data()));
    bool globalOffsetExtensionFound = false;
    std::string globalOffsetName = "ZE_experimental_global_offset";
    for (uint32_t i = 0; i < extensionsSupported.size(); i++) {
        if (strncmp(extensionsSupported[i].name, globalOffsetName.c_str(), globalOffsetName.size()) == 0) {
            if (extensionsSupported[i].version == ZE_GLOBAL_OFFSET_EXP_VERSION_1_0) {
                globalOffsetExtensionFound = true;
                break;
            }
        }
    }
    if (globalOffsetExtensionFound == false) {
        std::cout << "No global offset extension found on this driver\n";
        std::terminate();
    }

    setGlobalWorkOffsetFunctionType pfnSetGlobalWorkOffset = findSymbolForSetGlobalWorkOffsetFunction(userPath);

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    std::cout << "Device : \n"
              << " * name : " << deviceProperties.name << "\n"
              << " * vendorId : " << std::hex << deviceProperties.vendorId << "\n";

    executeKernelAndValidate(context, device, pfnSetGlobalWorkOffset, outputValidationSuccessful);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    std::cout << "\nZello World Global Work Offset Results validation "
              << (outputValidationSuccessful ? "PASSED" : "FAILED") << "\n";

    return 0;
}

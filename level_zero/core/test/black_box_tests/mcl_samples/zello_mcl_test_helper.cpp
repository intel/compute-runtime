/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/black_box_tests/mcl_samples/zello_mcl_test_helper.h"

#include <cstring>
#include <sstream>

namespace MclTests {

const char *mcl4GbBuildOption = "-ze-opt-greater-than-4GB-buffer-required ";
const char *mclDisableInlineDataOption = "-igc_opts 'EnablePassInlineData=-1' ";
const char *mclDisablePrivate2Scratch = "-igc_opts 'EnableOCLScratchPrivateMemory=0' ";
std::string mclBuildOption("");

std::string mclExtensionString("ZE_experimental_mutable_command_list");

ExecEnv::ExecEnv() {
    this->startTime = std::chrono::high_resolution_clock::now();

    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    device = devices[0];

    ze_driver_extension_properties_t mclExtension{};

    strncpy(mclExtension.name, mclExtensionString.c_str(), mclExtensionString.size());
    mclExtension.version = ZE_MUTABLE_COMMAND_LIST_EXP_VERSION_1_0;

    std::vector<ze_driver_extension_properties_t> extensionsToCheck;
    extensionsToCheck.push_back(mclExtension);

    bool mclExtensionFound = LevelZeroBlackBoxTests::checkExtensionIsPresent(driverHandle, extensionsToCheck);
    if (mclExtensionFound == false) {
        std::cerr << "No MCL extension found on this driver" << std::endl;
        std::terminate();
    }
    getMclDeviceProperties(this->device, this->mclDeviceProperties);

    this->mutationCapabilities = {
        isMclCapabilitySupported(mclDeviceProperties, ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS),
        isMclCapabilitySupported(mclDeviceProperties, ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT),
        isMclCapabilitySupported(mclDeviceProperties, ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE),
        isMclCapabilitySupported(mclDeviceProperties, ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET),
        isMclCapabilitySupported(mclDeviceProperties, ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT),
        isMclCapabilitySupported(mclDeviceProperties, ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS)};

    extensionsToCheck[0].version = ZE_MUTABLE_COMMAND_LIST_EXP_VERSION_1_1;
    bool mclKernelExtensionFound = LevelZeroBlackBoxTests::checkExtensionIsPresent(driverHandle, extensionsToCheck);
    if (mclKernelExtensionFound) {
        this->mutationCapabilities.kernelInstructions = isMclCapabilitySupported(mclDeviceProperties, ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION);
    }
    printMclDeviceSupportedMutations(this->mutationCapabilities);

    cbEventsExtension = LevelZeroBlackBoxTests::counterBasedEventsExtensionPresent(driverHandle);

    SUCCESS_OR_TERMINATE(zeDeviceGetComputeProperties(this->device, &this->computeProperties));

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &queue));

    // auxiliary immediate command list to reset/copy buffers on the GPU
    LevelZeroBlackBoxTests::createImmediateCmdlistWithMode(context, device,
                                                           true,  // syncMode
                                                           false, // copyEngine
                                                           immCmdList);

    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexVariableSetValue", reinterpret_cast<void **>(&zexVariableSetValueFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListLoadNativeBinary", reinterpret_cast<void **>(&zexCommandListLoadNativeBinaryFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListGetVariablesList", reinterpret_cast<void **>(&zexCommandListGetVariablesListFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListGetNativeBinary", reinterpret_cast<void **>(&zexCommandListGetNativeBinaryFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListGetVariable", reinterpret_cast<void **>(&zexCommandListGetVariableFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListGetLabel", reinterpret_cast<void **>(&zexCommandListGetLabelFunc)));

    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexKernelSetArgumentVariable", reinterpret_cast<void **>(&zexKernelSetArgumentVariableFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexKernelSetVariableGroupSize", reinterpret_cast<void **>(&zexKernelSetVariableGroupSizeFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListAppendVariableLaunchKernel", reinterpret_cast<void **>(&zexCommandListAppendVariableLaunchKernelFunc)));

    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListTempMemSetEleCount", reinterpret_cast<void **>(&zexCommandListTempMemSetEleCountFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListTempMemGetSize", reinterpret_cast<void **>(&zexCommandListTempMemGetSizeFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListTempMemSet", reinterpret_cast<void **>(&zexCommandListTempMemSetFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListAppendJump", reinterpret_cast<void **>(&zexCommandListAppendJumpFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListSetLabel", reinterpret_cast<void **>(&zexCommandListSetLabelFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListAppendLoadRegVariable", reinterpret_cast<void **>(&zexCommandListAppendLoadRegVariableFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListAppendMIMath", reinterpret_cast<void **>(&zexCommandListAppendMIMathFunc)));
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListAppendStoreRegVariable", reinterpret_cast<void **>(&zexCommandListAppendStoreRegVariableFunc)));

    if (cbEventsExtension) {
        SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexEventGetDeviceAddress", reinterpret_cast<void **>(&zexEventGetDeviceAddressFunc)));
        LevelZeroBlackBoxTests::loadCounterBasedEventCreateFunction(driverHandle);
    }

    if (!this->mutationCapabilities.kernelArguments) {
        mclBuildOption.append(mcl4GbBuildOption);
    }

    cooperativeQueueOrdinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, true);
    if (cmdQueueDesc.ordinal == cooperativeQueueOrdinal) {
        cooperativeQueue = queue;
    }
}

bool ExecEnv::verifyGroupSizeSupported(const uint32_t *groupSize) const {
    auto totalGroupSize = groupSize[0] * groupSize[1] * groupSize[2];
    if (totalGroupSize > this->computeProperties.maxTotalGroupSize) {
        std::cerr << "Total group size " << totalGroupSize << " exceeds device supported " << this->computeProperties.maxTotalGroupSize << std::endl;
        return false;
    }
    if (groupSize[0] > this->computeProperties.maxGroupSizeX) {
        std::cerr << "Group size X " << groupSize[0] << " exceeds device supported " << this->computeProperties.maxGroupSizeX << std::endl;
        return false;
    }
    if (groupSize[1] > this->computeProperties.maxGroupSizeY) {
        std::cerr << "Group size Y " << groupSize[1] << " exceeds device supported " << this->computeProperties.maxGroupSizeY << std::endl;
        return false;
    }
    if (groupSize[2] > this->computeProperties.maxGroupSizeZ) {
        std::cerr << "Group size Z " << groupSize[2] << " exceeds device supported " << this->computeProperties.maxGroupSizeZ << std::endl;
        return false;
    }

    return true;
}

ze_command_queue_handle_t ExecEnv::getCooperativeQueue() {
    if (cooperativeQueue == nullptr) {
        cooperativeQueue = LevelZeroBlackBoxTests::createCommandQueue(context, device, nullptr, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, true);
    }
    return cooperativeQueue;
}

ze_module_handle_t ExecEnv::getGlobalBarrierModule() {
    if (globalBarrierModule == nullptr) {
        MclTests::createModule(this->context, this->device, this->globalBarrierModule, MclTests::Kernels::globalBarrierKernelSources, nullptr);
    }
    return globalBarrierModule;
}

ze_module_handle_t ExecEnv::getSlmModule() {
    if (slmModule == nullptr) {
        MclTests::createModule(this->context, this->device, this->slmModule, MclTests::Kernels::slmKernelSources, nullptr);
    }
    return slmModule;
}

ze_kernel_handle_t ExecEnv::getKernelHandle(ze_module_handle_t module, const std::string kernelName) {
    ze_kernel_handle_t kernel = nullptr;
    if (!this->globalKernels) {
        ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
        kernelDesc.pKernelName = kernelName.c_str();
        SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
    } else {
        kernel = this->kernels[kernelName];
        if (kernel == nullptr) {
            ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
            kernelDesc.pKernelName = kernelName.c_str();
            SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
            this->kernels[kernelName] = kernel;
        }
    }
    return kernel;
}

void ExecEnv::buildMainModule(ze_module_handle_t &module) {
    MclTests::createModule(this->context, this->device, module, MclTests::Kernels::sources, nullptr);
    if (this->globalKernels) {
        std::cout << "Kernels are shared." << std::endl;
        this->mainModule = module;
    }
}

void ExecEnv::setGlobalKernels(int argc, char *argv[]) {
    constexpr int defaultValue = 1;
    this->globalKernels = !!(LevelZeroBlackBoxTests::getParamValue(argc, argv, "-sk", "--shared_kernels", defaultValue));
}

MclProgram *MclProgram::loadProgramFromBinary(ArrayRef<const char> programData, ExecEnv *env) {
    return new MclProgram(programData, env);
}

int MclProgram::setVar(std::string varName, size_t size, void *pVal) {
    if (vars.count(varName) == 0) {
        std::cerr << "No variable found" << std::endl;
        std::terminate();
    }
    SUCCESS_OR_TERMINATE(execEnv->zexVariableSetValueFunc(vars[varName], 0, size, pVal));
    return 0;
}

int MclProgram::setVars(std::vector<Var> vars) {
    int retVal = 0U;
    for (auto &var : vars) {
        retVal |= setVar(var.name, var.size, var.pVal);
    }
    return retVal;
}

MclProgram::MclProgram(ArrayRef<const char> programData, ExecEnv *env) : execEnv(env) {
    createMutableCmdList(env->context, env->device, cmdList, false, false);
    SUCCESS_OR_TERMINATE(env->zexCommandListLoadNativeBinaryFunc(cmdList, programData.begin(), programData.size()));

    uint32_t varCnt = 0U;
    SUCCESS_OR_TERMINATE(env->zexCommandListGetVariablesListFunc(cmdList, &varCnt, nullptr));
    varInfoVec.resize(varCnt);
    SUCCESS_OR_TERMINATE(env->zexCommandListGetVariablesListFunc(cmdList, &varCnt, varInfoVec.data()));
    for (auto &varInfo : varInfoVec) {
        vars[varInfo->name] = varInfo->handle;
    }
}

void createMutableCmdList(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_command_list_handle_t &hCmdList, bool inOrder, bool useCooperativeFlag) {
    ze_command_list_desc_t descriptor = {};
    descriptor.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;

    ze_mutable_command_list_exp_desc_t mutableExpDesc{};
    mutableExpDesc.stype = ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXP_DESC;

    descriptor.pNext = &mutableExpDesc;
    if (inOrder) {
        descriptor.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
    } else {
        descriptor.flags = 0;
    }
    descriptor.commandQueueGroupOrdinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(hDevice, useCooperativeFlag);

    SUCCESS_OR_TERMINATE(zeCommandListCreate(hContext, hDevice, &descriptor, &hCmdList));
}

void createModule(ze_context_handle_t context, ze_device_handle_t device, ze_module_handle_t &module, const char *pModule, const char *optionalDumpFileName) {
    std::string buildLog;
    auto spirV = LevelZeroBlackBoxTests::compileToSpirV(pModule, "-cl-std=CL2.0", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    if (optionalDumpFileName != nullptr) {
        std::stringstream fileName;
        fileName << optionalDumpFileName << ".spv";
        std::cout << "Dumping SPV file: " << fileName.str() << std::endl;
        std::ofstream out(fileName.str(), std::ios::binary);
        out.write(reinterpret_cast<const char *>(spirV.data()), spirV.size());
    }

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_build_log_handle_t buildlog;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = spirV.data();
    moduleDesc.inputSize = spirV.size();
    moduleDesc.pBuildFlags = mclBuildOption.c_str();
    if (zeModuleCreate(context, device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
        size_t szLog = 0;
        zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

        char *strLog = (char *)malloc(szLog);
        zeModuleBuildLogGetString(buildlog, &szLog, strLog);
        LevelZeroBlackBoxTests::printBuildLog(strLog);

        free(strLog);
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
        std::cerr << "\nZello MCL Test Results validation FAILED. Module creation error."
                  << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    if (optionalDumpFileName != nullptr) {
        std::vector<uint8_t> data;
        size_t moduleSize = 0;
        SUCCESS_OR_TERMINATE(zeModuleGetNativeBinary(module, &moduleSize, nullptr));
        data.resize(moduleSize);
        SUCCESS_OR_TERMINATE(zeModuleGetNativeBinary(module, &moduleSize, data.data()));

        std::stringstream fileName;
        fileName << optionalDumpFileName << ".bin";
        std::cout << "Dumping Native file: " << fileName.str() << std::endl;
        std::ofstream out(fileName.str(), std::ios::binary);
        out.write(reinterpret_cast<const char *>(data.data()), data.size());
    }
}

void getMclBinary(ze_command_list_handle_t cmdList, ze_module_handle_t module, std::vector<char> &mclBinaryBuffer, MclTests::ExecEnv *env) {
    size_t moduleSize;
    std::vector<uint8_t> moduleData;
    SUCCESS_OR_TERMINATE(zeModuleGetNativeBinary(module, &moduleSize, nullptr));
    moduleData.resize(moduleSize);
    SUCCESS_OR_TERMINATE(zeModuleGetNativeBinary(module, &moduleSize, moduleData.data()));

    size_t programSize;
    SUCCESS_OR_TERMINATE(env->zexCommandListGetNativeBinaryFunc(cmdList, nullptr, &programSize, moduleData.data(), moduleSize));
    mclBinaryBuffer.resize(programSize);
    SUCCESS_OR_TERMINATE(env->zexCommandListGetNativeBinaryFunc(cmdList, mclBinaryBuffer.data(), &programSize, moduleData.data(), moduleSize));
}

void saveMclProgram(ze_command_list_handle_t cmdList, ze_module_handle_t module, const std::string &programName, MclTests::ExecEnv *env) {
    std::vector<char> programData;
    getMclBinary(cmdList, module, programData, env);
    std::cout << "Dumping MCL binary file: " << programName << std::endl;
    std::ofstream out(programName, std::ios::binary);
    out.write(reinterpret_cast<const char *>(programData.data()), programData.size());
}

zex_variable_handle_t getVariable(ze_command_list_handle_t cmdList, const std::string &varName, MclTests::ExecEnv *env) {
    zex_variable_handle_t var;
    zex_variable_desc_t varDesc = {};
    varDesc.stype = ZEX_STRUCTURE_TYPE_VARIABLE_DESCRIPTOR;
    varDesc.name = varName.c_str();
    SUCCESS_OR_TERMINATE(env->zexCommandListGetVariableFunc(cmdList, &varDesc, &var));
    return var;
}

zex_variable_handle_t getTmpVariable(ze_command_list_handle_t cmdList, size_t size, bool isScalable, MclTests::ExecEnv *env) {
    zex_variable_handle_t var;
    zex_variable_desc_t varDesc = {};
    varDesc.stype = ZEX_STRUCTURE_TYPE_VARIABLE_DESCRIPTOR;
    varDesc.name = nullptr;
    zex_temp_variable_desc_t varDescTmp = {};
    varDescTmp.stype = ZEX_STRUCTURE_TYPE_TEMP_VARIABLE_DESCRIPTOR;
    varDesc.pNext = &varDescTmp;
    varDescTmp.size = size;
    if (isScalable) {
        varDescTmp.flags = ZEX_TEMP_VARIABLE_FLAGS_IS_SCALABLE;
    } else {
        varDescTmp.flags = ZEX_TEMP_VARIABLE_FLAGS_IS_CONST_SIZE;
    }
    SUCCESS_OR_TERMINATE(env->zexCommandListGetVariableFunc(cmdList, &varDesc, &var));
    return var;
}

zex_label_handle_t getLabel(ze_command_list_handle_t cmdList, const std::string &labelName, MclTests::ExecEnv *env) {
    zex_label_handle_t label;
    zex_label_desc_t labelDesc = {};
    labelDesc.stype = ZEX_STRUCTURE_TYPE_LABEL_DESCRIPTOR;
    labelDesc.name = labelName.c_str();
    labelDesc.alignment = 0x0U;
    SUCCESS_OR_TERMINATE(env->zexCommandListGetLabelFunc(cmdList, &labelDesc, &label));
    return label;
}

void getMclDeviceProperties(ze_device_handle_t device, ze_mutable_command_list_exp_properties_t &mclProperties) {
    mclProperties.stype = ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXP_PROPERTIES;
    mclProperties.pNext = nullptr;

    ze_device_properties_t deviceProperties{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    deviceProperties.pNext = &mclProperties;

    zeDeviceGetProperties(device, &deviceProperties);
}

void printMclDeviceSupportedMutations(MclMutationCapabilities &mutationCapabilities) {
    std::cout << "MCL capabilities :" << std::endl
              << " * Kernel argument : " << std::boolalpha << mutationCapabilities.kernelArguments << std::endl
              << " * Group count : " << std::boolalpha << mutationCapabilities.groupCount << std::endl
              << " * Group size : " << std::boolalpha << mutationCapabilities.groupSize << std::endl
              << " * Global offset : " << std::boolalpha << mutationCapabilities.globalOffset << std::endl
              << " * Signal event : " << std::boolalpha << mutationCapabilities.signalEvent << std::endl
              << " * Wait events : " << std::boolalpha << mutationCapabilities.waitEvents << std::endl
              << " * Kernel instruction : " << std::boolalpha << mutationCapabilities.kernelInstructions << std::endl;
}

bool isMclCapabilitySupported(ze_mutable_command_list_exp_properties_t &mclProperties, ze_mutable_command_exp_flag_t flag) {
    return !!(mclProperties.mutableCommandFlags & flag);
}

void setEventPoolEventFlags(EventOptions eventOptions,
                            ze_event_pool_flags_t &eventPoolFlag,
                            zex_counter_based_event_desc_t *counterBasedDesc,
                            ze_event_scope_flags_t &signalScope) {
    eventPoolFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    signalScope = 0;

    if (isTimestampEvent(eventOptions)) {
        eventPoolFlag |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    }
    if (isSignalEvent(eventOptions)) {
        signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    }

    if (counterBasedDesc && isCbEvent(eventOptions)) {
        counterBasedDesc->flags |= ZEX_COUNTER_BASED_EVENT_FLAG_HOST_VISIBLE;
        if (isTimestampEvent(eventOptions)) {
            counterBasedDesc->flags |= ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP;
        }
        counterBasedDesc->signalScope = signalScope;
    }
}

void createEventPoolAndEvents(ExecEnv &execEnv,
                              ze_event_pool_handle_t &eventPool,
                              uint32_t poolSize,
                              ze_event_handle_t *events,
                              EventOptions eventOptions) {
    ze_event_pool_flags_t eventPoolFlag = 0;
    ze_event_scope_flags_t signalScope = 0;
    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE;
    bool cbEventsCreate = isCbEvent(eventOptions);

    setEventPoolEventFlags(eventOptions, eventPoolFlag, &counterBasedDesc, signalScope);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(execEnv.context, execEnv.device, eventPool,
                                                     eventPoolFlag,
                                                     cbEventsCreate, &counterBasedDesc,
                                                     poolSize, events,
                                                     signalScope,
                                                     0);
}

void destroyEventPool(ze_event_pool_handle_t &eventPool, EventOptions eventOptions) {
    if (!isCbEvent(eventOptions)) {
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    }
}

bool isCbEvent(EventOptions eventOptions) {
    return eventOptions == cbEvent || eventOptions == cbEventTimestamp || eventOptions == cbEventSignal || eventOptions == cbEventSignalTimestamp;
}

void setEventTestStream(EventOptions eventOptions, std::ostringstream &testStream) {
    if (eventOptions == MclTests::EventOptions::noEvent) {
        testStream << "No events";
    } else {
        testStream << "Events are";
        if (eventOptions == MclTests::EventOptions::none) {
            testStream << " none flag";
        } else {
            if (isTimestampEvent(eventOptions)) {
                testStream << " timestamp";
            }
            if (isSignalEvent(eventOptions)) {
                testStream << " signal scope";
            }
            if (isCbEvent(eventOptions)) {
                testStream << " counter based";
            }
        }
    }
}

bool isTimestampEvent(EventOptions eventOptions) {
    return eventOptions == timestamp || eventOptions == signalEventTimestamp || eventOptions == cbEventTimestamp || eventOptions == cbEventSignalTimestamp;
}

bool isSignalEvent(EventOptions eventOptions) {
    return eventOptions == MclTests::EventOptions::signalEvent ||
           eventOptions == MclTests::EventOptions::signalEventTimestamp ||
           eventOptions == MclTests::EventOptions::cbEventSignal ||
           eventOptions == MclTests::EventOptions::cbEventSignalTimestamp;
}

namespace Kernels {

const char *sources = R"OpenCLC(

typedef ulong ComplexStructureType;

struct ConditionalCalculations1 {
    char conditionMul;
    ComplexStructureType valueMul;
    char conditionAdd;
    ComplexStructureType valueAdd;
};

struct ConditionalCalculations2 {
    ComplexStructureType valueMul;
    ComplexStructureType valueAdd;
    char conditionMul;
    char conditionAdd;
};

struct ConditionalCalculations3 {
    ComplexStructureType valueMul;
    char conditionMul;
    ComplexStructureType valueAdd;
    char conditionAdd;
};

struct ConditionalCalculations4 {
    ComplexStructureType valueMul;
    char conditionMul;
    char conditionAdd;
    ComplexStructureType valueAdd;
};

__kernel void computeScalarKernel1(__global ComplexStructureType *src, __global ComplexStructureType *dst, struct ConditionalCalculations1 input) {
    uint gid = get_global_id(0);
    ComplexStructureType value = src[gid];
    if (input.conditionMul != 0) {
        value *= input.valueMul;
    }
    if (input.conditionAdd != 0) {
        value += input.valueAdd;
    }
    dst[gid] = value;
}

__kernel void computeScalarKernel2(__global ComplexStructureType *src, __global ComplexStructureType *dst, struct ConditionalCalculations2 input) {
    uint gid = get_global_id(0);
    ComplexStructureType value = src[gid];
    if (input.conditionMul != 0) {
        value *= input.valueMul;
    }
    if (input.conditionAdd != 0) {
        value += input.valueAdd;
    }
    dst[gid] = value;
}

__kernel void computeScalarKernel3(__global ComplexStructureType *src, __global ComplexStructureType *dst, struct ConditionalCalculations3 input) {
    uint gid = get_global_id(0);
    ComplexStructureType value = src[gid];
    if (input.conditionMul != 0) {
        value *= input.valueMul;
    }
    if (input.conditionAdd != 0) {
        value += input.valueAdd;
    }
    dst[gid] = value;
}

__kernel void computeScalarKernel4(__global ComplexStructureType *src, __global ComplexStructureType *dst, struct ConditionalCalculations4 input) {
    uint gid = get_global_id(0);
    ComplexStructureType value = src[gid];
    if (input.conditionMul != 0) {
        value *= input.valueMul;
    }
    if (input.conditionAdd != 0) {
        value += input.valueAdd;
    }
    dst[gid] = value;
}

struct ConditionalMulCalculations1 {
    char conditionMul;
    ComplexStructureType valueMul;
};

struct ConditionalMulCalculations2 {
    ComplexStructureType valueMul;
    char conditionMul;
};

__kernel void computeScalarMulKernel1(__global ComplexStructureType *src, __global ComplexStructureType *dst, struct ConditionalMulCalculations1 input) {
    uint gid = get_global_id(0);
    ComplexStructureType value = src[gid];
    if (input.conditionMul != 0) {
        value *= input.valueMul;
    }
    dst[gid] = value;
}

__kernel void computeScalarMulKernel2(__global ComplexStructureType *src, __global ComplexStructureType *dst, struct ConditionalMulCalculations2 input) {
    uint gid = get_global_id(0);
    ComplexStructureType value = src[gid];
    if (input.conditionMul != 0) {
        value *= input.valueMul;
    }
    dst[gid] = value;
}

__kernel void addScalarKernel(__global uint *src, __global uint *dst, uint value) {
    uint gid = get_global_id(0);
    dst[gid] = src[gid] + value;
}

__kernel void mulScalarKernel(__global uint *src, __global uint *dst, uint value) {
    uint gid = get_global_id(0);
    dst[gid] = src[gid] * value;
}

__kernel void copyKernel(__global char *src, __global char *dst) {
    uint gid = get_global_id(0);
    dst[gid] = src[gid];
}

__kernel void copyKernelLinear(__global char *src, __global char *dst) {
    uint gid = get_global_linear_id();
    dst[gid] = src[gid];
}

__kernel void copyKernelOptional(__global char *src, __global char *dst) {
    uint gid = get_global_id(0);
    if (src != 0 && dst != 0) {
        dst[gid] = src[gid];
    }
}

__kernel void addScalarKernelLinear(__global uint *src, __global uint *dst, uint value) {
    uint glid = get_global_linear_id();
    dst[glid] = src[glid] + value;
}

__kernel void addScalarSlmKernel(__global uint *src, __global uint *dst, uint value) {
    uint gid = get_global_id(0);
    uint lid = get_local_id(0);
    __local uint slmValue;
    if(lid == 0) {
        slmValue = value;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    dst[gid] = src[gid] + slmValue;
}

__kernel void signalKernel(ulong signalAddressValue, ulong signalValueAddressValue) {
    uint gid = get_global_id(0);
    if (gid == 0) {
        __global ulong *signalAddress = (__global ulong *)signalAddressValue;
        __global ulong *signalValueAddress = (__global ulong *)signalValueAddressValue;
        *signalAddress = *signalValueAddress;
    }
}

__kernel void getGlobalWorkSize(__global ulong *dst) {
    size_t globalSizeX = get_global_size(0);
    size_t globalSizeY = get_global_size(1);
    size_t globalSizeZ = get_global_size(2);
    size_t gidX = get_global_id(0);
    size_t gidY = get_global_id(1);
    size_t gidZ = get_global_id(2);
    if (gidX == globalSizeX - 1 &&
        gidY == globalSizeY - 1 &&
        gidZ == globalSizeZ - 1) {
        dst[0] = get_global_size(0);
        dst[1] = get_global_size(1);
        dst[2] = get_global_size(2);
        dst[3] = gidX;
        dst[4] = gidY;
        dst[5] = gidZ;
    }
}

__kernel void getLocalWorkSize(__global ulong *dst) {
    size_t localSizeX = get_local_size(0);
    size_t localSizeY = get_local_size(1);
    size_t localSizeZ = get_local_size(2);
    size_t lidX = get_local_id(0);
    size_t lidY = get_local_id(1);
    size_t lidZ = get_local_id(2);
    if (lidX == localSizeX - 1 &&
        lidY == localSizeY - 1 &&
        lidZ == localSizeZ - 1) {
        dst[0] = get_local_size(0);
        dst[1] = get_local_size(1);
        dst[2] = get_local_size(2);
        dst[3] = lidX;
        dst[4] = lidY;
        dst[5] = lidZ;
        dst[6] = 0;
    }
    barrier (CLK_LOCAL_MEM_FENCE);
    barrier (CLK_GLOBAL_MEM_FENCE);
    __global uint *atomicDst = (__global uint *)(dst + 6);
    atomic_inc(atomicDst);
}

__kernel void getGlobaOffset(__global ulong *dst) {
    size_t globalSizeX = get_global_size(0);
    size_t globalSizeY = get_global_size(1);
    size_t globalSizeZ = get_global_size(2);
    size_t gidX = get_global_id(0);
    size_t gidY = get_global_id(1);
    size_t gidZ = get_global_id(2);

    if (gidX == globalSizeX - 1 &&
        gidY == globalSizeY - 1 &&
        gidZ == globalSizeZ - 1) {
        dst[0] = get_global_offset(0);
        dst[1] = get_global_offset(1);
        dst[2] = get_global_offset(2);
        dst[3] = get_group_id(0);
        dst[4] = get_group_id(1);
        dst[5] = get_group_id(2);
    }
    if(gidX == 0 &&
       gidY == 0 &&
       gidZ == 0) {
        dst[6] = 1;
    }
}

__kernel void addSourceAndOneKernel(__global char *src, __global char *dst){
    uint gid = get_global_id(0);
    dst[gid] = src[gid] + 1;
}

__kernel void addOneKernel(__global uchar *buffer){
    uint gid = get_global_id(0);
    buffer[gid] += 1;
}

__kernel void single_task(__global uint *dst) {
    for (size_t i = 0; i < 1024; ++i) {
        dst[i] = i;
    }
}

)OpenCLC";

const char *sourcesPrivateKernel = R"OpenCLC(

__kernel void privateMemoryArray(__global uint *src, __global uint *dst, uint value) {
    uint gid = get_global_id(0);
    const int arraySize = 256;
    uint privateArray[arraySize];
    uint srcData = src[gid];
    uint loopCount = value;
    if(loopCount > arraySize) {
        loopCount = arraySize;
    }

    for(uint i = 0; i < loopCount; i++) {
        privateArray[i] = srcData + value + i;
    }

    uint sum = 0;
    for(uint i = 0; i < loopCount; i++) {
        sum += privateArray[i];
    }

    dst[gid] = sum;
}

)OpenCLC";

const char *globalBarrierKernelSources = R"OpenCLC(

__kernel void globalBarrierKernel(__global uint *src, __global uint *dst, __global uint *stage) {
    uint groupId = get_group_id(0);
    uint globalId = get_global_id(0);
    uint localId = get_local_id(0);

    uint data = src[globalId];

    __global uint *address = stage + localId;
    atomic_init(address, 0);

    global_barrier();
    data += groupId;
    atomic_add(address, data);

    global_barrier();

    dst[globalId] = *address;
}

)OpenCLC";

const char *slmKernelSources = R"OpenCLC(

__kernel void slmKernelOneArg(__global uint *output, __local uint *slm) {
    uint gid = get_global_id(0);
    uint lid = get_local_id(0);
    slm[lid] = lid + 1;
    barrier(CLK_LOCAL_MEM_FENCE);
    output[gid] = slm[lid];
}

__kernel void slmKernelTwoArgs(__global uint *output, __local uint *slm, __local uint *slmReverse) {
    uint gid = get_global_id(0);
    uint lid = get_local_id(0);
    uint localSize = get_local_size(0);
    if(lid == 0) {
        for(uint i = 0; i < localSize; i++) {
            slm[i] = i;
            slmReverse[i] = (localSize - i - 1);
        }
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    output[gid] = slm[lid] + slmReverse[lid] + gid;
}

)OpenCLC";

} // namespace Kernels

void buildPrivateMemoryKernel(ze_context_handle_t context,
                              ze_device_handle_t device,
                              ze_module_handle_t &module,
                              ze_kernel_handle_t &kernel,
                              bool passDisableFlag) {
    std::string buildLog;
    auto spirV = LevelZeroBlackBoxTests::compileToSpirV(Kernels::sourcesPrivateKernel, "-cl-std=CL2.0", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    std::string buildOptionsCopy = mclBuildOption;
    if (passDisableFlag) {
        buildOptionsCopy.append(mclDisablePrivate2Scratch);
    }
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Private kernel build options: \"" << buildOptionsCopy << "\"" << std::endl;
    }

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_build_log_handle_t buildlog;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = spirV.data();
    moduleDesc.inputSize = spirV.size();
    moduleDesc.pBuildFlags = buildOptionsCopy.c_str();
    if (zeModuleCreate(context, device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
        size_t szLog = 0;
        zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

        char *strLog = (char *)malloc(szLog);
        zeModuleBuildLogGetString(buildlog, &szLog, strLog);
        LevelZeroBlackBoxTests::printBuildLog(strLog);

        free(strLog);
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
        std::cerr << "\nZello MCL Test Results validation FAILED. Module with private memory creation error."
                  << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "privateMemoryArray";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    ze_kernel_properties_t kernelProperties{ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernelProperties));
    std::cout << "privateMemoryArray Private size = " << std::dec << kernelProperties.privateMemSize << std::endl;
    std::cout << "privateMemoryArray Spill size = " << std::dec << kernelProperties.spillMemSize << std::endl;
}

bool validateGroupSizeData(uint64_t *outputData, uint32_t *groupSize, bool before) {
    bool valid = true;
    uint32_t groupSizeTotal = groupSize[0] * groupSize[1] * groupSize[2];

    std::string stage = before ? "before " : "after ";

    if (outputData[0] != groupSize[0]) {
        std::cerr << stage << "mutation fail outputData[0] != " << groupSize[0] << ": " << outputData[0] << std::endl;
        valid = false;
    }
    if (outputData[1] != groupSize[1]) {
        std::cerr << stage << "mutation fail outputData[1] != " << groupSize[1] << ": " << outputData[1] << std::endl;
        valid = false;
    }
    if (outputData[2] != groupSize[2]) {
        std::cerr << stage << "mutation fail outputData[2] != " << groupSize[2] << ": " << outputData[2] << std::endl;
        valid = false;
    }

    if (outputData[3] != groupSize[0] - 1) {
        std::cerr << stage << "mutation fail outputData[3] != " << groupSize[0] - 1 << ": " << outputData[3] << std::endl;
        valid = false;
    }
    if (outputData[4] != groupSize[1] - 1) {
        std::cerr << stage << "mutation fail outputData[4] != " << groupSize[1] - 1 << ": " << outputData[4] << std::endl;
        valid = false;
    }
    if (outputData[5] != groupSize[2] - 1) {
        std::cerr << stage << "mutation fail outputData[5] != " << groupSize[2] - 1 << ": " << outputData[5] << std::endl;
        valid = false;
    }
    if (outputData[6] != groupSizeTotal) {
        std::cerr << stage << "mutation fail outputData[6] != " << groupSizeTotal << ": " << outputData[6] << std::endl;
        valid = false;
    }
    return valid;
}

bool validateGroupCountData(uint64_t *outputData, uint32_t *groupCount, bool before) {
    bool valid = true;

    std::string stage = before ? "before " : "after ";

    if (outputData[0] != groupCount[0]) {
        std::cerr << stage << "mutation fail outputData[0] != " << groupCount[0] << ": " << outputData[0] << std::endl;
        valid = false;
    }
    if (outputData[1] != groupCount[1]) {
        std::cerr << stage << "mutation fail outputData[1] != " << groupCount[1] << ": " << outputData[1] << std::endl;
        valid = false;
    }
    if (outputData[2] != groupCount[2]) {
        std::cerr << stage << "mutation fail outputData[2] != " << groupCount[2] << ": " << outputData[2] << std::endl;
        valid = false;
    }

    if (outputData[3] != groupCount[0] - 1) {
        std::cerr << stage << "mutation fail outputData[3] != " << groupCount[0] - 1 << ": " << outputData[3] << std::endl;
        valid = false;
    }
    if (outputData[4] != groupCount[1] - 1) {
        std::cerr << stage << "mutation fail outputData[4] != " << groupCount[1] - 1 << ": " << outputData[4] << std::endl;
        valid = false;
    }
    if (outputData[5] != groupCount[2] - 1) {
        std::cerr << stage << "mutation fail outputData[5] != " << groupCount[2] - 1 << ": " << outputData[5] << std::endl;
        valid = false;
    }
    return valid;
}

bool validateGlobalOffsetData(uint64_t *outputData, uint32_t *offset, uint32_t *dispatchGroup, bool before) {
    bool valid = true;

    bool pointZeroSubmitted = (offset[0] == 0) && (offset[1] == 0) && (offset[2] == 0);

    uint32_t pointZeroValue = pointZeroSubmitted ? 1 : 0;

    std::string stage = before ? "before " : "after ";

    if (outputData[0] != offset[0]) {
        std::cerr << stage << "mutation fail outputData[0] != " << offset[0] << ": " << outputData[0] << std::endl;
        valid = false;
    }
    if (outputData[1] != offset[1]) {
        std::cerr << stage << "mutation fail outputData[1] != " << offset[1] << ": " << outputData[1] << std::endl;
        valid = false;
    }
    if (outputData[2] != offset[2]) {
        std::cerr << stage << "mutation fail outputData[2] != " << offset[2] << ": " << outputData[2] << std::endl;
        valid = false;
    }

    if (outputData[3] != dispatchGroup[0] - offset[0] - 1) {
        std::cerr << stage << "mutation fail outputData[3] != " << dispatchGroup[0] - offset[0] - 1 << ": " << outputData[3] << std::endl;
        valid = false;
    }
    if (outputData[4] != dispatchGroup[1] - offset[1] - 1) {
        std::cerr << stage << "mutation fail outputData[4] != " << dispatchGroup[1] - offset[1] - 1 << ": " << outputData[4] << std::endl;
        valid = false;
    }
    if (outputData[5] != dispatchGroup[2] - offset[2] - 1) {
        std::cerr << stage << "mutation fail outputData[5] != " << dispatchGroup[2] - offset[2] - 1 << ": " << outputData[5] << std::endl;
        valid = false;
    }

    if (outputData[6] != pointZeroValue) {
        std::cerr << stage << "mutation fail outputData[6] != " << pointZeroValue << ": " << outputData[6] << std::endl;
        valid = false;
    }

    return valid;
}

void prepareOutputSlmKernelOneArg(std::vector<uint32_t> &data, uint32_t groupSize, uint32_t groupCount) {
    uint32_t globalSize = groupSize * groupCount;
    data.resize(globalSize);
    for (uint32_t gid = 0; gid < groupCount; gid++) {
        for (uint32_t lid = 0; lid < groupSize; lid++) {
            data[gid * groupSize + lid] = lid + 1;
        }
    }
}

void prepareOutputSlmKernelTwoArgs(std::vector<uint32_t> &data, uint32_t groupSize, uint32_t groupCount) {
    uint32_t globalSize = groupSize * groupCount;
    data.resize(globalSize);

    uint32_t localSum = groupSize - 1;

    for (uint32_t gid = 0; gid < globalSize; gid++) {
        data[gid] = localSum + gid;
    }
}

static std::vector<std::string> failedCases;

void addFailedCase(bool caseResult, const std::string &caseName, uint32_t testNumber) {
    if (!caseResult) {
        std::stringstream stream;

        stream << "Test mask bit " << testNumber << " " << caseName;
        failedCases.push_back(stream.str());
    }
}

void printFailedCases() {
    if (failedCases.empty()) {
        return;
    }
    size_t failedCount = failedCases.size();
    std::cerr << "Failed cases:" << std::endl;
    for (size_t i = 0; i < failedCount; i++) {
        std::cerr << failedCases[i] << std::endl;
        if (i + 1 != failedCount) {
            std::cerr << "===================================" << std::endl;
        }
    }
}

} // namespace MclTests

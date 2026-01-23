/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/arrayref.h"

#include "level_zero/core/test/black_box_tests/common/zello_common.h"
#include "level_zero/core/test/black_box_tests/common/zello_compile.h"
#include "level_zero/driver_experimental/mcl_ext/zex_mutable_cmdlist_ext.h"
#include "level_zero/ze_intel_gpu.h"
#include "level_zero/ze_stypes.h"

#include <chrono>
#include <memory>
#include <ostream>
#include <unordered_map>
#include <vector>

namespace MclTests {

struct MclMutationCapabilities {
    bool kernelArguments = false;
    bool groupCount = false;
    bool groupSize = false;
    bool globalOffset = false;
    bool signalEvent = false;
    bool waitEvents = false;
    bool kernelInstructions = false;
};

class ExecEnv {
  public:
    ze_context_handle_t context = nullptr;
    ze_device_handle_t device = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    ze_command_queue_handle_t queue = nullptr;
    ze_command_queue_handle_t cooperativeQueue = nullptr;
    ze_module_handle_t globalBarrierModule = nullptr;
    ze_module_handle_t slmModule = nullptr;
    ze_module_handle_t mainModule = nullptr;
    ze_command_list_handle_t immCmdList = nullptr;
    ze_mutable_command_list_exp_properties_t mclDeviceProperties = {ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXP_PROPERTIES};
    ze_device_compute_properties_t computeProperties = {ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES};

    MclMutationCapabilities mutationCapabilities;

    static ExecEnv *create() {
        return new ExecEnv();
    }
    ~ExecEnv() {
        for (auto &kernel : this->kernels) {
            SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel.second));
        }
        if (cooperativeQueue != nullptr && cooperativeQueue != queue) {
            SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cooperativeQueue));
        }
        if (globalBarrierModule != nullptr) {
            SUCCESS_OR_TERMINATE(zeModuleDestroy(globalBarrierModule));
        }
        if (slmModule != nullptr) {
            SUCCESS_OR_TERMINATE(zeModuleDestroy(slmModule));
        }
        if (mainModule != nullptr) {
            SUCCESS_OR_TERMINATE(zeModuleDestroy(mainModule));
        }
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(queue));
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(immCmdList));
        std::chrono::seconds durationTime{0};

        std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
        durationTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - this->startTime);
        std::cout << "Test duration: " << durationTime.count() << " seconds" << std::endl;
    }

    bool getCbEventExtensionPresent() const {
        return cbEventsExtension;
    }

    uint32_t getCooperativeQueueOrdinal() const {
        return cooperativeQueueOrdinal;
    }

    bool verifyGroupSizeSupported(const uint32_t *groupSize) const;

    ze_command_queue_handle_t getCooperativeQueue();
    ze_module_handle_t getGlobalBarrierModule();
    ze_module_handle_t getSlmModule();

    bool getGlobalKernels() {
        return globalKernels;
    }
    void setGlobalKernels(int argc, char *argv[]);
    void buildMainModule(ze_module_handle_t &module);
    void releaseMainModule(ze_module_handle_t &module) {
        if (!this->globalKernels) {
            SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
        }
    }
    ze_kernel_handle_t getKernelHandle(ze_module_handle_t module, const std::string kernelName);
    void destroyKernelHandle(ze_kernel_handle_t kernel) {
        if (!this->globalKernels) {
            SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
        }
    }

    zex_pfnVariableSetValueCb_t zexVariableSetValueFunc = nullptr;
    zex_pfnCommandListLoadNativeBinaryCb_t zexCommandListLoadNativeBinaryFunc = nullptr;
    zex_pfnCommandListGetVariablesListCb_t zexCommandListGetVariablesListFunc = nullptr;
    zex_pfnCommandListGetNativeBinaryCb_t zexCommandListGetNativeBinaryFunc = nullptr;
    zex_pfnCommandListGetVariableCb_t zexCommandListGetVariableFunc = nullptr;
    zex_pfnCommandListGetLabelCb_t zexCommandListGetLabelFunc = nullptr;
    zex_pfnKernelSetArgumentVariableCb_t zexKernelSetArgumentVariableFunc = nullptr;
    zex_pfnKernelSetVariableGroupSizeCb_t zexKernelSetVariableGroupSizeFunc = nullptr;
    zex_pfnCommandListAppendVariableLaunchKernelCb_t zexCommandListAppendVariableLaunchKernelFunc = nullptr;
    zex_pfnCommandListTempMemSetEleCountCb_t zexCommandListTempMemSetEleCountFunc = nullptr;
    zex_pfnCommandListTempMemGetSizeCb_t zexCommandListTempMemGetSizeFunc = nullptr;
    zex_pfnCommandListTempMemSetCb_t zexCommandListTempMemSetFunc = nullptr;
    zex_pfnCommandListAppendJumpCb_t zexCommandListAppendJumpFunc = nullptr;
    zex_pfnCommandListSetLabelCb_t zexCommandListSetLabelFunc = nullptr;
    zex_pfnCommandListAppendLoadRegVariableCb_t zexCommandListAppendLoadRegVariableFunc = nullptr;
    zex_pfnCommandListAppendMIMathCb_t zexCommandListAppendMIMathFunc = nullptr;
    zex_pfnCommandListAppendStoreRegVariableCb_t zexCommandListAppendStoreRegVariableFunc = nullptr;

    decltype(&zexEventGetDeviceAddress) zexEventGetDeviceAddressFunc = nullptr;

  protected:
    ExecEnv();

    std::unordered_map<std::string, ze_kernel_handle_t> kernels;
    std::chrono::high_resolution_clock::time_point startTime;

    uint32_t cooperativeQueueOrdinal = std::numeric_limits<uint32_t>::max();
    bool cbEventsExtension = false;
    bool globalKernels = false;
};

class MclProgram {
  public:
    static MclProgram *loadProgramFromBinary(ArrayRef<const char> programData, ExecEnv *env);

    struct Var {
        std::string name;
        size_t size;
        void *pVal;
    };

    int setVar(std::string varName, size_t size, void *pVal);

    int setVars(std::vector<Var> vars);

    const std::vector<const zex_variable_info_t *> getVarsInfo() const {
        return varInfoVec;
    }

    ze_command_list_handle_t getCmdlist() const {
        return cmdList;
    }

    ~MclProgram() {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }

  protected:
    MclProgram(ArrayRef<const char> programData, ExecEnv *env);

    std::unordered_map<std::string, zex_variable_handle_t> vars;
    std::vector<const zex_variable_info_t *> varInfoVec;
    ze_command_list_handle_t cmdList;
    ExecEnv *execEnv = nullptr;
};

void createMutableCmdList(ze_context_handle_t hContext, ze_device_handle_t hDevice, ze_command_list_handle_t &hCmdList, bool inOrder, bool useCooperativeFlag);

void createModule(ze_context_handle_t context, ze_device_handle_t device, ze_module_handle_t &module, const char *pModule, const char *optionalDumpFileName);

void saveMclProgram(ze_command_list_handle_t cmdList, ze_module_handle_t module, const std::string &programName, MclTests::ExecEnv *env);

void getMclBinary(ze_command_list_handle_t cmdList, ze_module_handle_t module, std::vector<char> &mclBinaryBuffer, MclTests::ExecEnv *env);

zex_variable_handle_t getVariable(ze_command_list_handle_t cmdList, const std::string &varName, MclTests::ExecEnv *env);

zex_variable_handle_t getTmpVariable(ze_command_list_handle_t cmdList, size_t size, bool isScalable, MclTests::ExecEnv *env);

zex_label_handle_t getLabel(ze_command_list_handle_t cmdList, const std::string &labelName, MclTests::ExecEnv *env);

void getMclDeviceProperties(ze_device_handle_t device, ze_mutable_command_list_exp_properties_t &mclProperties);

void printMclDeviceSupportedMutations(MclMutationCapabilities &mutationCapabilities);

bool isMclCapabilitySupported(ze_mutable_command_list_exp_properties_t &mclProperties, ze_mutable_command_exp_flag_t flag);

enum EventOptions {
    noEvent,
    none,
    timestamp,
    signalEvent,
    signalEventTimestamp,
    cbEvent,
    cbEventTimestamp,
    cbEventSignal,
    cbEventSignalTimestamp
};

enum class KernelType { copy,
                        globalBarrier,
                        globalBarrierMultiplicationGroupCount,
                        slmKernelOneArgs,
                        slmKernelTwoArgs };

void setEventPoolEventFlags(EventOptions eventOptions,
                            ze_event_pool_flags_t &eventPoolFlag,
                            zex_counter_based_event_desc_t *counterBasedDesc,
                            ze_event_scope_flags_t &signalScope);

void createEventPoolAndEvents(ExecEnv &execEnv,
                              ze_event_pool_handle_t &eventPool,
                              uint32_t poolSize,
                              ze_event_handle_t *events,
                              EventOptions eventOptions);

void destroyEventPool(ze_event_pool_handle_t &eventPool, EventOptions eventOptions);

bool isCbEvent(EventOptions eventOptions);

bool isTimestampEvent(EventOptions eventOptions);

bool isSignalEvent(EventOptions eventOptions);

void setEventTestStream(EventOptions eventOptions, std::ostringstream &testStream);
void buildPrivateMemoryKernel(ze_context_handle_t context,
                              ze_device_handle_t device,
                              ze_module_handle_t &module,
                              ze_kernel_handle_t &kernel,
                              bool passDisableFlag);
bool validateGroupSizeData(uint64_t *outputData, uint32_t *groupSize, bool before);
bool validateGroupCountData(uint64_t *outputData, uint32_t *groupCount, bool before);
bool validateGlobalOffsetData(uint64_t *outputData, uint32_t *offset, uint32_t *dispatchGroup, bool before);

void prepareOutputSlmKernelOneArg(std::vector<uint32_t> &data, uint32_t groupSize, uint32_t groupCount);
void prepareOutputSlmKernelTwoArgs(std::vector<uint32_t> &data, uint32_t groupSize, uint32_t groupCount);

void addFailedCase(bool caseResult, const std::string &caseName, uint32_t testNumber);
void printFailedCases();
LevelZeroBlackBoxTests::TestBitMask getTestSubmask(int argc, char *argv[], uint32_t defaultValue);

namespace Kernels {
extern const char *sources;
extern const char *sourcesPrivateKernel;
extern const char *globalBarrierKernelSources;
extern const char *slmKernelSources;
} // namespace Kernels

extern std::string mclBuildOption;

} // namespace MclTests

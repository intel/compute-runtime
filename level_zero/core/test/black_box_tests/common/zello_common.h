/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/driver_experimental/zex_event.h"
#include <level_zero/ze_api.h>

#include <bitset>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace LevelZeroBlackBoxTests {
template <bool terminateOnFailure, typename ResulT>
inline void validate(ResulT result, const char *message);
} // namespace LevelZeroBlackBoxTests

#define SUCCESS_OR_TERMINATE(CALL) LevelZeroBlackBoxTests::validate<true>(CALL, #CALL)
#define SUCCESS_OR_TERMINATE_BOOL(FLAG) LevelZeroBlackBoxTests::validate<true>(!(FLAG), #FLAG)
#define SUCCESS_OR_WARNING(CALL) LevelZeroBlackBoxTests::validate<false>(CALL, #CALL)
#define SUCCESS_OR_WARNING_BOOL(FLAG) LevelZeroBlackBoxTests::validate<false>(!(FLAG), #FLAG)

namespace LevelZeroBlackBoxTests {

using pfnZexZexEventGetDeviceAddress = ze_result_t(ZE_APICALL *)(ze_event_handle_t event, uint64_t *completionValue, uint64_t *address);
using pfnZexCounterBasedEventCreate2 = ze_result_t(ZE_APICALL *)(ze_context_handle_t hContext, ze_device_handle_t hDevice, const zex_counter_based_event_desc_t *desc, ze_event_handle_t *phEvent);

#define QTR(a) #a
#define TOSTR(b) QTR(b)

extern bool verbose;

template <bool terminateOnFailure, typename ResulT>
inline void validate(ResulT result, const char *message) {
    if (result == ZE_RESULT_SUCCESS) {
        if (verbose) {
            std::cerr << "SUCCESS : " << message << std::endl;
        }
        return;
    }

    if (verbose) {
        std::cerr << (terminateOnFailure ? "ERROR : " : "WARNING : ") << message << " : " << result
                  << std::endl;
    }

    if (terminateOnFailure) {
        std::terminate();
    }
}

bool isParamEnabled(int argc, char *argv[], const char *shortName, const char *longName);

int getParamValue(int argc, char *argv[], const char *shortName, const char *longName, int defaultValue);
uint32_t getParamValue(int argc, char *argv[], const char *shortName, const char *longName, uint32_t defaultValue);

const char *getParamValue(int argc, char *argv[], const char *shortName, const char *longName, const char *defaultString);

bool isCircularDepTest(int argc, char *argv[]);

bool isVerbose(int argc, char *argv[]);

bool isSyncQueueEnabled(int argc, char *argv[]);

bool isAsyncQueueEnabled(int argc, char *argv[]);

bool isAubMode(int argc, char *argv[]);

bool isCommandListShared(int argc, char *argv[]);

bool isImmediateFirst(int argc, char *argv[]);

bool getAllocationFlag(int argc, char *argv[], int defaultValue);

void selectQueueMode(ze_command_queue_desc_t &desc, bool useSync);

uint32_t getBufferLength(int argc, char *argv[], uint32_t defaultLength);

void getErrorMax(int argc, char *argv[]);

void printResult(bool aubMode, bool outputValidationSuccessful, const std::string &blackBoxName, const std::string &currentTest);

void printResult(bool aubMode, bool outputValidationSuccessful, const std::string &blackBoxName);

uint32_t getCommandQueueOrdinal(ze_device_handle_t &device, bool useCooperativeFlag);

std::vector<uint32_t> getComputeQueueOrdinals(ze_device_handle_t &device);

uint32_t getCopyOnlyCommandQueueOrdinal(ze_device_handle_t &device);

ze_command_queue_handle_t createCommandQueue(ze_context_handle_t &context, ze_device_handle_t &device,
                                             uint32_t *ordinal, ze_command_queue_mode_t mode,
                                             ze_command_queue_priority_t priority, bool useCooperativeFlag);

ze_command_queue_handle_t createCommandQueueWithOrdinal(ze_context_handle_t &context, ze_device_handle_t &device,
                                                        uint32_t ordinal, ze_command_queue_mode_t mode,
                                                        ze_command_queue_priority_t priority);

ze_command_queue_handle_t createCommandQueue(ze_context_handle_t &context, ze_device_handle_t &device, uint32_t *ordinal, bool useCooperativeFlag);

ze_result_t createCommandList(ze_context_handle_t &context, ze_device_handle_t &device, ze_command_list_handle_t &cmdList, bool useCooperativeFlag);
ze_result_t createCommandList(ze_context_handle_t &context, ze_device_handle_t &device, ze_command_list_handle_t &cmdList, uint32_t ordinal);

void createEventPoolAndEvents(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_event_pool_handle_t &eventPool,
                              ze_event_pool_flags_t poolFlag,
                              bool counterEvents,
                              const zex_counter_based_event_desc_t *counterBasedDesc,
                              pfnZexCounterBasedEventCreate2 zexCounterBasedEventCreate2Func,
                              uint32_t poolSize,
                              ze_event_handle_t *events,
                              ze_event_scope_flags_t signalScope,
                              ze_event_scope_flags_t waitScope);

bool counterBasedEventsExtensionPresent(ze_driver_handle_t &driverHandle);

std::vector<ze_device_handle_t> zelloGetSubDevices(ze_device_handle_t &device, uint32_t &subDevCount);

std::vector<ze_device_handle_t> zelloInitContextAndGetDevices(ze_context_handle_t &context, ze_driver_handle_t &driverHandle);

std::vector<ze_device_handle_t> zelloInitContextAndGetDevices(ze_context_handle_t &context);

void initialize(ze_driver_handle_t &driver, ze_context_handle_t &context, ze_device_handle_t &device, ze_command_queue_handle_t &cmdQueue, uint32_t &ordinal);

bool checkImageSupport(ze_device_handle_t hDevice, bool test1D, bool test2D, bool test3D, bool testArray);

void teardown(ze_context_handle_t context, ze_command_queue_handle_t cmdQueue);

void printDeviceProperties(const ze_device_properties_t &props);

void printCacheProperties(uint32_t index, const ze_device_cache_properties_t &props);

void printP2PProperties(const ze_device_p2p_properties_t &props, bool canAccessPeer, uint32_t device0Index, uint32_t device1Index);

void printKernelProperties(const ze_kernel_properties_t &props, const char *kernelName);

void printCommandQueueGroupsProperties(ze_device_handle_t &device);

const std::vector<const char *> &getResourcesSearchLocations();

void setEnvironmentVariable(const char *variableName, const char *variableValue);

// read binary file into a non-NULL-terminated string
template <typename SizeT>
inline std::unique_ptr<char[]> readBinaryFile(const std::string &name, SizeT &outSize) {
    for (const char *base : getResourcesSearchLocations()) {
        std::string s(base);
        std::ifstream file(s + name, std::ios_base::in | std::ios_base::binary);
        if (false == file.good()) {
            continue;
        }

        size_t length;
        file.seekg(0, file.end);
        length = static_cast<size_t>(file.tellg());
        file.seekg(0, file.beg);

        auto storage = std::make_unique<char[]>(length);
        file.read(storage.get(), length);

        outSize = static_cast<SizeT>(length);
        return storage;
    }
    outSize = 0;
    return nullptr;
}

// read text file into a NULL-terminated string
template <typename SizeT>
inline std::unique_ptr<char[]> readTextFile(const std::string &name, SizeT &outSize) {
    for (const char *base : getResourcesSearchLocations()) {
        std::string s(base);
        std::ifstream file(s + name, std::ios_base::in);
        if (false == file.good()) {
            continue;
        }

        size_t length;
        file.seekg(0, file.end);
        length = static_cast<size_t>(file.tellg());
        file.seekg(0, file.beg);

        auto storage = std::make_unique<char[]>(length + 1);
        file.read(storage.get(), length);
        storage[length] = '\0';

        outSize = static_cast<SizeT>(length);
        return storage;
    }
    outSize = 0;
    return nullptr;
}

template <typename T = uint8_t>
inline bool validate(const void *expected, const void *tested, size_t len) {
    bool resultsAreOk = true;
    size_t offset = 0;

    const T *expectedT = reinterpret_cast<const T *>(expected);
    const T *testedT = reinterpret_cast<const T *>(tested);
    uint32_t errorsCount = 0;
    constexpr uint32_t errorsMax = 20;
    while (offset < len) {
        if (expectedT[offset] != testedT[offset]) {
            resultsAreOk = false;
            if (verbose == false) {
                break;
            }

            std::cerr << "Data mismatch expectedU8[" << offset << "] != testedU8[" << offset
                      << "]   ->    " << +expectedT[offset] << " != " << +testedT[offset]
                      << std::endl;
            ++errorsCount;
            if (errorsCount >= errorsMax) {
                std::cerr << "Found " << errorsCount
                          << " data mismatches - skipping further comparison " << std::endl;
                break;
            }
        }
        ++offset;
    }

    return resultsAreOk;
}

extern uint32_t overrideErrorMax;

template <typename T>
inline bool validateToValue(const T expected, const void *tested, size_t len) {
    bool resultsAreOk = true;
    size_t offset = 0;

    const T *testedT = reinterpret_cast<const T *>(tested);
    uint32_t errorsCount = 0;
    uint32_t errorsMax = verbose ? 20 : 1;
    errorsMax = overrideErrorMax > 0 ? overrideErrorMax : errorsMax;
    while (offset < len) {
        if (expected != testedT[offset]) {
            resultsAreOk = false;

            std::cerr << "Data mismatch expected != tested[" << offset
                      << "]   ->    " << +expected << " != " << +testedT[offset]
                      << std::endl;
            ++errorsCount;
            if (errorsCount >= errorsMax) {
                std::cerr << "Found " << errorsCount
                          << " data mismatches - skipping further comparison " << std::endl;
                break;
            }
        }
        ++offset;
    }

    return resultsAreOk;
}

struct CommandHandler {
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    bool isImmediate = false;

    ze_result_t create(ze_context_handle_t context, ze_device_handle_t device, bool immediate);

    ze_result_t appendKernel(ze_kernel_handle_t kernel, const ze_group_count_t &dispatchTraits, ze_event_handle_t event = nullptr) {
        return zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                               event, 0, nullptr);
    }

    ze_result_t execute();
    ze_result_t synchronize();
    ze_result_t destroy();
};

using TestBitMask = std::bitset<32>;

TestBitMask getTestMask(int argc, char *argv[], uint32_t defaultValue);

void printGroupCount(ze_group_count_t &groupCount);

void printBuildLog(std::string &buildLog);

void printBuildLog(const char *strLog);

void loadDriverExtensions(ze_driver_handle_t &driverHandle, std::vector<ze_driver_extension_properties_t> &driverExtensions);

bool checkExtensionIsPresent(ze_driver_handle_t &driverHandle, std::vector<ze_driver_extension_properties_t> &extensionsToCheck);

void prepareScratchTestValues(uint32_t &arraySize, uint32_t &vectorSize, uint32_t &expectedMemorySize, uint32_t &srcAdditionalMul, uint32_t &srcMemorySize, uint32_t &idxMemorySize);
void prepareScratchTestBuffers(void *srcBuffer, void *idxBuffer, void *expectedMemory, uint32_t arraySize, uint32_t vectorSize, uint32_t expectedMemorySize, uint32_t srcAdditionalMul);

} // namespace LevelZeroBlackBoxTests

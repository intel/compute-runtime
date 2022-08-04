/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#define QTR(a) #a
#define TOSTR(b) QTR(b)

extern bool verbose;

template <bool TerminateOnFailure, typename ResulT>
inline void validate(ResulT result, const char *message) {
    if (result == ZE_RESULT_SUCCESS) {
        if (verbose) {
            std::cerr << "SUCCESS : " << message << std::endl;
        }
        return;
    }

    if (verbose) {
        std::cerr << (TerminateOnFailure ? "ERROR : " : "WARNING : ") << message << " : " << result
                  << std::endl;
    }

    if (TerminateOnFailure) {
        std::terminate();
    }
}

#define SUCCESS_OR_TERMINATE(CALL) validate<true>(CALL, #CALL)
#define SUCCESS_OR_TERMINATE_BOOL(FLAG) validate<true>(!(FLAG), #FLAG)
#define SUCCESS_OR_WARNING(CALL) validate<false>(CALL, #CALL)
#define SUCCESS_OR_WARNING_BOOL(FLAG) validate<false>(!(FLAG), #FLAG)

bool isParamEnabled(int argc, char *argv[], const char *shortName, const char *longName);

int getParamValue(int argc, char *argv[], const char *shortName, const char *longName, int defaultValue);

bool isCircularDepTest(int argc, char *argv[]);

bool isVerbose(int argc, char *argv[]);

bool isSyncQueueEnabled(int argc, char *argv[]);

bool isAsyncQueueEnabled(int argc, char *argv[]);

bool isAubMode(int argc, char *argv[]);

bool isCommandListShared(int argc, char *argv[]);

bool getAllocationFlag(int argc, char *argv[], int defaultValue);

void selectQueueMode(ze_command_queue_desc_t &desc, bool useSync);

uint32_t getBufferLength(int argc, char *argv[], uint32_t defaultLength);

void printResult(bool aubMode, bool outputValidationSuccessful, const std::string &blackBoxName, const std::string &currentTest);

void printResult(bool aubMode, bool outputValidationSuccessful, const std::string &blackBoxName);

uint32_t getCommandQueueOrdinal(ze_device_handle_t &device);

int32_t getCopyOnlyCommandQueueOrdinal(ze_device_handle_t &device);

ze_command_queue_handle_t createCommandQueue(ze_context_handle_t &context, ze_device_handle_t &device,
                                             uint32_t *ordinal, ze_command_queue_mode_t mode,
                                             ze_command_queue_priority_t priority);

ze_command_queue_handle_t createCommandQueue(ze_context_handle_t &context, ze_device_handle_t &device, uint32_t *ordinal);

ze_result_t createCommandList(ze_context_handle_t &context, ze_device_handle_t &device, ze_command_list_handle_t &cmdList);

void createEventPoolAndEvents(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_event_pool_handle_t &eventPool,
                              ze_event_pool_flag_t poolFlag,
                              uint32_t poolSize,
                              ze_event_handle_t *events,
                              ze_event_scope_flag_t signalScope,
                              ze_event_scope_flag_t waitScope);

std::vector<ze_device_handle_t> zelloGetSubDevices(ze_device_handle_t &device, int &subDevCount);

std::vector<ze_device_handle_t> zelloInitContextAndGetDevices(ze_context_handle_t &context, ze_driver_handle_t &driverHandle);

std::vector<ze_device_handle_t> zelloInitContextAndGetDevices(ze_context_handle_t &context);

void initialize(ze_driver_handle_t &driver, ze_context_handle_t &context, ze_device_handle_t &device, ze_command_queue_handle_t &cmdQueue, uint32_t &ordinal);

void teardown(ze_context_handle_t context, ze_command_queue_handle_t cmdQueue);

void printDeviceProperties(const ze_device_properties_t &props);

void printCacheProperties(uint32_t index, const ze_device_cache_properties_t &props);

void printP2PProperties(const ze_device_p2p_properties_t &props, bool canAccessPeer, uint32_t device0Index, uint32_t device1Index);

void printKernelProperties(const ze_kernel_properties_t &props, const char *kernelName);

const std::vector<const char *> &getResourcesSearchLocations();

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

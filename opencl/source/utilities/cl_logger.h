/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/logger.h"

#include <cinttypes>
#include <cstddef>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace NEO {
struct MultiDispatchInfo;

template <DebugFunctionalityLevel debugLevel>
class ClFileLogger : public NonCopyableAndNonMovableClass {
  public:
    ClFileLogger(FileLogger<debugLevel> &baseLoggerInm, const DebugVariables &flags);

    void dumpKernel(const std::string &name, const std::string &src);
    void logApiCall(const char *function, bool enter, int32_t errorCode);
    size_t getInput(const size_t *input, int32_t index);
    void dumpBinaryProgram(int32_t numDevices, const size_t *lengths, const unsigned char **binaries);

    void dumpKernelArgs(const MultiDispatchInfo *multiDispatchInfo);
    const std::string getEvents(const uintptr_t *input, uint32_t numOfEvents);
    const std::string getMemObjects(const uintptr_t *input, uint32_t numOfObjects);

    std::string getSizes(const uintptr_t *input, uint32_t workDim, bool local) {
        if (false == this->baseLogger.enabled()) {
            return "";
        }

        std::stringstream os;
        std::string workSize;
        if (local) {
            workSize = "localWorkSize";
        } else {
            workSize = "globalWorkSize";
        }

        for (uint32_t i = 0; i < workDim; i++) {
            if (input != nullptr) {
                os << workSize << "[" << i << "]: \t" << input[i] << "\n";
            }
        }
        return os.str();
    }

    std::string infoPointerToString(const void *paramValue, size_t paramSize) {
        if (false == this->baseLogger.enabled()) {
            return "";
        }

        std::stringstream os;
        if (paramValue) {
            switch (paramSize) {
            case sizeof(uint32_t):
                os << readValue<uint32_t>(paramValue);
                break;
            case sizeof(uint64_t):
                os << readValue<uint64_t>(paramValue);
                break;
            case sizeof(uint8_t):
                os << static_cast<uint32_t>(readValue<uint8_t>(paramValue));
                break;
            default:
                break;
            }
        }
        return os.str();
    }

    // Expects pairs of args (even number of args)
    template <typename... Types>
    void logInputs(const Types &...params) {
        if (this->baseLogger.enabled()) {
            if (this->logApiCalls) {
                std::thread::id thisThread = std::this_thread::get_id();
                std::stringstream ss;
                ss << "------------------------------\n";
                this->printInputs(ss, "ThreadID", thisThread, params...);
                ss << "------------------------------" << std::endl;

                this->baseLogger.logDebugString(true, ss.str());
            }
        }
    }

    template <typename FT>
    void logLazyEvaluateArgs(bool predicate, FT &&callable) {
        this->baseLogger.logLazyEvaluateArgs(predicate, std::forward<FT>(callable));
    }

    bool peekLogApiCalls() { return this->logApiCalls; }

  protected:
    template <typename T>
    static T readValue(const void *paramValue) {
        T value{};
        memcpy_s(&value, sizeof(T), paramValue, sizeof(T));
        return value;
    }

    // Required for variadic template with 0 args passed
    void printInputs(std::stringstream &ss) {}

    // Prints inputs in format: InputName: InputValue \newline
    template <typename T1, typename... Types>
    void printInputs(std::stringstream &ss, const T1 &first, const Types &...params) {
        const size_t argsLeft = sizeof...(params);

        ss << "\t" << first;
        if (argsLeft % 2) {
            ss << ": ";
        } else {
            ss << std::endl;
        }
        this->printInputs(ss, params...);
    }

    bool dumpKernels = false;
    bool logApiCalls = false;
    bool dumpKernelArgsEnabled = false;
    FileLogger<debugLevel> &baseLogger;
};

ClFileLogger<globalDebugFunctionalityLevel> &getClFileLogger();

template <bool enabled>
class LoggerApiEnterWrapper {
  public:
    LoggerApiEnterWrapper(const char *funcName, const int *errorCode)
        : funcName(funcName), errorCode(errorCode) {
        if (enabled) {
            getClFileLogger().logApiCall(funcName, true, 0);
        }
    }
    ~LoggerApiEnterWrapper() {
        if (enabled) {
            getClFileLogger().logApiCall(funcName, false, (errorCode != nullptr) ? *errorCode : 0);
        }
    }
    const char *funcName;
    const int *errorCode;
};
}; // namespace NEO

#define DBG_LOG_INPUTS(...) \
    DBG_LOG_LAZY_EVALUATE_ARGS(NEO::getClFileLogger(), NEO::getClFileLogger().peekLogApiCalls(), logInputs, __VA_ARGS__)

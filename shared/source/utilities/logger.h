/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"

#include <cinttypes>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace NEO {
class Kernel;
struct MultiDispatchInfo;
class GraphicsAllocation;

const char *getAllocationTypeString(GraphicsAllocation const *graphicsAllocation);
const char *getMemoryPoolString(GraphicsAllocation const *graphicsAllocation);

template <DebugFunctionalityLevel DebugLevel>
class FileLogger {
  public:
    FileLogger(std::string filename, const DebugVariables &flags);
    ~FileLogger();

    FileLogger(const FileLogger &) = delete;
    FileLogger &operator=(const FileLogger &) = delete;

    static constexpr bool enabled() {
        return DebugLevel == DebugFunctionalityLevel::Full;
    }

    void dumpKernel(const std::string &name, const std::string &src);
    void logApiCall(const char *function, bool enter, int32_t errorCode);
    void logAllocation(GraphicsAllocation const *graphicsAllocation);
    size_t getInput(const size_t *input, int32_t index);

    MOCKABLE_VIRTUAL void writeToFile(std::string filename, const char *str, size_t length, std::ios_base::openmode mode);

    void dumpBinaryProgram(int32_t numDevices, const size_t *lengths, const unsigned char **binaries);

    const std::string getSizes(const uintptr_t *input, uint32_t workDim, bool local) {
        if (false == enabled()) {
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

    const std::string infoPointerToString(const void *paramValue, size_t paramSize) {
        if (false == enabled()) {
            return "";
        }

        std::stringstream os;
        if (paramValue) {
            switch (paramSize) {
            case sizeof(uint32_t):
                os << *(uint32_t *)paramValue;
                break;
            case sizeof(uint64_t):
                os << *(uint64_t *)paramValue;
                break;
            case sizeof(uint8_t):
                os << (uint32_t)(*(uint8_t *)paramValue);
                break;
            default:
                break;
            }
        }
        return os.str();
    }

    // Expects pairs of args (even number of args)
    template <typename... Types>
    void logInputs(Types &&...params) {
        if (enabled()) {
            if (logApiCalls) {
                std::thread::id thisThread = std::this_thread::get_id();
                std::stringstream ss;
                ss << "------------------------------\n";
                printInputs(ss, "ThreadID", thisThread, params...);
                ss << "------------------------------" << std::endl;
                writeToFile(logFileName, ss.str().c_str(), ss.str().length(), std::ios::app);
            }
        }
    }

    template <typename FT>
    void logLazyEvaluateArgs(bool predicate, FT &&callable) {
        if (enabled()) {
            if (predicate) {
                callable();
            }
        }
    }

    template <typename... Types>
    void log(bool enableLog, Types... params) {
        if (enabled()) {
            if (enableLog) {
                std::thread::id thisThread = std::this_thread::get_id();
                std::stringstream ss;
                print(ss, "ThreadID", thisThread, params...);
                writeToFile(logFileName, ss.str().c_str(), ss.str().length(), std::ios::app);
            }
        }
    }

    const char *getLogFileName() {
        return logFileName.c_str();
    }

    void setLogFileName(std::string filename) {
        logFileName = filename;
    }

    bool peekLogApiCalls() { return logApiCalls; }

  protected:
    std::mutex mutex;
    std::string logFileName;
    bool dumpKernels = false;
    bool logApiCalls = false;
    bool logAllocationMemoryPool = false;
    bool logAllocationType = false;
    bool logAllocationStdout = false;

    // Required for variadic template with 0 args passed
    void printInputs(std::stringstream &ss) {}

    // Prints inputs in format: InputName: InputValue \newline
    template <typename T1, typename... Types>
    void printInputs(std::stringstream &ss, T1 first, Types... params) {
        if (enabled()) {
            const size_t argsLeft = sizeof...(params);

            ss << "\t" << first;
            if (argsLeft % 2) {
                ss << ": ";
            } else {
                ss << std::endl;
            }
            printInputs(ss, params...);
        }
    }

    // Required for variadic template with 0 args passed
    void print(std::stringstream &ss) {}

    template <typename T1, typename... Types>
    void print(std::stringstream &ss, T1 first, Types... params) {
        if (enabled()) {
            const size_t argsLeft = sizeof...(params);

            ss << first << " ";
            if (argsLeft == 0) {
                ss << std::endl;
            }
            print(ss, params...);
        }
    }
};

extern FileLogger<globalDebugFunctionalityLevel> &fileLoggerInstance();

template <bool Enabled>
class LoggerApiEnterWrapper {
  public:
    LoggerApiEnterWrapper(const char *funcName, const int *errorCode)
        : funcName(funcName), errorCode(errorCode) {
        if (Enabled) {
            fileLoggerInstance().logApiCall(funcName, true, 0);
        }
    }
    ~LoggerApiEnterWrapper() {
        if (Enabled) {
            fileLoggerInstance().logApiCall(funcName, false, (errorCode != nullptr) ? *errorCode : 0);
        }
    }
    const char *funcName;
    const int *errorCode;
};

}; // namespace NEO

#define DBG_LOG_LAZY_EVALUATE_ARGS(LOGGER, PREDICATE, LOG_FUNCTION, ...) \
    LOGGER.logLazyEvaluateArgs(PREDICATE, [&] { LOGGER.LOG_FUNCTION(__VA_ARGS__); })

#define DBG_LOG(PREDICATE, ...) \
    DBG_LOG_LAZY_EVALUATE_ARGS(NEO::fileLoggerInstance(), NEO::DebugManager.flags.PREDICATE.get(), log, NEO::DebugManager.flags.PREDICATE.get(), __VA_ARGS__)

#define DBG_LOG_INPUTS(...) \
    DBG_LOG_LAZY_EVALUATE_ARGS(NEO::fileLoggerInstance(), NEO::fileLoggerInstance().peekLogApiCalls(), logInputs, __VA_ARGS__)

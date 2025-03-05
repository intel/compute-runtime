/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <mutex>
#include <sstream>
#include <thread>

#define CREATE_DEBUG_STRING(buffer, format, ...)                            \
    std::unique_ptr<char[]> buffer(new char[NEO::maxErrorDescriptionSize]); \
    snprintf(buffer.get(), NEO::maxErrorDescriptionSize, format, __VA_ARGS__)

namespace NEO {
class Kernel;
struct MultiDispatchInfo;
class GraphicsAllocation;
class MemoryManager;

static const int32_t maxErrorDescriptionSize = 1024;
const char *getAllocationTypeString(GraphicsAllocation const *graphicsAllocation);
const char *getMemoryPoolString(GraphicsAllocation const *graphicsAllocation);

template <DebugFunctionalityLevel debugLevel>
class FileLogger : NEO::NonCopyableAndNonMovableClass {
  public:
    FileLogger(std::string filename, const DebugVariables &flags);
    MOCKABLE_VIRTUAL ~FileLogger();

    static constexpr bool enabled() {
        return debugLevel != DebugFunctionalityLevel::none;
    }

    void dumpKernel(const std::string &name, const std::string &src);
    void logApiCall(const char *function, bool enter, int32_t errorCode);
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
    void logInputs(const Types &...params) {
        if (enabled()) {
            if (logApiCalls) {
                std::thread::id thisThread = std::this_thread::get_id();
                std::stringstream ss;
                ss << "------------------------------\n";
                printInputs(ss, "ThreadID", thisThread, params...);
                ss << "------------------------------" << std::endl;

                const auto str = ss.str();
                writeToFile(logFileName, str.c_str(), str.length(), std::ios::app);
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
    void log(bool enableLog, const Types &...params) {
        if (enabled()) {
            if (enableLog) {
                std::thread::id thisThread = std::this_thread::get_id();
                std::stringstream ss;
                print(ss, "ThreadID", thisThread, params...);

                const auto str = ss.str();
                writeToFile(logFileName, str.c_str(), str.length(), std::ios::app);
            }
        }
    }

    void logDebugString(bool enableLog, std::string_view debugString);

    const char *getLogFileName() { return logFileName.c_str(); }
    std::string getLogFileNameString() { return logFileName; }

    void setLogFileName(std::string filename) { logFileName = std::move(filename); }

    bool peekLogApiCalls() { return logApiCalls; }
    bool shouldLogAllocationType() { return logAllocationType; }
    bool shouldLogAllocationToStdout() { return logAllocationStdout; }
    bool shouldLogAllocationMemoryPool() { return logAllocationMemoryPool; }

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
    void printInputs(std::stringstream &ss, const T1 &first, const Types &...params) {
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
    void print(std::stringstream &ss, const T1 &first, const Types &...params) {
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

static_assert(NEO::NonCopyableAndNonMovable<FileLogger<DebugFunctionalityLevel::none>>);
static_assert(NEO::NonCopyableAndNonMovable<FileLogger<DebugFunctionalityLevel::full>>);
static_assert(NEO::NonCopyableAndNonMovable<FileLogger<DebugFunctionalityLevel::regKeys>>);

extern FileLogger<globalDebugFunctionalityLevel> &fileLoggerInstance();
extern FileLogger<globalDebugFunctionalityLevel> &usmReusePerfLoggerInstance();

template <bool enabled>
class LoggerApiEnterWrapper {
  public:
    LoggerApiEnterWrapper(const char *funcName, const int *errorCode)
        : funcName(funcName), errorCode(errorCode) {
        if (enabled) {
            fileLoggerInstance().logApiCall(funcName, true, 0);
        }
    }
    ~LoggerApiEnterWrapper() {
        if (enabled) {
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
    DBG_LOG_LAZY_EVALUATE_ARGS(NEO::fileLoggerInstance(), NEO::debugManager.flags.PREDICATE.get(), log, NEO::debugManager.flags.PREDICATE.get(), __VA_ARGS__)

#define DBG_LOG_INPUTS(...) \
    DBG_LOG_LAZY_EVALUATE_ARGS(NEO::fileLoggerInstance(), NEO::fileLoggerInstance().peekLogApiCalls(), logInputs, __VA_ARGS__)

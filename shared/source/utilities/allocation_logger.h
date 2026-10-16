/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/logger.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace NEO {
class GraphicsAllocation;

const char *getAllocationTypeString(GraphicsAllocation const *graphicsAllocation);
const char *getMemoryPoolString(GraphicsAllocation const *graphicsAllocation);

class AllocationSummaryTracker : NEO::NonCopyableAndNonMovableClass {
  public:
    void trackAllocationForSummary(const char *allocTypeName, size_t allocationSize, bool isLocalMemory);
    void trackLiveAllocation(const char *allocTypeName, size_t allocationSize, bool isLocalMemory);
    void untrackLiveAllocation(const char *allocTypeName, size_t allocationSize, bool isLocalMemory);
    void printReport();

  private:
    std::mutex mutex;
    std::unordered_map<std::string, size_t> systemMemoryAllocationSummary;
    std::unordered_map<std::string, size_t> localMemoryAllocationSummary;
    std::unordered_map<std::string, size_t> currentSystemMemoryByType;
    std::unordered_map<std::string, size_t> currentLocalMemoryByType;
    std::unordered_map<std::string, size_t> peakSystemMemoryByType;
    std::unordered_map<std::string, size_t> peakLocalMemoryByType;
    size_t currentTotalSystemMemory = 0;
    size_t currentTotalLocalMemory = 0;
    size_t peakTotalSystemMemory = 0;
    size_t peakTotalLocalMemory = 0;
};

template <DebugFunctionalityLevel debugLevel>
class AllocationLogger : NEO::NonCopyableAndNonMovableClass {
  public:
    AllocationLogger(FileLogger<debugLevel> &fileLoggerIn, const DebugVariables &flags);
    ~AllocationLogger();

    FileLogger<debugLevel> &getFileLogger() { return this->fileLogger; }

    bool shouldLogAllocationType() { return this->logAllocationType; }
    bool shouldLogAllocationToStdout() { return this->logAllocationStdout; }
    bool shouldLogAllocationMemoryPool() { return this->logAllocationMemoryPool; }
    bool shouldLogAllocationSummaryReport() { return this->logAllocationSummaryReport; }

    void trackAllocationForSummary(const char *allocTypeName, size_t allocationSize, bool isLocalMemory) { this->allocationTracker.trackAllocationForSummary(allocTypeName, allocationSize, isLocalMemory); }
    void trackLiveAllocation(const char *allocTypeName, size_t allocationSize, bool isLocalMemory) { this->allocationTracker.trackLiveAllocation(allocTypeName, allocationSize, isLocalMemory); }
    void untrackLiveAllocation(const char *allocTypeName, size_t allocationSize, bool isLocalMemory) { this->allocationTracker.untrackLiveAllocation(allocTypeName, allocationSize, isLocalMemory); }

  protected:
    FileLogger<debugLevel> &fileLogger;
    bool logAllocationMemoryPool = false;
    bool logAllocationType = false;
    bool logAllocationStdout = false;
    bool logAllocationSummaryReport = false;
    AllocationSummaryTracker allocationTracker;
};

static_assert(NEO::NonCopyableAndNonMovable<AllocationLogger<DebugFunctionalityLevel::none>>);
static_assert(NEO::NonCopyableAndNonMovable<AllocationLogger<DebugFunctionalityLevel::full>>);
static_assert(NEO::NonCopyableAndNonMovable<AllocationLogger<DebugFunctionalityLevel::regKeys>>);

AllocationLogger<globalDebugFunctionalityLevel> &allocationLoggerInstance();

} // namespace NEO

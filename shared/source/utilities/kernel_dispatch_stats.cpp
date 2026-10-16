/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/kernel_dispatch_stats.h"

#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/utilities/logger.h"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

namespace NEO {

static std::string toCsvField(const std::string &value) {
    std::string field{"\""};
    for (const auto character : value) {
        if (character == '"') {
            field.push_back('"');
        }
        field.push_back(character);
    }
    field.push_back('"');
    return field;
}

static std::string createCsvRow(const KernelDispatchStats &stats) {
    std::stringstream row;
    row << toCsvField(stats.kernelName)
        << "," << stats.globalWorkSize[0] << "," << stats.globalWorkSize[1] << "," << stats.globalWorkSize[2]
        << "," << stats.localWorkSize[0] << "," << stats.localWorkSize[1] << "," << stats.localWorkSize[2]
        << "," << stats.simdSize
        << "," << stats.numGrfRequired
        << "," << stats.slmInlineSize
        << "," << stats.slmTotalSizePerThreadGroup
        << "," << stats.barrierCount
        << "," << stats.perThreadScratchSize[0] << "," << stats.perThreadScratchSize[1]
        << "," << stats.threadsPerThreadGroup
        << "," << stats.threadGroupCount
        << "," << stats.usesSystolicMode
        << "," << stats.isIndirect;
    return row.str();
}

void KernelDispatchStatsTracker::trackDispatch(const KernelDispatchStats &stats) {
    auto row = createCsvRow(stats);

    std::lock_guard theLock(mutex);
    dispatchCounts[row]++;
}

void KernelDispatchStatsTracker::merge(KernelDispatchStatsTracker &source) {
    auto snapshot = source.takeSnapshot();

    std::lock_guard theLock(mutex);
    for (const auto &[row, dispatchCount] : snapshot) {
        dispatchCounts[row] += dispatchCount;
    }
}

bool KernelDispatchStatsTracker::isEmpty() {
    std::lock_guard theLock(mutex);
    return dispatchCounts.empty();
}

void KernelDispatchStatsTracker::clear() {
    DispatchCounts emptyCounts;

    std::lock_guard theLock(mutex);
    dispatchCounts.swap(emptyCounts);
}

KernelDispatchStatsTracker::DispatchCounts KernelDispatchStatsTracker::takeSnapshot() {
    std::lock_guard theLock(mutex);
    return dispatchCounts;
}

std::string KernelDispatchStatsTracker::createReport() {
    auto snapshot = takeSnapshot();

    std::vector<std::pair<uint64_t, std::string>> rows;
    rows.reserve(snapshot.size());
    for (const auto &[row, dispatchCount] : snapshot) {
        rows.emplace_back(dispatchCount, row);
    }

    std::sort(rows.begin(), rows.end(), [](const auto &lhs, const auto &rhs) {
        if (lhs.first != rhs.first) {
            return lhs.first > rhs.first;
        }
        return lhs.second < rhs.second;
    });

    std::stringstream report;
    report << "kernelName,gwsX,gwsY,gwsZ,lwsX,lwsY,lwsZ,simd,grf,slmInline,slmPerThreadGroup,"
              "barriers,scratchSlot0,scratchSlot1,threadsPerThreadGroup,threadGroupCount,"
              "systolic,indirect,dispatchCount\n";
    for (const auto &[dispatchCount, row] : rows) {
        report << row << "," << dispatchCount << "\n";
    }
    return report.str();
}

std::string getKernelDispatchStatsFileName() {
    return "kernel_dispatch_stats_" + std::to_string(SysCalls::getProcessId()) + ".csv";
}

template <DebugFunctionalityLevel debugLevel>
void writeKernelDispatchStatsReport(FileLogger<debugLevel> &fileLogger, KernelDispatchStatsTracker &tracker) {
    if (!fileLogger.enabled()) {
        return;
    }
    const auto report = tracker.createReport();
    fileLogger.writeToFile(getKernelDispatchStatsFileName(), report.c_str(), report.size(), std::ios::trunc);
}

template void writeKernelDispatchStatsReport<DebugFunctionalityLevel::none>(FileLogger<DebugFunctionalityLevel::none> &fileLogger, KernelDispatchStatsTracker &tracker);
template void writeKernelDispatchStatsReport<DebugFunctionalityLevel::regKeys>(FileLogger<DebugFunctionalityLevel::regKeys> &fileLogger, KernelDispatchStatsTracker &tracker);
template void writeKernelDispatchStatsReport<DebugFunctionalityLevel::full>(FileLogger<DebugFunctionalityLevel::full> &fileLogger, KernelDispatchStatsTracker &tracker);

void collectKernelDispatchStats(KernelDispatchStatsTracker &target, KernelDispatchStatsTracker *commandListStats, bool consumeStats) {
    if (commandListStats == nullptr) {
        return;
    }

    target.merge(*commandListStats);
    if (consumeStats) {
        commandListStats->clear();
    }
}

} // namespace NEO

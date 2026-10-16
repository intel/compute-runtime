/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/kernel_dispatch_stats.h"
#include "shared/test/common/utilities/logger_tests.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <string>

using namespace NEO;

namespace {
KernelDispatchStats createKernelDispatchStats(const std::string &kernelName) {
    KernelDispatchStats stats{};
    stats.kernelName = kernelName;
    stats.globalWorkSize[0] = 256;
    stats.localWorkSize[0] = 64;
    stats.simdSize = 32;
    stats.numGrfRequired = 128;
    stats.slmInlineSize = 1024;
    stats.slmTotalSizePerThreadGroup = 2048;
    stats.barrierCount = 1;
    stats.perThreadScratchSize[0] = 512;
    stats.threadsPerThreadGroup = 2;
    stats.threadGroupCount = 4;
    return stats;
}

size_t countRows(const std::string &report) {
    return static_cast<size_t>(std::count(report.begin(), report.end(), '\n'));
}
} // namespace

TEST(KernelDispatchStatsTracker, givenNoTrackedDispatchesThenTrackerIsEmpty) {
    KernelDispatchStatsTracker tracker;

    EXPECT_TRUE(tracker.isEmpty());

    tracker.trackDispatch(createKernelDispatchStats("myKernel"));

    EXPECT_FALSE(tracker.isEmpty());
}

TEST(KernelDispatchStatsTracker, givenSameShapeTrackedTwiceThenReportHasSingleRowWithDispatchCountTwo) {
    KernelDispatchStatsTracker tracker;

    auto stats = createKernelDispatchStats("myKernel");
    tracker.trackDispatch(stats);
    tracker.trackDispatch(stats);

    const auto report = tracker.createReport();

    EXPECT_EQ(2u, countRows(report));
    EXPECT_NE(std::string::npos, report.find("\"myKernel\",256,0,0,64,0,0,32,128,1024,2048,1,512,0,2,4,0,0,2\n"));
}

TEST(KernelDispatchStatsTracker, givenDifferentShapesOfSameKernelThenReportHasRowPerShape) {
    KernelDispatchStatsTracker tracker;

    auto stats = createKernelDispatchStats("myKernel");
    tracker.trackDispatch(stats);
    stats.localWorkSize[0] = 32;
    tracker.trackDispatch(stats);

    const auto report = tracker.createReport();

    EXPECT_EQ(3u, countRows(report));
    EXPECT_NE(std::string::npos, report.find(",256,0,0,64,0,0,"));
    EXPECT_NE(std::string::npos, report.find(",256,0,0,32,0,0,"));
}

TEST(KernelDispatchStatsTracker, givenTrackedDispatchesWhenCreatingReportThenRowsAreSortedByDispatchCountDescending) {
    KernelDispatchStatsTracker tracker;

    tracker.trackDispatch(createKernelDispatchStats("rareKernel"));
    tracker.trackDispatch(createKernelDispatchStats("hotKernel"));
    tracker.trackDispatch(createKernelDispatchStats("hotKernel"));

    const auto report = tracker.createReport();

    EXPECT_LT(report.find("\"hotKernel\""), report.find("\"rareKernel\""));
}

TEST(KernelDispatchStatsTracker, givenKernelNameWithSpecialCharactersWhenCreatingReportThenNameIsQuotedAndEscaped) {
    KernelDispatchStatsTracker tracker;

    tracker.trackDispatch(createKernelDispatchStats("kernel<int, \"x\">"));

    const auto report = tracker.createReport();

    EXPECT_NE(std::string::npos, report.find("\"kernel<int, \"\"x\"\">\","));
}

TEST(KernelDispatchStatsTracker, givenSourceTrackerWhenMergingThenCountsAreAddedAndSourceIsPreserved) {
    KernelDispatchStatsTracker source;
    KernelDispatchStatsTracker target;

    source.trackDispatch(createKernelDispatchStats("myKernel"));
    source.trackDispatch(createKernelDispatchStats("myKernel"));

    target.merge(source);
    target.merge(source);

    const auto report = target.createReport();
    EXPECT_EQ(2u, countRows(report));
    EXPECT_NE(std::string::npos, report.find("\"myKernel\",256,0,0,64,0,0,32,128,1024,2048,1,512,0,2,4,0,0,4\n"));

    EXPECT_NE(std::string::npos, source.createReport().find("\"myKernel\",256,0,0,64,0,0,32,128,1024,2048,1,512,0,2,4,0,0,2\n"));
}

TEST(KernelDispatchStatsTracker, givenTrackedDispatchesWhenClearedThenReportHasHeaderOnly) {
    KernelDispatchStatsTracker tracker;

    tracker.trackDispatch(createKernelDispatchStats("myKernel"));
    tracker.clear();

    EXPECT_TRUE(tracker.isEmpty());
    EXPECT_EQ(1u, countRows(tracker.createReport()));
}

TEST(KernelDispatchStatsTracker, givenCommandListStatsWhenCollectingWithoutConsumingThenSourceKeepsItsCounts) {
    KernelDispatchStatsTracker collectedStats;
    KernelDispatchStatsTracker commandListStats;
    commandListStats.trackDispatch(createKernelDispatchStats("keptKernel"));

    collectKernelDispatchStats(collectedStats, &commandListStats, false);
    collectKernelDispatchStats(collectedStats, &commandListStats, false);

    EXPECT_FALSE(commandListStats.isEmpty());
    EXPECT_NE(std::string::npos, collectedStats.createReport().find("\"keptKernel\",256,0,0,64,0,0,32,128,1024,2048,1,512,0,2,4,0,0,2\n"));
}

TEST(KernelDispatchStatsTracker, givenCommandListStatsWhenCollectingWithConsumingThenSourceIsCleared) {
    KernelDispatchStatsTracker collectedStats;
    KernelDispatchStatsTracker commandListStats;
    commandListStats.trackDispatch(createKernelDispatchStats("consumedKernel"));

    collectKernelDispatchStats(collectedStats, &commandListStats, true);
    collectKernelDispatchStats(collectedStats, &commandListStats, true);

    EXPECT_TRUE(commandListStats.isEmpty());
    EXPECT_NE(std::string::npos, collectedStats.createReport().find("\"consumedKernel\",256,0,0,64,0,0,32,128,1024,2048,1,512,0,2,4,0,0,1\n"));
}

TEST(KernelDispatchStatsTracker, givenNoCommandListStatsWhenCollectingThenNothingIsReported) {
    KernelDispatchStatsTracker collectedStats;

    collectKernelDispatchStats(collectedStats, nullptr, true);

    EXPECT_TRUE(collectedStats.isEmpty());
}

TEST(KernelDispatchStatsReport, givenDisabledFileLoggerWhenWritingReportThenReportFileIsNotWritten) {
    removeVirtualFile(getKernelDispatchStatsFileName());
    FullyDisabledFileLogger fileLogger(std::string("testfile"));
    KernelDispatchStatsTracker trackedDispatches;
    trackedDispatches.trackDispatch(createKernelDispatchStats("myKernel"));

    writeKernelDispatchStatsReport(fileLogger, trackedDispatches);

    EXPECT_FALSE(virtualFileExists(getKernelDispatchStatsFileName()));
}

TEST(KernelDispatchStatsReport, givenNoTrackedDispatchesWhenWritingReportThenStaleReportIsOverwritten) {
    const auto reportFileName = getKernelDispatchStatsFileName();
    FullyEnabledFileLogger fileLogger(std::string("testfile"));
    KernelDispatchStatsTracker staleRunDispatches;
    staleRunDispatches.trackDispatch(createKernelDispatchStats("staleKernel"));
    writeKernelDispatchStatsReport(fileLogger, staleRunDispatches);
    ASSERT_NE(std::string::npos, NEO::virtualFileList[reportFileName].str().find("\"staleKernel\","));

    KernelDispatchStatsTracker noDispatches;
    writeKernelDispatchStatsReport(fileLogger, noDispatches);

    ASSERT_TRUE(virtualFileExists(reportFileName));
    const auto report = NEO::virtualFileList[reportFileName].str();
    EXPECT_TRUE(report.starts_with("kernelName,"));
    EXPECT_EQ(std::string::npos, report.find("\"staleKernel\","));
    removeVirtualFile(reportFileName);
}

TEST(KernelDispatchStatsReport, givenTrackedKernelDispatchesWhenWritingReportThenReportFileHasHeaderAndRow) {
    removeVirtualFile(getKernelDispatchStatsFileName());
    FullyEnabledFileLogger fileLogger(std::string("testfile"));
    KernelDispatchStatsTracker trackedDispatches;
    trackedDispatches.trackDispatch(createKernelDispatchStats("myKernel"));

    writeKernelDispatchStatsReport(fileLogger, trackedDispatches);

    ASSERT_TRUE(virtualFileExists(getKernelDispatchStatsFileName()));
    const auto report = NEO::virtualFileList[getKernelDispatchStatsFileName()].str();
    EXPECT_TRUE(report.starts_with("kernelName,gwsX,gwsY,gwsZ,lwsX,lwsY,lwsZ,simd,grf,slmInline,slmPerThreadGroup,barriers,scratchSlot0,scratchSlot1,threadsPerThreadGroup,threadGroupCount,systolic,indirect,dispatchCount\n"));
    EXPECT_NE(std::string::npos, report.find("\"myKernel\","));
    removeVirtualFile(getKernelDispatchStatsFileName());
}

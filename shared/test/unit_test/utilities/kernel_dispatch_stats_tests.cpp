/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/kernel_dispatch_stats.h"

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

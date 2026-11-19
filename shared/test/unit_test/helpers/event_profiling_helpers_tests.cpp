/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/event_profiling_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct TimestampBits {
    uint32_t globalBits;
    uint32_t kernelBits;
};

using EventProfilingHelpersWidthTests = ::testing::TestWithParam<TimestampBits>;

INSTANTIATE_TEST_SUITE_P(,
                         EventProfilingHelpersWidthTests,
                         ::testing::Values(TimestampBits{36u, 32u},   // gen12lp
                                           TimestampBits{32u, 32u},   // DG2 / PVC / MTL / ARL
                                           TimestampBits{32u, 64u})); // BMG / LNL / PTL / xe3+

TEST_P(EventProfilingHelpersWidthTests, givenNarrowPacketWhenRestoringHighBitsThenBitsAreTakenFromReference) {
    const auto [globalBits, kernelBits] = GetParam();
    const uint64_t highMask = maxNBitValue(64) - maxNBitValue(globalBits);
    const uint64_t reference = highMask | 0x1000; // bits above globalBits + a low bit the restore must ignore
    const uint64_t rawValue = 0x2000;             // narrow packet: no bits above globalBits

    const uint64_t restored = restoreHighBitsFromReference(rawValue, reference, globalBits);

    EXPECT_EQ(highMask | rawValue, restored);
}

TEST_P(EventProfilingHelpersWidthTests, givenValueThatAlreadyCarriesReferenceHighBitsWhenRestoringThenResultIsUnchanged) {
    const auto [globalBits, kernelBits] = GetParam();
    const uint64_t highMask = maxNBitValue(64) - maxNBitValue(globalBits);
    const uint64_t reference = highMask | 0x1000;
    const uint64_t rawValue = highMask | 0x8000; // full-width packet: high bits already present

    EXPECT_EQ(rawValue, restoreHighBitsFromReference(rawValue, reference, globalBits));
}

TEST_P(EventProfilingHelpersWidthTests, givenNoWrapWhenComputingDeltaThenPlainDifferenceReturned) {
    const auto [globalBits, kernelBits] = GetParam();
    EXPECT_EQ(0x400u, wrapAwareDelta(0x100, 0x500, kernelBits));
}

TEST_P(EventProfilingHelpersWidthTests, givenEndBeforeStartWhenComputingDeltaThenSingleWrapHandled) {
    const auto [globalBits, kernelBits] = GetParam();
    const uint64_t maxKernelTs = maxNBitValue(kernelBits);
    const uint64_t start = maxKernelTs - 0x10;
    const uint64_t end = 0x20;

    EXPECT_EQ(0x30u, wrapAwareDelta(start, end, kernelBits)); // (maxKernelTs - start) + end
}

TEST_P(EventProfilingHelpersWidthTests, givenBitsAboveValidWidthWhenComputingDeltaThenTheyAreMasked) {
    const auto [globalBits, kernelBits] = GetParam();
    // bits above the valid width must be masked off before the delta (aboveMask is 0 when kernelBits == 64)
    const uint64_t aboveMask = maxNBitValue(64) - maxNBitValue(kernelBits);
    const uint64_t start = aboveMask | 0x100;
    const uint64_t end = aboveMask | 0x500;

    EXPECT_EQ(0x400u, wrapAwareDelta(start, end, kernelBits));
}

using EventProfilingHelpersCalcTest = Test<DeviceFixture>;

struct EventProfilingHelpersStartTimestampTest : public EventProfilingHelpersCalcTest {
    void calculate(uint64_t globalStart, uint64_t globalEnd) {
        const uint32_t kernelTimestampValidBits = pDevice->getHardwareInfo().capabilityTable.kernelTimestampValidBits;
        calculateProfilingData(pDevice->getGfxCoreHelper(), osTime, resolution, kernelTimestampValidBits,
                               queue, submit, start, end, complete,
                               globalStart, globalEnd, &contextComplete, globalStart);
    }

    void calculate(uint64_t globalStart) {
        calculate(globalStart, globalStart + 0x100);
    }

    MockOSTime osTime;
    const double resolution = 1.0;
    ProfilingInfo queue{};
    ProfilingInfo submit{};
    ProfilingInfo start{};
    ProfilingInfo end{};
    ProfilingInfo complete{};
    uint64_t contextComplete = 0;
};

TEST_F(EventProfilingHelpersStartTimestampTest, givenUnsetCompleteTimestampWhenCalculatingProfilingDataThenCompleteEqualsEndAndOrderingHolds) {
    submit.cpuTimeInNs = 100;
    submit.gpuTimeStamp = 0x1000;

    const uint64_t contextEnd = 0x1800;
    calculate(0x1500, contextEnd);

    EXPECT_GE(start.gpuTimeStamp, submit.gpuTimeStamp);
    EXPECT_GE(end.gpuTimeStamp, start.gpuTimeStamp);
    EXPECT_EQ(contextEnd, contextComplete); // unset complete -> assigned end
    EXPECT_EQ(end.gpuTimeStamp, complete.gpuTimeStamp);
    EXPECT_EQ(end.cpuTimeInNs, complete.cpuTimeInNs);
}

TEST_F(EventProfilingHelpersStartTimestampTest, givenZeroedSubmitStampWhenCalculatingProfilingDataThenNoUnderflowAndOrderingHolds) {
    calculate(0x1500, 0x1800);

    EXPECT_GE(start.gpuTimeStamp, submit.gpuTimeStamp);
    EXPECT_GE(end.gpuTimeStamp, start.gpuTimeStamp);
    EXPECT_GE(complete.gpuTimeStamp, start.gpuTimeStamp);
}

TEST_F(EventProfilingHelpersStartTimestampTest, givenKernelStartBeforeSubmitWithinRefreshTimeoutWhenCalculatingProfilingDataThenSubmitAndQueueAreAlignedBeforeStart) {
    osTime.deviceTime->refreshTimestamps = false;
    submit.cpuTimeInNs = 100;
    submit.gpuTimeStamp = 0x1001;

    calculate(0x1000);

    EXPECT_EQ(0x1000u, start.gpuTimeStamp);
    EXPECT_EQ(0xFFFu, submit.gpuTimeStamp);
    EXPECT_EQ(0xFFFu, submit.gpuTimeInNs);
    EXPECT_EQ(submit.gpuTimeInNs, submit.cpuTimeInNs);
    EXPECT_EQ(0xFFEu, queue.gpuTimeStamp);
    EXPECT_EQ(0xFFEu, queue.gpuTimeInNs);
    EXPECT_TRUE(osTime.deviceTime->refreshTimestamps);
}

TEST_F(EventProfilingHelpersStartTimestampTest, givenKernelStartBeforeSubmitBeyondRefreshTimeoutWhenCalculatingProfilingDataThenOneTimestampWrapIsAddedToStart) {
    osTime.deviceTime->refreshTimestamps = false;
    const uint64_t globalStart = 0x1000;
    const uint64_t submitGpuTimeStamp = globalStart + osTime.getTimestampRefreshTimeout() + 1;
    submit.cpuTimeInNs = 100;
    submit.gpuTimeStamp = submitGpuTimeStamp;

    calculate(globalStart);

    EXPECT_EQ(globalStart + (1ULL << pDevice->getGfxCoreHelper().getGlobalTimeStampBits()), start.gpuTimeStamp);
    EXPECT_EQ(submitGpuTimeStamp, submit.gpuTimeStamp);
    EXPECT_EQ(0u, queue.gpuTimeStamp);
    EXPECT_FALSE(osTime.deviceTime->refreshTimestamps);
}

TEST_F(EventProfilingHelpersStartTimestampTest, givenStartTimestampAlreadySetOnCpuWhenCalculatingProfilingDataThenStartGpuTimeStampIsNotTakenFromGlobalStart) {
    submit.cpuTimeInNs = 100;
    submit.gpuTimeStamp = 0x1000;
    start.cpuTimeInNs = 555;
    start.gpuTimeStamp = 0x1200;

    calculate(0x1500);

    EXPECT_EQ(0x1200u, start.gpuTimeStamp);
    EXPECT_EQ(submit.cpuTimeInNs + 0x200u, start.cpuTimeInNs);
    EXPECT_EQ(0x1200u, start.gpuTimeInNs);
}

TEST_F(EventProfilingHelpersStartTimestampTest, givenKernelStartNotAboveTwoTicksAndBeforeSubmitWhenCalculatingProfilingDataThenSubmitAndQueueAreZero) {
    osTime.deviceTime->refreshTimestamps = false;
    submit.cpuTimeInNs = 100;
    submit.gpuTimeStamp = 0x10;

    calculate(2);

    EXPECT_EQ(2u, start.gpuTimeStamp);
    EXPECT_EQ(0u, submit.gpuTimeStamp);
    EXPECT_EQ(0u, queue.gpuTimeStamp);
    EXPECT_TRUE(osTime.deviceTime->refreshTimestamps);
}

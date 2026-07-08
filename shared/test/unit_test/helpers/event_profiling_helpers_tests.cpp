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

TEST_F(EventProfilingHelpersCalcTest, givenUnsetCompleteTimestampWhenCalculatingProfilingDataThenCompleteEqualsEndAndOrderingHolds) {
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    MockOSTime osTime;
    const double resolution = 1.0;
    const uint32_t kernelTimestampValidBits = pDevice->getHardwareInfo().capabilityTable.kernelTimestampValidBits;

    ProfilingInfo queue{};
    ProfilingInfo submit{};
    ProfilingInfo start{};
    ProfilingInfo end{};
    ProfilingInfo complete{};

    submit.cpuTimeInNs = 100;
    submit.gpuTimeStamp = 0x1000;

    const uint64_t contextStart = 0x1500;
    const uint64_t contextEnd = 0x1800;
    uint64_t contextComplete = 0;

    calculateProfilingData(gfxCoreHelper, osTime, resolution, kernelTimestampValidBits,
                           queue, submit, start, end, complete,
                           contextStart, contextEnd, &contextComplete, contextStart);

    EXPECT_GE(start.gpuTimeStamp, submit.gpuTimeStamp);
    EXPECT_GE(end.gpuTimeStamp, start.gpuTimeStamp);
    EXPECT_EQ(contextEnd, contextComplete); // unset complete -> assigned end
    EXPECT_EQ(end.gpuTimeStamp, complete.gpuTimeStamp);
    EXPECT_EQ(end.cpuTimeInNs, complete.cpuTimeInNs);
}

TEST_F(EventProfilingHelpersCalcTest, givenZeroedSubmitStampWhenCalculatingProfilingDataThenNoUnderflowAndOrderingHolds) {
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    MockOSTime osTime;
    const double resolution = 1.0;
    const uint32_t kernelTimestampValidBits = pDevice->getHardwareInfo().capabilityTable.kernelTimestampValidBits;

    ProfilingInfo queue{};
    ProfilingInfo submit{};
    ProfilingInfo start{};
    ProfilingInfo end{};
    ProfilingInfo complete{};

    const uint64_t contextStart = 0x1500;
    const uint64_t contextEnd = 0x1800;
    uint64_t contextComplete = 0;

    calculateProfilingData(gfxCoreHelper, osTime, resolution, kernelTimestampValidBits,
                           queue, submit, start, end, complete,
                           contextStart, contextEnd, &contextComplete, contextStart);

    EXPECT_GE(start.gpuTimeStamp, submit.gpuTimeStamp);
    EXPECT_GE(end.gpuTimeStamp, start.gpuTimeStamp);
    EXPECT_GE(complete.gpuTimeStamp, start.gpuTimeStamp);
}

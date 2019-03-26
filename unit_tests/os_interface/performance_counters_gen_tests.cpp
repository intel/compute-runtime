/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/options.h"
#include "runtime/os_interface/performance_counters.h"
#include "test.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_ostime.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

using namespace NEO;

struct PerformanceCountersGenTest : public ::testing::Test {
};

namespace NEO {
extern decltype(&instrGetPerfCountersQueryData) getPerfCountersQueryDataFactory[IGFX_MAX_CORE];
extern size_t perfCountersQuerySize[IGFX_MAX_CORE];
} // namespace NEO

class MockPerformanceCountersGen : public PerformanceCounters {
  public:
    MockPerformanceCountersGen(OSTime *osTime) : PerformanceCounters(osTime) {
    }

    decltype(&instrGetPerfCountersQueryData) getFn() const {
        return getPerfCountersQueryDataFunc;
    }
};

HWTEST_F(PerformanceCountersGenTest, givenPerfCountersWhenInitializedWithoutGenSpecificThenDefaultFunctionIsUsed) {
    auto gfxCore = platformDevices[0]->pPlatform->eRenderCoreFamily;

    VariableBackup<decltype(&instrGetPerfCountersQueryData)> bkp(&getPerfCountersQueryDataFactory[gfxCore], nullptr);

    MockOSTime osTime;
    std::unique_ptr<MockPerformanceCountersGen> counters(new MockPerformanceCountersGen(&osTime));
    ASSERT_NE(nullptr, counters.get());

    counters->initialize(platformDevices[0]);
    EXPECT_EQ(counters->getFn(), &instrGetPerfCountersQueryData);

    size_t expected = sizeof(GTDI_QUERY);
    EXPECT_EQ(expected, perfCountersQuerySize[gfxCore]);
}

HWTEST_F(PerformanceCountersGenTest, givenPerfCountersWhenInitializedWithGenSpecificThenGenFunctionIsUsed) {
    VariableBackup<decltype(&instrGetPerfCountersQueryData)> bkp(&getPerfCountersQueryDataFactory[platformDevices[0]->pPlatform->eRenderCoreFamily]);

    auto mockFn = [](
                      InstrEscCbData cbData,
                      GTDI_QUERY *pData,
                      HwPerfCounters *pLayout,
                      uint64_t cpuRawTimestamp,
                      void *pASInterface,
                      InstrPmRegsCfg *pPmRegsCfg,
                      bool useMiRPC,
                      bool resetASData = false,
                      const InstrAllowedContexts *pAllowedContexts = nullptr) -> void {
    };

    bkp = mockFn;

    MockOSTime osTime;
    std::unique_ptr<MockPerformanceCountersGen> counters(new MockPerformanceCountersGen(&osTime));
    ASSERT_NE(nullptr, counters.get());

    counters->initialize(platformDevices[0]);
    EXPECT_EQ(counters->getFn(), mockFn);
}

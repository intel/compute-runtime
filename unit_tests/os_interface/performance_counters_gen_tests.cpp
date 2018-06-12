/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/helpers/options.h"
#include "runtime/os_interface/performance_counters.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_ostime.h"
#include "unit_tests/os_interface/mock_performance_counters.h"
#include "test.h"

using namespace OCLRT;

struct PerformanceCountersGenTest : public ::testing::Test {
};

namespace OCLRT {
extern decltype(&instrGetPerfCountersQueryData) getPerfCountersQueryDataFactory[IGFX_MAX_CORE];
extern size_t perfCountersQuerySize[IGFX_MAX_CORE];
} // namespace OCLRT

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

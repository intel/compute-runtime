/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/test/unit_tests/sources/metrics/mock_metric_oa.h"

#include "gtest/gtest.h"

namespace L0 {

extern _ze_driver_handle_t *globalDriverHandle;
namespace ult {

class MockOsLibrary : public NEO::OsLibrary {
  public:
    MockOsLibrary(const std::string &name, std::string *errorValue) {
    }

    void *getProcAddress(const std::string &procName) override {
        return nullptr;
    }

    bool isLoaded() override {
        return false;
    }

    std::string getFullPath() override {
        return std::string();
    }

    static OsLibrary *load(const std::string &name) {
        auto ptr = new (std::nothrow) MockOsLibrary(name, nullptr);
        if (ptr == nullptr) {
            return nullptr;
        }
        return ptr;
    }
};

using MetricInitializationTest = Test<MetricContextFixture>;

TEST_F(MetricInitializationTest, GivenOaDependenciesAreAvailableThenMetricInitializationIsSuccess) {

    globalDriverHandle = static_cast<_ze_driver_handle_t *>(driverHandle.get());
    OaMetricSourceImp::osLibraryLoadFunction = MockOsLibrary::load;
    EXPECT_EQ(device->getMetricDeviceContext().enableMetricApi(), ZE_RESULT_SUCCESS);
    OaMetricSourceImp::osLibraryLoadFunction = NEO::OsLibrary::load;
}

} // namespace ult
} // namespace L0

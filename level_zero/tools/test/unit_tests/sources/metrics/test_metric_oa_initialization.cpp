/*
 * Copyright (C) 2022-2025 Intel Corporation
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

namespace ult {

class MockOsLibrary : public NEO::OsLibrary {
  public:
    MockOsLibrary() {
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

    static OsLibrary *load(const OsLibraryCreateProperties &properties) {
        auto ptr = new (std::nothrow) MockOsLibrary();
        if (ptr == nullptr) {
            return nullptr;
        }
        return ptr;
    }
};

using MetricInitializationTest = Test<MetricContextFixture>;

TEST_F(MetricInitializationTest, GivenOaDependenciesAreAvailableThenMetricInitializationIsSuccess) {

    globalDriverHandles->push_back(driverHandle.get());
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    EXPECT_EQ(device->getMetricDeviceContext().enableMetricApi(), ZE_RESULT_SUCCESS);
    globalDriverHandles->clear();
}

} // namespace ult
} // namespace L0

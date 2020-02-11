/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/execution_environment.h"
#include "core/os_interface/linux/drm_neo.h"
#include "core/os_interface/os_library.h"
#include "core/unit_tests/helpers/default_hw_info.inl"
#include "core/unit_tests/helpers/ult_hw_config.inl"
#include "test.h"
#include "unit_tests/custom_event_listener.h"

using namespace NEO;

int main(int argc, char **argv) {
    bool useDefaultListener = false;

    ::testing::InitGoogleTest(&argc, argv);

    for (int i = 1; i < argc; ++i) {
        if (!strcmp("--disable_default_listener", argv[i])) {
            useDefaultListener = false;
        } else if (!strcmp("--enable_default_listener", argv[i])) {
            useDefaultListener = true;
        }
    }

    if (useDefaultListener == false) {
        auto &listeners = ::testing::UnitTest::GetInstance()->listeners();
        auto defaultListener = listeners.default_result_printer();
        auto customEventListener = new CCustomEventListener(defaultListener);

        listeners.Release(defaultListener);
        listeners.Append(customEventListener);
    }

    auto retVal = RUN_ALL_TESTS();

    return retVal;
}

class DrmWrap : Drm {
  public:
    static Drm *createDrm(RootDeviceEnvironment &rootDeviceEnvironment) {
        return Drm::create(std::make_unique<HwDeviceId>(0), rootDeviceEnvironment);
    }
};

TEST(Drm, createReturnsNull) {
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto ptr = DrmWrap::createDrm(*executionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_EQ(ptr, nullptr);
}

/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/drm_neo.h"
#include "core/os_interface/os_library.h"
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
    static Drm *createDrm(int32_t deviceOrdinal) {
        return Drm::create(deviceOrdinal);
    }
    static void closeDeviceDrm(int32_t deviceOrdinal) {
        closeDevice(deviceOrdinal);
    }
};

TEST(Drm, getReturnsNull) {
    auto ptr = Drm::get(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST(Drm, createReturnsNull) {
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST(Drm, closeDeviceReturnsNone) {
    auto retNone = true;
    DrmWrap::closeDeviceDrm(0);
    EXPECT_EQ(retNone, true);
}

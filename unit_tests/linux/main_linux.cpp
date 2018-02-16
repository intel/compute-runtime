/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/os_library.h"
#include "unit_tests/custom_event_listener.h"
#include "test.h"

using namespace OCLRT;

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

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

#include "mock_os_layer.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/os_interface/linux/allocator_helper.h"
#include "unit_tests/custom_event_listener.h"
#include "test.h"

using namespace OCLRT;

class DrmWrap : public Drm {
  public:
    static Drm *createDrm(int32_t deviceOrdinal) {
        return Drm::create(deviceOrdinal);
    }
    static void closeDevice(int32_t deviceOrdinal) {
        Drm::closeDevice(deviceOrdinal);
    };
};

class DrmTestsFixture {
  public:
    void SetUp() {
        //make static things into initial state
        DrmWrap::closeDevice(0);
        resetOSMockGlobalState();
    }

    void TearDown() {
    }
};

typedef Test<DrmTestsFixture> DrmTests;

TEST_F(DrmTests, getReturnsNull) {
    auto ptr = Drm::get(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, getNoOverrun) {
    //negative device ordinal
    auto ptr = Drm::get(-1);
    EXPECT_EQ(ptr, nullptr);

    //some high value
    ptr = Drm::get(1 << (sizeof(int32_t) * 8 - 2));
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, closeNotOpened) {
    auto ptr = DrmWrap::get(0);
    EXPECT_EQ(ptr, nullptr);

    DrmWrap::closeDevice(0);

    DrmWrap::get(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, openClose) {
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_NE(ptr, nullptr);

    DrmWrap::closeDevice(0);

    ptr = DrmWrap::get(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, closeNoOverrun) {
    //negative device ordinal
    DrmWrap::closeDevice(-1);

    //some high value
    DrmWrap::closeDevice(1 << (sizeof(int32_t) * 8 - 2));
}

TEST_F(DrmTests, createReturnsDrm) {
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_NE(ptr, nullptr);

    drm_i915_getparam_t getParam;
    int lDeviceId;

    ioctlCnt = 0;
    ioctlSeq[0] = -1;
    errno = EINTR;
    // check if device works, although there was EINTR error from KMD
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &lDeviceId;
    auto ret = ptr->ioctl(DRM_IOCTL_I915_GETPARAM, &getParam);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(deviceId, lDeviceId);

    ioctlCnt = 0;
    ioctlSeq[0] = -1;
    errno = EAGAIN;
    // check if device works, although there was EAGAIN error from KMD
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &lDeviceId;
    ret = ptr->ioctl(DRM_IOCTL_I915_GETPARAM, &getParam);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(deviceId, lDeviceId);

    ioctlCnt = 0;
    ioctlSeq[0] = -1;
    errno = 0;
    // we failed with any other error code
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &lDeviceId;
    ret = ptr->ioctl(DRM_IOCTL_I915_GETPARAM, &getParam);
    EXPECT_EQ(-1, ret);
    EXPECT_EQ(deviceId, lDeviceId);
}

TEST_F(DrmTests, createTwiceReturnsSameDrm) {
    auto ptr1 = DrmWrap::createDrm(0);
    EXPECT_NE(ptr1, nullptr);
    auto ptr2 = DrmWrap::createDrm(0);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr1, ptr2);
}

TEST_F(DrmTests, createDriFallback) {
    haveDri = 1;
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_NE(ptr, nullptr);
}

TEST_F(DrmTests, createNoDevice) {
    haveDri = -1;
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, createNoOverrun) {
    auto ptr = DrmWrap::createDrm(-1);
    EXPECT_EQ(ptr, nullptr);

    ptr = DrmWrap::createDrm(1 << (sizeof(int32_t) * 8 - 2));
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, createUnknownDevice) {
    deviceId = -1;

    auto ptr = DrmWrap::createDrm(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, createNoSoftPin) {
    haveSoftPin = 0;

    auto ptr = DrmWrap::createDrm(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, failOnDeviceId) {
    failOnDeviceId = -1;

    auto ptr = DrmWrap::createDrm(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, failOnRevisionId) {
    failOnRevisionId = -1;

    auto ptr = DrmWrap::createDrm(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, failOnSoftPin) {
    failOnSoftPin = -1;

    auto ptr = DrmWrap::createDrm(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, failOnParamBoost) {
    failOnParamBoost = -1;

    auto ptr = DrmWrap::createDrm(0);
    //non-fatal error - issue warning only
    EXPECT_NE(ptr, nullptr);
}

TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenDeviceLowerThenGen9IsCreatedThenSimplifiedMocsSelectionIsFalse) {
    deviceId = IBDW_GT3_WRK_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_NE(ptr, nullptr);
    EXPECT_FALSE(Gmm::useSimplifiedMocsTable);
}

TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenDeviceIsNewerThenGen9IsCreatedThenSimplifiedMocsSelectionIsTrue) {
    Gmm::useSimplifiedMocsTable = false;
    deviceId = IBXT_X_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_NE(ptr, nullptr);
    EXPECT_TRUE(Gmm::useSimplifiedMocsTable);
    Gmm::useSimplifiedMocsTable = false;
}

TEST_F(DrmTests, givenKernelSupportingTurboPatchWhenDeviceIsCreatedThenSimplifiedMocsSelectionIsFalse) {
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_NE(ptr, nullptr);
    EXPECT_FALSE(Gmm::useSimplifiedMocsTable);
}

#if defined(I915_PARAM_HAS_PREEMPTION)
TEST_F(DrmTests, checkPreemption) {
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_NE(ptr, nullptr);
    bool ret = ptr->hasPreemption();
    EXPECT_EQ(ret, true);
    DrmWrap::closeDevice(0);

    ptr = DrmWrap::get(0);
    EXPECT_EQ(ptr, nullptr);
}
#endif

TEST_F(DrmTests, failOnContextCreate) {
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_NE(ptr, nullptr);
    failOnContextCreate = -1;
    bool ret = ptr->hasPreemption();
    EXPECT_EQ(ret, false);
    failOnContextCreate = 0;
    DrmWrap::closeDevice(0);

    ptr = DrmWrap::get(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, failOnSetPriority) {
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_NE(ptr, nullptr);
    failOnSetPriority = -1;
    bool ret = ptr->hasPreemption();
    EXPECT_EQ(ret, false);
    failOnSetPriority = 0;
    DrmWrap::closeDevice(0);

    ptr = DrmWrap::get(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, failOnDrmGetVersion) {
    failOnDrmVersion = -1;
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_EQ(ptr, nullptr);
    failOnDrmVersion = 0;
    DrmWrap::closeDevice(0);

    ptr = DrmWrap::get(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmTests, failOnInvalidDeviceName) {
    strcpy(providedDrmVersion, "NA");
    auto ptr = DrmWrap::createDrm(0);
    EXPECT_EQ(ptr, nullptr);
    failOnDrmVersion = 0;
    strcpy(providedDrmVersion, "i915");
    DrmWrap::closeDevice(0);

    ptr = DrmWrap::get(0);
    EXPECT_EQ(ptr, nullptr);
}

TEST(AllocatorHelper, givenExpectedSizeToMapWhenGetSizetoMapCalledThenExpectedValueReturned) {
    EXPECT_EQ((alignUp(4 * GB - 8096, 4096)), OCLRT::getSizeToMap());
}

int main(int argc, char **argv) {
    bool useDefaultListener = false;

    ::testing::InitGoogleTest(&argc, argv);

    // parse remaining args assuming they're mine
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

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
#include "runtime/os_interface/linux/drm_null_device.h"
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

class DrmNullDeviceTestsFixture {
  public:
    void SetUp() {
        oldFlag = DebugManager.flags.EnableNullHardware.get();

        // Make global staff into init state
        DrmWrap::closeDevice(0);
        resetOSMockGlobalState();

        // Create nullDevice drm
        DebugManager.flags.EnableNullHardware.set(true);
        DrmWrap::createDrm(0);

        // Obtain nullDevice drm
        drmNullDevice = DrmWrap::get(0);
        ASSERT_NE(drmNullDevice, nullptr);
    }

    void TearDown() {
        // Close drm
        DrmWrap::closeDevice(0);
        DebugManager.flags.EnableNullHardware.set(oldFlag);
    }

    Drm *drmNullDevice;

  protected:
    bool oldFlag;
};

typedef Test<DrmNullDeviceTestsFixture> DrmNullDeviceTests;

TEST_F(DrmNullDeviceTests, GIVENdrmNullDeviceWHENcallGetDeviceIdTHENreturnProperDeviceId) {
    int deviceId = 0;
    int ret = drmNullDevice->getDeviceID(deviceId);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(deviceDescriptorTable[0].deviceId, deviceId);
}

TEST_F(DrmNullDeviceTests, GIVENdrmNullDeviceWHENcallIoctlTHENalwaysSuccess) {
    EXPECT_EQ(drmNullDevice->ioctl(0, nullptr), 0);
}

TEST_F(DrmNullDeviceTests, GIVENdrmNullDeviceWHENregReadOtherThenTimestampReadTHENalwaysSuccess) {
    struct drm_i915_reg_read arg;

    arg.offset = 0;
    ASSERT_EQ(drmNullDevice->ioctl(DRM_IOCTL_I915_REG_READ, &arg), 0);
}

TEST_F(DrmNullDeviceTests, GIVENdrmNullDeviceWHENgetGpuTimestamp32bOr64bTHENerror) {
    struct drm_i915_reg_read arg;

    arg.offset = TIMESTAMP_LOW_REG;
    ASSERT_EQ(drmNullDevice->ioctl(DRM_IOCTL_I915_REG_READ, &arg), -1);

    arg.offset = TIMESTAMP_HIGH_REG;
    ASSERT_EQ(drmNullDevice->ioctl(DRM_IOCTL_I915_REG_READ, &arg), -1);
}

TEST_F(DrmNullDeviceTests, GIVENdrmNullDeviceWHENgetGpuTimestamp36bTHENproperValues) {
    struct drm_i915_reg_read arg;

    arg.offset = TIMESTAMP_LOW_REG | 1;
    ASSERT_EQ(drmNullDevice->ioctl(DRM_IOCTL_I915_REG_READ, &arg), 0);
    EXPECT_EQ(arg.val, 1000ULL);

    ASSERT_EQ(drmNullDevice->ioctl(DRM_IOCTL_I915_REG_READ, &arg), 0);
    EXPECT_EQ(arg.val, 2000ULL);

    ASSERT_EQ(drmNullDevice->ioctl(DRM_IOCTL_I915_REG_READ, &arg), 0);
    EXPECT_EQ(arg.val, 3000ULL);
}

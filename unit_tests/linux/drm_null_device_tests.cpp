/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_null_device.h"
#include "test.h"

#include "mock_os_layer.h"

using namespace NEO;

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

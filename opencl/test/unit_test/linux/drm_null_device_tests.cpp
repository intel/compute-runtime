/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/drm_null_device.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/linux/drm_wrap.h"
#include "opencl/test/unit_test/linux/mock_os_layer.h"

#include <memory>

using namespace NEO;

extern const DeviceDescriptor NEO::deviceDescriptorTable[];

class DrmNullDeviceTestsFixture {
  public:
    void setUp() {
        if (deviceDescriptorTable[0].deviceId == 0) {
            GTEST_SKIP();
        }

        // Create nullDevice drm
        DebugManager.flags.EnableNullHardware.set(true);
        executionEnvironment.prepareRootDeviceEnvironments(1);

        drmNullDevice = DrmWrap::createDrm(*executionEnvironment.rootDeviceEnvironments[0]);
        ASSERT_NE(drmNullDevice, nullptr);
    }

    void tearDown() {
    }

    std::unique_ptr<DrmWrap, std::function<void(Drm *)>> drmNullDevice;
    ExecutionEnvironment executionEnvironment;

  protected:
    DebugManagerStateRestore dbgRestorer;
};

typedef Test<DrmNullDeviceTestsFixture> DrmNullDeviceTests;

TEST_F(DrmNullDeviceTests, GivenDrmNullDeviceWhenCallGetDeviceIdThenReturnProperDeviceId) {
    int ret = drmNullDevice->queryDeviceIdAndRevision();
    EXPECT_TRUE(ret);
    auto hwInfo = drmNullDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    EXPECT_EQ(deviceId, hwInfo->platform.usDeviceID);
    EXPECT_EQ(revisionId, hwInfo->platform.usRevId);
}

TEST_F(DrmNullDeviceTests, GivenDrmNullDeviceWhenCallIoctlThenAlwaysSuccess) {
    EXPECT_EQ(drmNullDevice->ioctl(DrmIoctl::GemExecbuffer2, nullptr), 0);
}

TEST_F(DrmNullDeviceTests, GivenDrmNullDeviceWhenRegReadOtherThenTimestampReadThenAlwaysSuccessIsReturned) {
    RegisterRead arg;

    arg.offset = 0;
    ASSERT_EQ(drmNullDevice->ioctl(DrmIoctl::RegRead, &arg), 0);
}

TEST_F(DrmNullDeviceTests, GivenDrmNullDeviceWhenGetGpuTimestamp32bOr64bThenErrorIsReturned) {
    RegisterRead arg;

    arg.offset = REG_GLOBAL_TIMESTAMP_LDW;
    ASSERT_EQ(drmNullDevice->ioctl(DrmIoctl::RegRead, &arg), -1);

    arg.offset = REG_GLOBAL_TIMESTAMP_UN;
    ASSERT_EQ(drmNullDevice->ioctl(DrmIoctl::RegRead, &arg), -1);
}

TEST_F(DrmNullDeviceTests, GivenDrmNullDeviceWhenGetGpuTimestamp36bThenProperValuesAreReturned) {
    RegisterRead arg;

    arg.offset = REG_GLOBAL_TIMESTAMP_LDW | 1;
    ASSERT_EQ(drmNullDevice->ioctl(DrmIoctl::RegRead, &arg), 0);
    EXPECT_EQ(arg.value, 1000ULL);

    ASSERT_EQ(drmNullDevice->ioctl(DrmIoctl::RegRead, &arg), 0);
    EXPECT_EQ(arg.value, 2000ULL);

    ASSERT_EQ(drmNullDevice->ioctl(DrmIoctl::RegRead, &arg), 0);
    EXPECT_EQ(arg.value, 3000ULL);
}

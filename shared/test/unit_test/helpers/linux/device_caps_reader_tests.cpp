/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/linux/device_caps_reader.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

class DeviceCapsReaderDrmTest : public ::testing::Test {
  public:
    DeviceCapsReaderDrmTest() {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmMock> drm;
};

TEST_F(DeviceCapsReaderDrmTest, givenDrmReaderWhenCreateIsCalledThenAReaderIsReturned) {
    struct MyMockIoctlHelper : MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

        std::optional<std::vector<uint32_t>> queryDeviceCaps() override {
            return std::vector<uint32_t>();
        }
    };

    drm->ioctlHelper = std::make_unique<MyMockIoctlHelper>(*drm);

    auto capsReader = DeviceCapsReaderDrm::create(*drm);
    EXPECT_NE(nullptr, capsReader);
}

TEST_F(DeviceCapsReaderDrmTest, givenDrmReaderAndFailedIoctlWhenCreateIsCalledThenNullptrIsReturned) {
    struct MyMockIoctlHelper : MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

        std::optional<std::vector<uint32_t>> queryDeviceCaps() override {
            return std::nullopt;
        }
    };

    drm->ioctlHelper = std::make_unique<MyMockIoctlHelper>(*drm);

    auto capsReader = DeviceCapsReaderDrm::create(*drm);
    EXPECT_EQ(nullptr, capsReader);
}

TEST_F(DeviceCapsReaderDrmTest, givenDrmReaderWhenIndexOperatorIsUsedThenReadFromBuffer) {
    struct MyMockIoctlHelper : MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

        std::optional<std::vector<uint32_t>> queryDeviceCaps() override {
            return std::vector<uint32_t>(1, 123);
        }
    };

    drm->ioctlHelper = std::make_unique<MyMockIoctlHelper>(*drm);

    auto capsReader = DeviceCapsReaderDrm::create(*drm);
    EXPECT_NE(nullptr, capsReader);

    auto val = (*capsReader)[0];
    EXPECT_EQ(123u, val);

    val = (*capsReader)[1];
    EXPECT_EQ(0u, val);
}

TEST_F(DeviceCapsReaderDrmTest, givenDrmReaderWhenGettingOffsetThenZeroIsReturned) {
    struct MyMockIoctlHelper : MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

        std::optional<std::vector<uint32_t>> queryDeviceCaps() override {
            return std::vector<uint32_t>();
        }
    };

    drm->ioctlHelper = std::make_unique<MyMockIoctlHelper>(*drm);

    auto capsReader = DeviceCapsReaderDrm::create(*drm);
    EXPECT_NE(nullptr, capsReader);

    auto val = capsReader->getOffset();
    EXPECT_EQ(0u, val);
}

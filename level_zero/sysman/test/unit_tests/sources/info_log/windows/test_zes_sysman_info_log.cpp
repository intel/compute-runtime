/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/windows/sysman_os_info_log_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_driver.h"

namespace L0 {
namespace Sysman {
namespace ult {

static constexpr uint32_t handleCount = 1u;
class SysmanInfoLogFixture : public SysmanDriverHandleTest {
  protected:
    void SetUp() override {
        SysmanDriverHandleTest::SetUp();
    }

    void TearDown() override {
        SysmanDriverHandleTest::TearDown();
    }

    std::vector<zes_intel_info_log_handle_t> getInfoLogHandles(uint32_t count) {
        std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanInfoLogFixture, GivenValidDriverHandleWhenEnumeratingInfoLogsThenSuccessAndOneCountAreReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(1u, count);
}

TEST_F(SysmanInfoLogFixture, GivenDriverWhenEnumeratingInfoLogsThenHandleCreationSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);

    std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()));

    auto hInfoLog = handles[0];
    EXPECT_NE(nullptr, hInfoLog);
}

TEST_F(SysmanInfoLogFixture, GivenRequestedInfoLogCountGreaterThanOneWhenEnumeratingInfoLogsThenOneHandleCountIsReturned) {

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);

    count = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingGetPropertiesApiThenSuccessIsReturned) {
    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogGetPropertiesExp(infoLogHandles[0], &properties));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingReadApiThenUnsupportedFeatureIsReturned) {
    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogReadExp(infoLogHandles[0], &size, nullptr));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingEnableApiThenUnsupportedFeatureIsReturned) {
    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogEnableExp(infoLogHandles[0], true));
}

TEST_F(SysmanInfoLogFixture, GivenNullOsSysmanDriverWhenEnumeratingInfoLogsThenErrorIsReturned) {
    auto *originalOsSysmanDriver = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = nullptr;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, driverHandle->enumInfoLogs(&count, nullptr));

    driverHandle->pOsSysmanDriver = originalOsSysmanDriver;
}

} // namespace ult
} // namespace Sysman
} // namespace L0

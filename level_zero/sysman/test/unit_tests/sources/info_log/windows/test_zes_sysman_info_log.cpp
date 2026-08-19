/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/sysman_info_log_imp.h"
#include "level_zero/sysman/source/api/info_log/windows/sysman_os_info_log_imp.h"
#include "level_zero/sysman/source/driver/os_sysman_driver.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_driver.h"

namespace L0 {
namespace Sysman {
namespace ult {

static constexpr uint32_t expectedInfoLogHandleCount = 1u;

struct MockOsSysmanDriver : public OsSysmanDriver {
    MockOsSysmanDriver() {
        context.supportedFormats = {ZES_INTEL_INFO_LOG_FORMAT_CPER};
    }
    ze_result_t eventsListen(uint64_t, uint32_t, zes_device_handle_t *, uint32_t *, zes_event_type_flags_t *) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t driverEventsListen(uint64_t, uint32_t, zes_device_handle_t *, uint32_t *, zes_event_type_flags_t *, zes_event_type_flags_t *) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t driverEventRegister(zes_event_type_flags_t) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t enumInfoLogs(uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) override {
        return context.infoLogGet(pCount, phInfoLogs);
    }
    ze_result_t rescanDevices(SysmanDriverHandleImp *, uint32_t *, zes_device_handle_t *) override {
        return ZE_RESULT_SUCCESS;
    }
    InfoLogHandleContext context;
};

struct MockInfoLog : public InfoLog {
    ze_result_t infoLogGetProperties(zes_intel_info_log_properties_exp_t *) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t infoLogRead(uint32_t *, uint8_t *) override {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t infoLogEnable(bool state) override {
        enableCallCount++;
        lastRequestedState = state;
        return enableReturnValue;
    }
    uint32_t enableCallCount = 0;
    bool lastRequestedState = true;
    ze_result_t enableReturnValue = ZE_RESULT_SUCCESS;
};

class SysmanInfoLogFixture : public SysmanDriverHandleTest {
  protected:
    void SetUp() override {
        SysmanDriverHandleTest::SetUp();
    }

    void TearDown() override {
        SysmanDriverHandleTest::TearDown();
    }
};

TEST_F(SysmanInfoLogFixture, GivenInfoLogImpWhenCallingGetPropertiesThenSuccessIsReturned) {
    auto pInfoLogImp = std::make_unique<InfoLogImp>(ZES_INTEL_INFO_LOG_FORMAT_CPER);
    pInfoLogImp->init();
    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    ze_result_t result = pInfoLogImp->infoLogGetProperties(&properties);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogImpWhenCallingInfoLogReadThenUnsupportedFeatureIsReturned) {
    auto pInfoLogImp = std::make_unique<InfoLogImp>(ZES_INTEL_INFO_LOG_FORMAT_CPER);
    pInfoLogImp->init();
    uint32_t size = 0;
    ze_result_t result = pInfoLogImp->infoLogRead(&size, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogImpWhenCallingInfoLogEnableThenUnsupportedFeatureIsReturned) {
    auto pInfoLogImp = std::make_unique<InfoLogImp>(ZES_INTEL_INFO_LOG_FORMAT_CPER);
    pInfoLogImp->init();
    ze_result_t result = pInfoLogImp->infoLogEnable(true);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(SysmanInfoLogFixture, GivenWindowsPlatformWhenGettingSupportedInfoLogFormatsThenEmptyListIsReturned) {
    auto formats = OsInfoLog::getSupportedInfoLogFormats();
    EXPECT_TRUE(formats.empty());
}

TEST_F(SysmanInfoLogFixture, GivenValidDriverHandleWhenEnumeratingInfoLogsThenZeroHandlesAreReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogContextAlreadyExistsWhenEnumeratingInfoLogsAgainThenSameContextIsReused) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);
}

TEST_F(SysmanInfoLogFixture, GivenSupportedFormatsInjectedWhenEnumeratingInfoLogsThenCorrectHandleCountIsReturned) {
    InfoLogHandleContext context;
    context.supportedFormats = {ZES_INTEL_INFO_LOG_FORMAT_CPER};

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context.infoLogGet(&count, nullptr));
    EXPECT_EQ(expectedInfoLogHandleCount, count);

    std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, context.infoLogGet(&count, handles.data()));
    EXPECT_NE(nullptr, handles[0]);
}

TEST_F(SysmanInfoLogFixture, GivenMultipleInfoLogHandlesWhenDisablingInfoLogCollectionThenEveryHandleIsDisabled) {
    InfoLogHandleContext context;

    auto firstInfoLog = std::make_unique<MockInfoLog>();
    auto secondInfoLog = std::make_unique<MockInfoLog>();
    auto *pFirstInfoLog = firstInfoLog.get();
    auto *pSecondInfoLog = secondInfoLog.get();
    context.handleList.push_back(std::move(firstInfoLog));
    context.handleList.push_back(std::move(secondInfoLog));

    context.disableInfoLogCollection();

    EXPECT_EQ(1u, pFirstInfoLog->enableCallCount);
    EXPECT_FALSE(pFirstInfoLog->lastRequestedState);
    EXPECT_EQ(1u, pSecondInfoLog->enableCallCount);
    EXPECT_FALSE(pSecondInfoLog->lastRequestedState);
}

TEST_F(SysmanInfoLogFixture, GivenHandleReportingFailureWhenDisablingInfoLogCollectionThenRemainingHandlesAreStillDisabled) {
    InfoLogHandleContext context;

    auto failingInfoLog = std::make_unique<MockInfoLog>();
    failingInfoLog->enableReturnValue = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    auto lastInfoLog = std::make_unique<MockInfoLog>();
    auto *pFailingInfoLog = failingInfoLog.get();
    auto *pLastInfoLog = lastInfoLog.get();
    context.handleList.push_back(std::move(failingInfoLog));
    context.handleList.push_back(std::move(lastInfoLog));

    context.disableInfoLogCollection();

    EXPECT_EQ(1u, pFailingInfoLog->enableCallCount);
    EXPECT_EQ(1u, pLastInfoLog->enableCallCount);
    EXPECT_FALSE(pLastInfoLog->lastRequestedState);
}

TEST_F(SysmanInfoLogFixture, GivenNoInfoLogHandlesWhenDisablingInfoLogCollectionThenNothingIsDisabled) {
    InfoLogHandleContext context;
    ASSERT_TRUE(context.handleList.empty());

    context.disableInfoLogCollection();

    EXPECT_TRUE(context.handleList.empty());
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogHandlesWhenReleasingInfoLogHandlesThenHandleListIsEmptied) {
    InfoLogHandleContext context;
    context.supportedFormats = {ZES_INTEL_INFO_LOG_FORMAT_CPER};

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context.infoLogGet(&count, nullptr));
    ASSERT_EQ(expectedInfoLogHandleCount, count);
    ASSERT_EQ(expectedInfoLogHandleCount, static_cast<uint32_t>(context.handleList.size()));

    context.releaseInfoLogHandles();
    EXPECT_TRUE(context.handleList.empty());
}

TEST_F(SysmanInfoLogFixture, GivenCountZeroOrGreaterThanAvailableWhenEnumeratingInfoLogsThenCountIsUpdatedAndOneHandleIsReturned) {
    auto mockOsDriver = std::make_unique<MockOsSysmanDriver>();
    auto *savedOsDriver = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = mockOsDriver.get();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(expectedInfoLogHandleCount, count);

    count = 5;
    std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()));
    EXPECT_EQ(expectedInfoLogHandleCount, count);
    EXPECT_NE(nullptr, handles[0]);
    EXPECT_EQ(nullptr, handles[1]);

    driverHandle->pOsSysmanDriver = savedOsDriver;
}

TEST_F(SysmanInfoLogFixture, GivenCperFormatSupportedWhenCallingEnumInfoLogsExpThenOneHandleIsReturned) {
    auto mockOsDriver = std::make_unique<MockOsSysmanDriver>();
    auto *savedOsDriver = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = mockOsDriver.get();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(expectedInfoLogHandleCount, count);

    std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()));
    EXPECT_NE(nullptr, handles[0]);

    driverHandle->pOsSysmanDriver = savedOsDriver;
}

TEST_F(SysmanInfoLogFixture, GivenNullOsSysmanDriverWhenEnumeratingInfoLogsThenErrorIsReturned) {
    auto *savedOsDriver = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = nullptr;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));

    driverHandle->pOsSysmanDriver = savedOsDriver;
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingGetPropertiesExpThenUnsupportedFeatureIsReturned) {
    auto mockOsDriver = std::make_unique<MockOsSysmanDriver>();
    auto *savedOsDriver = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = mockOsDriver.get();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_EQ(expectedInfoLogHandleCount, count);

    std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()));
    ASSERT_NE(nullptr, handles[0]);

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogGetPropertiesExp(handles[0], &properties));

    driverHandle->pOsSysmanDriver = savedOsDriver;
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingInfoLogReadExpThenUnsupportedFeatureIsReturned) {
    auto mockOsDriver = std::make_unique<MockOsSysmanDriver>();
    auto *savedOsDriver = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = mockOsDriver.get();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_EQ(expectedInfoLogHandleCount, count);

    std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()));
    ASSERT_NE(nullptr, handles[0]);

    uint32_t size = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogReadExp(handles[0], &size, nullptr));

    driverHandle->pOsSysmanDriver = savedOsDriver;
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingInfoLogEnableExpThenUnsupportedFeatureIsReturned) {
    auto mockOsDriver = std::make_unique<MockOsSysmanDriver>();
    auto *savedOsDriver = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = mockOsDriver.get();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_EQ(expectedInfoLogHandleCount, count);

    std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()));
    ASSERT_NE(nullptr, handles[0]);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogEnableExp(handles[0], true));

    driverHandle->pOsSysmanDriver = savedOsDriver;
}

} // namespace ult
} // namespace Sysman
} // namespace L0

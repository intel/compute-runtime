/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_sysfs_engine.h"

using ::testing::Matcher;
namespace L0 {
namespace ult {
constexpr uint32_t handleComponentCount = 1u;
class SysmanDeviceEngineFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<Mock<EngineSysfsAccess>> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<NiceMock<Mock<EngineSysfsAccess>>>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<EngineSysfsAccess>::getVal));

        pSysmanDeviceImp->pEngineHandleContext->init();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
    }

    std::vector<zes_engine_handle_t> get_engine_handles(uint32_t count) {
        std::vector<zes_engine_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceEngineFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumEngineGroupsThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCount);

    uint32_t testcount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &testcount, NULL));
    EXPECT_EQ(testcount, count);

    count = 0;
    std::vector<zes_engine_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCount);
}

TEST_F(SysmanDeviceEngineFixture, GivenValidEngineHandleWhenCallingZesEngineGetPropertiesThenVerifyCallSucceeds) {
    zes_engine_properties_t properties;
    auto handle = get_engine_handles(handleComponentCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handle[0], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COMPUTE_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
}
TEST_F(SysmanDeviceEngineFixture, GivenValidEngineHandleWhenCallingZesEngineGetActivityThenVerifyCallReturnsUnsupportedErrorStatus) {
    zes_engine_stats_t Stats;
    auto handle = get_engine_handles(handleComponentCount);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivity(handle[0], &Stats));
}

TEST_F(SysmanDeviceEngineFixture, GivenEngineGroupEmptyWhenGettingEngineGroupAndIfRetrievedEngineGroupNotComputeThenErrorReturned) {
    auto pZesEngineImpTest = std::make_unique<EngineImp>(pOsSysman, ZES_ENGINE_GROUP_COMPUTE_ALL);
    zes_engine_group_t engineGroup;
    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<EngineSysfsAccess>::getIncorrectVal));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pZesEngineImpTest->pOsEngine->getEngineGroup(engineGroup));
}

TEST_F(SysmanDeviceEngineFixture, GivenEmptyTimeStampWhenGettingTimeStampThenSuccessIsReturned) {
    auto pZesEngineImpTest = std::make_unique<EngineImp>(pOsSysman, ZES_ENGINE_GROUP_COMPUTE_ALL);
    uint64_t timeStamp = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pZesEngineImpTest->pOsEngine->getTimeStamp(timeStamp));
}

TEST_F(SysmanDeviceEngineFixture, GivenEngineGroupEmptyWhenGettingEngineGroupAndIfAPIFailsThenErrorReturned) {
    auto pZesEngineImpTest = std::make_unique<EngineImp>(pOsSysman, ZES_ENGINE_GROUP_COMPUTE_ALL);
    zes_engine_group_t engineGroup;
    ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
        .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<EngineSysfsAccess>::getValReturnError));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pZesEngineImpTest->pOsEngine->getEngineGroup(engineGroup));
}

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/scheduler/os_scheduler.h"
#include "level_zero/tools/source/sysman/scheduler/scheduler_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

class ZesSchedulerFixture : public SysmanDeviceFixture {

  protected:
    FirmwareUtil *pFwUtilInterfaceOld = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesSchedulerFixture, GivenComponentCountZeroWhenQueryingSchedulerHandlesThenNoHandlesAreReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumSchedulers(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(ZesSchedulerFixture, GivenValidDeviceHandleWhenGettingCurrentModeThenErrorIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    zes_sched_mode_t mode;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSchedulerImp->getCurrentMode(&mode));
}

TEST_F(ZesSchedulerFixture, GivenValidDeviceHandleWhenGettingTimeoutModePropertiesThenErrorIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    ze_bool_t getDefaults = 0;
    zes_sched_timeout_properties_t config;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSchedulerImp->getTimeoutModeProperties(getDefaults, &config));
}

TEST_F(ZesSchedulerFixture, GivenValidDeviceHandleWhenGettingTimesliceModePropertiesThenErrorIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    ze_bool_t getDefaults = 0;
    zes_sched_timeslice_properties_t config;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSchedulerImp->getTimesliceModeProperties(getDefaults, &config));
}

TEST_F(ZesSchedulerFixture, GivenValidDeviceHandleWhenSettingTimeoutModeThenErrorIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    zes_sched_timeout_properties_t properties;
    ze_bool_t needReload;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSchedulerImp->setTimeoutMode(&properties, &needReload));
}

TEST_F(ZesSchedulerFixture, GivenValidDeviceHandleWhenSettingTimesliceModeThenErrorIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    zes_sched_timeslice_properties_t properties;
    ze_bool_t needReload;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSchedulerImp->setTimesliceMode(&properties, &needReload));
}

TEST_F(ZesSchedulerFixture, GivenValidDeviceHandleWhenSettingExclusiveModeThenErrorIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    ze_bool_t needReload;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSchedulerImp->setExclusiveMode(&needReload));
}

TEST_F(ZesSchedulerFixture, GivenValidDeviceHandleWhenSettingDebugModeThenErrorIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    ze_bool_t needReload;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSchedulerImp->setComputeUnitDebugMode(&needReload));
}

TEST_F(ZesSchedulerFixture, GivenValidDeviceHandleWhenGettingSchedulerPropertiesThenSuccessIsReturned) {
    std::vector<std::string> listOfEngines = {"rcs0"};
    auto pSchedulerImp = std::make_unique<SchedulerImp>(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines, device->toHandle());
    zes_sched_properties_t properties;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSchedulerImp->schedulerGetProperties(&properties));
}

} // namespace ult
} // namespace L0

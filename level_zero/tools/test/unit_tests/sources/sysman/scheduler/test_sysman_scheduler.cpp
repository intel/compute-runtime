/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_scheduler.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {

class SysmanSchedulerFixture : public DeviceFixture, public ::testing::Test {
  protected:
    SysmanImp *sysmanImp;
    zet_sysman_handle_t hSysman;

    Mock<OsScheduler> *pOsScheduler;
    L0::SchedulerImp *pSchedImp;
    L0::Scheduler *pSchedPrev;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = new SysmanImp(device->toHandle());
        pOsScheduler = new NiceMock<Mock<OsScheduler>>;
        pOsScheduler->setMockTimeout(640);
        pOsScheduler->setMockTimeslice(1);
        pOsScheduler->setMockHeartbeat(2500);

        ON_CALL(*pOsScheduler, getPreemptTimeout(_))
            .WillByDefault(Invoke(pOsScheduler, &Mock<OsScheduler>::getMockTimeout));
        ON_CALL(*pOsScheduler, getTimesliceDuration(_))
            .WillByDefault(Invoke(pOsScheduler, &Mock<OsScheduler>::getMockTimeslice));
        ON_CALL(*pOsScheduler, getHeartbeatInterval(_))
            .WillByDefault(Invoke(pOsScheduler, &Mock<OsScheduler>::getMockHeartbeat));
        ON_CALL(*pOsScheduler, setPreemptTimeout(_))
            .WillByDefault(Invoke(pOsScheduler, &Mock<OsScheduler>::setMockTimeout));
        ON_CALL(*pOsScheduler, setTimesliceDuration(_))
            .WillByDefault(Invoke(pOsScheduler, &Mock<OsScheduler>::setMockTimeslice));
        ON_CALL(*pOsScheduler, setHeartbeatInterval(_))
            .WillByDefault(Invoke(pOsScheduler, &Mock<OsScheduler>::setMockHeartbeat));

        pSchedImp = new SchedulerImp();
        pSchedPrev = sysmanImp->pSched;
        sysmanImp->pSched = static_cast<Scheduler *>(pSchedImp);
        pSchedImp->pOsScheduler = pOsScheduler;
        pSchedImp->init();
        hSysman = sysmanImp->toHandle();
    }
    void TearDown() override {
        // restore state
        sysmanImp->pSched = pSchedPrev;
        // cleanup
        if (pSchedImp != nullptr) {
            delete pSchedImp;
            pSchedImp = nullptr;
        }
        if (sysmanImp != nullptr) {
            delete sysmanImp;
            sysmanImp = nullptr;
        }
        DeviceFixture::TearDown();
    }

    zet_sched_mode_t fixtureGetCurrentMode(zet_sysman_handle_t hSysman) {
        zet_sched_mode_t mode;
        ze_result_t result = zetSysmanSchedulerGetCurrentMode(hSysman, &mode);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        return mode;
    }

    zet_sched_timeout_properties_t fixtureGetTimeoutModeProperties(zet_sysman_handle_t hSysman) {
        zet_sched_timeout_properties_t config;
        ze_result_t result = zetSysmanSchedulerGetTimeoutModeProperties(hSysman, false, &config);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        return config;
    }

    zet_sched_timeslice_properties_t fixtureGetTimesliceModeProperties(zet_sysman_handle_t hSysman) {
        zet_sched_timeslice_properties_t config;
        ze_result_t result = zetSysmanSchedulerGetTimesliceModeProperties(hSysman, false, &config);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        return config;
    }
};

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetCurrentModeThenVerifyzetSysmanSchedulerGetCurrentModeCallSucceeds) {
    auto mode = fixtureGetCurrentMode(hSysman);
    EXPECT_EQ(mode, ZET_SCHED_MODE_TIMESLICE);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetTimeoutModePropertiesThenVerifyzetSysmanSchedulerGetTimeoutModePropertiesCallSucceeds) {
    auto config = fixtureGetTimeoutModeProperties(hSysman);
    EXPECT_EQ(config.watchdogTimeout, 2500ul);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetTimesliceModePropertiesThenVerifyzetSysmanSchedulerGetTimesliceModePropertiesCallSucceeds) {
    auto config = fixtureGetTimesliceModeProperties(hSysman);
    EXPECT_EQ(config.interval, 1ul);
    EXPECT_EQ(config.yieldTimeout, 640ul);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerSetTimeoutModeThenVerifyzetSysmanSchedulerSetTimeoutModeCallSucceeds) {
    ze_bool_t needReboot;
    zet_sched_timeout_properties_t setConfig;
    setConfig.watchdogTimeout = 5000;
    ze_result_t result = zetSysmanSchedulerSetTimeoutMode(hSysman, &setConfig, &needReboot);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(needReboot);
    auto getConfig = fixtureGetTimeoutModeProperties(hSysman);
    EXPECT_EQ(getConfig.watchdogTimeout, 5000ul);
    auto mode = fixtureGetCurrentMode(hSysman);
    EXPECT_EQ(mode, ZET_SCHED_MODE_TIMEOUT);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerSetTimesliceModeThenVerifyzetSysmanSchedulerSetTimesliceModeCallSucceeds) {
    ze_bool_t needReboot;
    zet_sched_timeslice_properties_t setConfig;
    setConfig.interval = 2;
    setConfig.yieldTimeout = 1000;
    ze_result_t result = zetSysmanSchedulerSetTimesliceMode(hSysman, &setConfig, &needReboot);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(needReboot);
    auto getConfig = fixtureGetTimesliceModeProperties(hSysman);
    EXPECT_EQ(getConfig.interval, 2ul);
    EXPECT_EQ(getConfig.yieldTimeout, 1000ul);
    auto mode = fixtureGetCurrentMode(hSysman);
    EXPECT_EQ(mode, ZET_SCHED_MODE_TIMESLICE);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerSetExclusiveModeThenVerifyzetSysmanSchedulerSetExclusiveModeCallSucceeds) {
    ze_bool_t needReboot;
    ze_result_t result = zetSysmanSchedulerSetExclusiveMode(hSysman, &needReboot);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(needReboot);
    auto mode = fixtureGetCurrentMode(hSysman);
    EXPECT_EQ(mode, ZET_SCHED_MODE_EXCLUSIVE);
}

} // namespace ult
} // namespace L0

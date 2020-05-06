/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/scheduler/linux/os_scheduler_imp.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_scheduler.h"
#include "sysman/scheduler/os_scheduler.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {

constexpr uint64_t convertMilliToMicro = 1000u;
constexpr uint64_t defaultTimeoutMilliSecs = 640u;
constexpr uint64_t defaultTimesliceMilliSecs = 1u;
constexpr uint64_t defaultHeartbeatMilliSecs = 2500u;
constexpr uint64_t expectedHeartbeatTimeoutMicroSecs = defaultHeartbeatMilliSecs * convertMilliToMicro;
constexpr uint64_t expectedTimeoutMicroSecs = defaultTimeoutMilliSecs * convertMilliToMicro;
constexpr uint64_t expectedTimesliceMicroSecs = defaultTimesliceMilliSecs * convertMilliToMicro;

class SysmanSchedulerFixture : public DeviceFixture, public ::testing::Test {
  protected:
    SysmanImp *sysmanImp = nullptr;
    zet_sysman_handle_t hSysman;

    OsScheduler *pOsScheduler = nullptr;
    Mock<SchedulerSysfsAccess> *pSysfsAccess = nullptr;
    L0::Scheduler *pSchedPrev = nullptr;
    L0::SchedulerImp schedImp;
    PublicLinuxSchedulerImp linuxSchedulerImp;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = new SysmanImp(device->toHandle());
        pSysfsAccess = new NiceMock<Mock<SchedulerSysfsAccess>>;
        linuxSchedulerImp.pSysfsAccess = pSysfsAccess;
        pOsScheduler = static_cast<OsScheduler *>(&linuxSchedulerImp);
        pSysfsAccess->setVal(preemptTimeoutMilliSecs, defaultTimeoutMilliSecs);
        pSysfsAccess->setVal(timesliceDurationMilliSecs, defaultTimesliceMilliSecs);
        pSysfsAccess->setVal(heartbeatIntervalMilliSecs, defaultHeartbeatMilliSecs);

        ON_CALL(*pSysfsAccess, read(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<SchedulerSysfsAccess>::getVal));
        ON_CALL(*pSysfsAccess, write(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<SchedulerSysfsAccess>::setVal));

        pSchedPrev = sysmanImp->pSched;
        sysmanImp->pSched = static_cast<Scheduler *>(&schedImp);
        schedImp.pOsScheduler = pOsScheduler;
        schedImp.init();
        hSysman = sysmanImp->toHandle();
    }
    void TearDown() override {
        // restore state
        sysmanImp->pSched = pSchedPrev;
        schedImp.pOsScheduler = nullptr;

        // cleanup
        if (pSysfsAccess != nullptr) {
            delete pSysfsAccess;
            pSysfsAccess = nullptr;
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
    EXPECT_EQ(config.watchdogTimeout, expectedHeartbeatTimeoutMicroSecs);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetTimesliceModePropertiesThenVerifyzetSysmanSchedulerGetTimesliceModePropertiesCallSucceeds) {
    auto config = fixtureGetTimesliceModeProperties(hSysman);
    EXPECT_EQ(config.interval, expectedTimesliceMicroSecs);
    EXPECT_EQ(config.yieldTimeout, expectedTimeoutMicroSecs);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerSetTimeoutModeThenVerifyzetSysmanSchedulerSetTimeoutModeCallSucceeds) {
    ze_bool_t needReboot;
    zet_sched_timeout_properties_t setConfig;
    setConfig.watchdogTimeout = 10000u;
    ze_result_t result = zetSysmanSchedulerSetTimeoutMode(hSysman, &setConfig, &needReboot);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(needReboot);
    auto getConfig = fixtureGetTimeoutModeProperties(hSysman);
    EXPECT_EQ(getConfig.watchdogTimeout, setConfig.watchdogTimeout);
    auto mode = fixtureGetCurrentMode(hSysman);
    EXPECT_EQ(mode, ZET_SCHED_MODE_TIMEOUT);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerSetTimesliceModeThenVerifyzetSysmanSchedulerSetTimesliceModeCallSucceeds) {
    ze_bool_t needReboot;
    zet_sched_timeslice_properties_t setConfig;
    setConfig.interval = 1000u;
    setConfig.yieldTimeout = 1000u;
    ze_result_t result = zetSysmanSchedulerSetTimesliceMode(hSysman, &setConfig, &needReboot);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(needReboot);
    auto getConfig = fixtureGetTimesliceModeProperties(hSysman);
    EXPECT_EQ(getConfig.interval, setConfig.interval);
    EXPECT_EQ(getConfig.yieldTimeout, setConfig.yieldTimeout);
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

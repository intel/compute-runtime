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
constexpr uint64_t defaultTimeoutMilliSecs = 650u;
constexpr uint64_t defaultTimesliceMilliSecs = 1u;
constexpr uint64_t defaultHeartbeatMilliSecs = 3000u;
constexpr uint64_t timeoutMilliSecs = 640u;
constexpr uint64_t timesliceMilliSecs = 1u;
constexpr uint64_t heartbeatMilliSecs = 2500u;
constexpr uint64_t expectedDefaultHeartbeatTimeoutMicroSecs = defaultHeartbeatMilliSecs * convertMilliToMicro;
constexpr uint64_t expectedDefaultTimeoutMicroSecs = defaultTimeoutMilliSecs * convertMilliToMicro;
constexpr uint64_t expectedDefaultTimesliceMicroSecs = defaultTimesliceMilliSecs * convertMilliToMicro;
constexpr uint64_t expectedHeartbeatTimeoutMicroSecs = heartbeatMilliSecs * convertMilliToMicro;
constexpr uint64_t expectedTimeoutMicroSecs = timeoutMilliSecs * convertMilliToMicro;
constexpr uint64_t expectedTimesliceMicroSecs = timesliceMilliSecs * convertMilliToMicro;

class SysmanSchedulerFixture : public DeviceFixture, public ::testing::Test {
  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;

    OsScheduler *pOsScheduler = nullptr;
    Mock<SchedulerSysfsAccess> *pSysfsAccess = nullptr;
    L0::Scheduler *pSchedPrev = nullptr;
    L0::SchedulerImp schedImp;
    PublicLinuxSchedulerImp linuxSchedulerImp;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pSysfsAccess = new NiceMock<Mock<SchedulerSysfsAccess>>;
        linuxSchedulerImp.pSysfsAccess = pSysfsAccess;
        pOsScheduler = static_cast<OsScheduler *>(&linuxSchedulerImp);
        pSysfsAccess->setVal(defaultPreemptTimeoutMilliSecs, defaultTimeoutMilliSecs);
        pSysfsAccess->setVal(defaultTimesliceDurationMilliSecs, defaultTimesliceMilliSecs);
        pSysfsAccess->setVal(defaultHeartbeatIntervalMilliSecs, defaultHeartbeatMilliSecs);
        pSysfsAccess->setVal(preemptTimeoutMilliSecs, timeoutMilliSecs);
        pSysfsAccess->setVal(timesliceDurationMilliSecs, timesliceMilliSecs);
        pSysfsAccess->setVal(heartbeatIntervalMilliSecs, heartbeatMilliSecs);

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
    }

    zet_sched_mode_t fixtureGetCurrentMode(zet_sysman_handle_t hSysman) {
        zet_sched_mode_t mode;
        ze_result_t result = zetSysmanSchedulerGetCurrentMode(hSysman, &mode);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        return mode;
    }

    zet_sched_timeout_properties_t fixtureGetTimeoutModeProperties(zet_sysman_handle_t hSysman, ze_bool_t getDefaults) {
        zet_sched_timeout_properties_t config;
        ze_result_t result = zetSysmanSchedulerGetTimeoutModeProperties(hSysman, getDefaults, &config);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        return config;
    }

    zet_sched_timeslice_properties_t fixtureGetTimesliceModeProperties(zet_sysman_handle_t hSysman, ze_bool_t getDefaults) {
        zet_sched_timeslice_properties_t config;
        ze_result_t result = zetSysmanSchedulerGetTimesliceModeProperties(hSysman, getDefaults, &config);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        return config;
    }
};

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetCurrentModeThenVerifyzetSysmanSchedulerGetCurrentModeCallSucceeds) {
    auto mode = fixtureGetCurrentMode(hSysman);
    EXPECT_EQ(mode, ZET_SCHED_MODE_TIMESLICE);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetTimeoutModePropertiesThenVerifyzetSysmanSchedulerGetTimeoutModePropertiesCallSucceeds) {
    auto config = fixtureGetTimeoutModeProperties(hSysman, false);
    EXPECT_EQ(config.watchdogTimeout, expectedHeartbeatTimeoutMicroSecs);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetTimeoutModePropertiesWithDefaultsThenVerifyzetSysmanSchedulerGetTimeoutModePropertiesCallSucceeds) {
    auto config = fixtureGetTimeoutModeProperties(hSysman, true);
    EXPECT_EQ(config.watchdogTimeout, expectedDefaultHeartbeatTimeoutMicroSecs);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetTimesliceModePropertiesThenVerifyzetSysmanSchedulerGetTimesliceModePropertiesCallSucceeds) {
    auto config = fixtureGetTimesliceModeProperties(hSysman, false);
    EXPECT_EQ(config.interval, expectedTimesliceMicroSecs);
    EXPECT_EQ(config.yieldTimeout, expectedTimeoutMicroSecs);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetTimesliceModePropertiesWithDefaultsThenVerifyzetSysmanSchedulerGetTimesliceModePropertiesCallSucceeds) {
    auto config = fixtureGetTimesliceModeProperties(hSysman, true);
    EXPECT_EQ(config.interval, expectedDefaultTimesliceMicroSecs);
    EXPECT_EQ(config.yieldTimeout, expectedDefaultTimeoutMicroSecs);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerSetTimeoutModeThenVerifyzetSysmanSchedulerSetTimeoutModeCallSucceeds) {
    ze_bool_t needReboot;
    zet_sched_timeout_properties_t setConfig;
    setConfig.watchdogTimeout = 10000u;
    ze_result_t result = zetSysmanSchedulerSetTimeoutMode(hSysman, &setConfig, &needReboot);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(needReboot);
    auto getConfig = fixtureGetTimeoutModeProperties(hSysman, false);
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
    auto getConfig = fixtureGetTimesliceModeProperties(hSysman, false);
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

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetCurrentModeWhenSysfsNodeIsAbsentThenFailureIsReturned) {
    ON_CALL(*pSysfsAccess, read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<SchedulerSysfsAccess>::getValForError));
    zet_sched_mode_t mode;
    ze_result_t result = zetSysmanSchedulerGetCurrentMode(hSysman, &mode);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetTimeoutModePropertiesWithDefaultsWhenSysfsNodeIsAbsentThenFailureIsReturned) {
    ON_CALL(*pSysfsAccess, read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<SchedulerSysfsAccess>::getValForError));
    zet_sched_timeout_properties_t config;
    ze_result_t result = zetSysmanSchedulerGetTimeoutModeProperties(hSysman, true, &config);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanSchedulerFixture, GivenValidSysmanHandleWhenCallingzetSysmanSchedulerGetTimesliceModePropertiesWithDefaultsWhenSysfsNodeIsAbsentThenFailureIsReturned) {
    ON_CALL(*pSysfsAccess, read(_, _))
        .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<SchedulerSysfsAccess>::getValForError));
    zet_sched_timeslice_properties_t config;
    ze_result_t result = zetSysmanSchedulerGetTimesliceModeProperties(hSysman, true, &config);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

} // namespace ult
} // namespace L0

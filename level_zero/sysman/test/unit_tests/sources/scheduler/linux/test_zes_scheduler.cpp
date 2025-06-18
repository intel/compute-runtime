/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/sysman_const.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/scheduler/linux/mock_sysfs_scheduler.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t handleComponentCount = 5u;
constexpr uint64_t convertMilliToMicro = 1000u;

class SysmanDeviceSchedulerFixture : public SysmanDeviceFixture {

  protected:
    L0::Sysman::SysmanDevice *device = nullptr;

    std::vector<zes_sched_handle_t> getSchedHandles(uint32_t count) {
        std::vector<zes_sched_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumSchedulers(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    zes_sched_mode_t fixtureGetCurrentMode(zes_sched_handle_t hScheduler) {
        zes_sched_mode_t mode;
        ze_result_t result = zesSchedulerGetCurrentMode(hScheduler, &mode);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        return mode;
    }

    zes_sched_timeout_properties_t fixtureGetTimeoutModeProperties(zes_sched_handle_t hScheduler, ze_bool_t getDefaults) {
        zes_sched_timeout_properties_t config;
        ze_result_t result = zesSchedulerGetTimeoutModeProperties(hScheduler, getDefaults, &config);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        return config;
    }

    zes_sched_timeslice_properties_t fixtureGetTimesliceModeProperties(zes_sched_handle_t hScheduler, ze_bool_t getDefaults) {
        zes_sched_timeslice_properties_t config;
        ze_result_t result = zesSchedulerGetTimesliceModeProperties(hScheduler, getDefaults, &config);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        return config;
    }

    SysmanDeviceSchedulerFixture() = default;
};

class SysmanDeviceSchedulerFixtureI915 : public SysmanDeviceSchedulerFixture {

  protected:
    std::unique_ptr<MockSchedulerSysfsAccessI915> pSysfsAccess;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOld = nullptr;

    uint64_t defaultTimeout = 650u;
    uint64_t defaultTimeslice = 1u;
    uint64_t defaultHeartbeat = 3000u;
    uint64_t timeout = 640u;
    uint64_t timeslice = 1u;
    uint64_t heartbeat = 2500u;
    uint64_t expectedDefaultHeartbeatTimeoutMicroSecs = defaultHeartbeat * convertMilliToMicro;
    uint64_t expectedDefaultTimeoutMicroSecs = defaultTimeout * convertMilliToMicro;
    uint64_t expectedDefaultTimesliceMicroSecs = defaultTimeslice * convertMilliToMicro;
    uint64_t expectedHeartbeatTimeoutMicroSecs = heartbeat * convertMilliToMicro;
    uint64_t expectedTimeoutMicroSecs = timeout * convertMilliToMicro;
    uint64_t expectedTimesliceMicroSecs = timeslice * convertMilliToMicro;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockSchedulerSysfsAccessI915>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pSysfsAccess->engineSchedFilePropertiesMap.clear();
        pSysfsAccess->cleanUpMap();
        for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->defaultpreemptTimeout, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->defaulttimesliceDuration, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->defaultheartbeatInterval, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->preemptTimeout, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->timesliceDuration, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->heartbeatInterval, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);

            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->defaultpreemptTimeout, defaultTimeout);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->defaulttimesliceDuration, defaultTimeslice);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->defaultheartbeatInterval, defaultHeartbeat);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->preemptTimeout, timeout);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->timesliceDuration, timeslice);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->heartbeatInterval, heartbeat);
        }
        pSysmanDeviceImp->pSchedulerHandleContext->handleList.clear();
        device = pSysmanDevice;
        getSchedHandles(0);
    }

    void TearDown() override {
        pSysfsAccess->cleanUpMap();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenComponentCountZeroWhenCallingZesDeviceEnumSchedulersAndSysfsCanReadReturnsErrorThenZeroCountIsReturned) {
    pSysfsAccess->mockGetScanDirEntryError = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto pSchedulerHandleContextTest = std::make_unique<L0::Sysman::SchedulerHandleContext>(pOsSysman);
    pSchedulerHandleContextTest->init(pOsSysman->getSubDeviceCount());
    EXPECT_EQ(0u, static_cast<uint32_t>(pSchedulerHandleContextTest->handleList.size()));
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenComponentCountZeroWhenCallingZesDeviceEnumSchedulersAndSysfsCanReadReturnsIncorrectPermissionThenZeroCountIsReturned) {
    pSysfsAccess->setEngineDirectoryPermission(0);
    auto pSchedulerHandleContextTest = std::make_unique<L0::Sysman::SchedulerHandleContext>(pOsSysman);
    pSchedulerHandleContextTest->init(pOsSysman->getSubDeviceCount());
    EXPECT_EQ(0u, static_cast<uint32_t>(pSchedulerHandleContextTest->handleList.size()));
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenComponentCountZeroWhenCallingZesDeviceEnumSchedulersThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumSchedulers(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCount);

    uint32_t testcount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumSchedulers(device->toHandle(), &testcount, NULL));
    EXPECT_EQ(testcount, count);

    count = 0;
    std::vector<zes_sched_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumSchedulers(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCount);
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenComponentCountZeroAndKMDVersionIsPrelimWhenCallingZesDeviceEnumSchedulersThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {

    pLinuxSysmanImp->pSysmanKmdInterface.reset();
    pLinuxSysmanImp->pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915Prelim>(pLinuxSysmanImp->getSysmanProductHelper());

    auto pSchedulerHandleContextTest = std::make_unique<L0::Sysman::SchedulerHandleContext>(pOsSysman);
    pSchedulerHandleContextTest->init(pOsSysman->getSubDeviceCount());

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumSchedulers(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCount);

    uint32_t testcount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumSchedulers(device->toHandle(), &testcount, NULL));
    EXPECT_EQ(testcount, count);

    count = 0;
    std::vector<zes_sched_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumSchedulers(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCount);
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetCurrentModeThenVerifyzesSchedulerGetCurrentModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        auto mode = fixtureGetCurrentMode(handle);
        EXPECT_EQ(mode, ZES_SCHED_MODE_TIMESLICE);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesThenVerifyzesSchedulerGetTimeoutModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        auto config = fixtureGetTimeoutModeProperties(handle, false);
        EXPECT_EQ(config.watchdogTimeout, expectedHeartbeatTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesThenVerifyzesSchedulerGetTimeoutModePropertiesForDifferingValues) {
    auto handles = getSchedHandles(handleComponentCount);
    pSysfsAccess->write(pSysfsAccess->engineDir + "/" + "vcs1" + "/" + pSysfsAccess->heartbeatInterval, (heartbeat + 5));

    for (auto handle : handles) {
        zes_sched_properties_t properties = {};
        zes_sched_timeout_properties_t config;
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            ze_result_t result = zesSchedulerGetTimeoutModeProperties(handle, false, &config);
            EXPECT_NE(ZE_RESULT_SUCCESS, result);
        }
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesWithDefaultsThenVerifyzesSchedulerGetTimeoutModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        auto config = fixtureGetTimeoutModeProperties(handle, true);
        EXPECT_EQ(config.watchdogTimeout, expectedDefaultHeartbeatTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimesliceModePropertiesThenVerifyzesSchedulerGetTimesliceModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        auto config = fixtureGetTimesliceModeProperties(handle, false);
        EXPECT_EQ(config.interval, expectedTimesliceMicroSecs);
        EXPECT_EQ(config.yieldTimeout, expectedTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimescliceModePropertiesThenVerifyzesSchedulerGetTimescliceModePropertiesForDifferingPreemptTimeoutValues) {
    auto handles = getSchedHandles(handleComponentCount);
    pSysfsAccess->write(pSysfsAccess->engineDir + "/" + "vcs1" + "/" + pSysfsAccess->preemptTimeout, (timeout + 5));

    for (auto handle : handles) {
        zes_sched_properties_t properties = {};
        zes_sched_timeslice_properties_t config;
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            ze_result_t result = zesSchedulerGetTimesliceModeProperties(handle, false, &config);
            EXPECT_NE(ZE_RESULT_SUCCESS, result);
        }
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimescliceModePropertiesThenVerifyzesSchedulerGetTimescliceModePropertiesForDifferingTimesliceDurationValues) {
    auto handles = getSchedHandles(handleComponentCount);
    pSysfsAccess->write(pSysfsAccess->engineDir + "/" + "vcs1" + "/" + pSysfsAccess->timesliceDuration, (timeslice + 5));

    for (auto handle : handles) {
        zes_sched_properties_t properties = {};
        zes_sched_timeslice_properties_t config;
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            ze_result_t result = zesSchedulerGetTimesliceModeProperties(handle, false, &config);
            EXPECT_NE(ZE_RESULT_SUCCESS, result);
        }
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesThenVerifyzesSchedulerGetTimeoutModePropertiesForReadFileFailureFileUnavailable) {
    auto handles = getSchedHandles(handleComponentCount);

    for (auto handle : handles) {
        zes_sched_properties_t properties = {};
        zes_sched_timeout_properties_t config;
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {

            pSysfsAccess->mockReadFileFailureError = ZE_RESULT_ERROR_NOT_AVAILABLE;

            ze_result_t result = zesSchedulerGetTimeoutModeProperties(handle, false, &config);
            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
        }
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesThenVerifyzesSchedulerGetTimeoutModePropertiesForReadFileFailureInsufficientPermissions) {
    auto handles = getSchedHandles(handleComponentCount);

    for (auto handle : handles) {
        zes_sched_properties_t properties = {};
        zes_sched_timeout_properties_t config;
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {

            pSysfsAccess->mockReadFileFailureError = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;

            ze_result_t result = zesSchedulerGetTimeoutModeProperties(handle, false, &config);
            EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
        }
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimescliceModePropertiesThenVerifyzesSchedulerGetTimescliceModePropertiesForReadFileFailureDueToUnavailable) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_sched_timeslice_properties_t config;
        zes_sched_properties_t properties = {};
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {

            /* The flag mockReadReturnStatus enables the call to read call where
             * the read call returns ZE_RESULT_SUCCESS for the first 3 invocations
             * and the 4th invocation returns the ZE_RESULT_ERROR_NOT_AVAILABLE
             */

            pSysfsAccess->mockReadReturnStatus = true;

            result = zesSchedulerGetTimesliceModeProperties(handle, false, &config);
            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
        }
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimescliceModePropertiesThenVerifyzesSchedulerGetTimescliceModePropertiesForReadFileFailureDueToInsufficientPermissions) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_sched_timeslice_properties_t config;
        zes_sched_properties_t properties = {};
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {

            pSysfsAccess->mockReadFileFailureError = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;

            result = zesSchedulerGetTimesliceModeProperties(handle, false, &config);
            EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
        }
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimesliceModePropertiesWithDefaultsThenVerifyzesSchedulerGetTimesliceModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        auto config = fixtureGetTimesliceModeProperties(handle, true);
        EXPECT_EQ(config.interval, expectedDefaultTimesliceMicroSecs);
        EXPECT_EQ(config.yieldTimeout, expectedDefaultTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeThenVerifyzesSchedulerSetTimeoutModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setConfig;
        setConfig.watchdogTimeout = 10000u;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(needReboot);
        auto getConfig = fixtureGetTimeoutModeProperties(handle, false);
        EXPECT_EQ(getConfig.watchdogTimeout, setConfig.watchdogTimeout);
        auto mode = fixtureGetCurrentMode(handle);
        EXPECT_EQ(mode, ZES_SCHED_MODE_TIMEOUT);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenTimeoutLessThanMinimumThenVerifyzesSchedulerSetTimeoutModeCallFails) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setConfig;
        setConfig.watchdogTimeout = minTimeoutModeHeartbeat - 1;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenCurrentModeIsTimeoutModeThenVerifyzesSchedulerSetTimeoutModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setTimeOutConfig;
        setTimeOutConfig.watchdogTimeout = 10000u;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setTimeOutConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        result = zesSchedulerSetTimeoutMode(handle, &setTimeOutConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenHeartBeatSettingFailsThenVerifyzesSchedulerSetTimeoutModeCallFails) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->heartbeatInterval, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setTimeOutConfig;
        setTimeOutConfig.watchdogTimeout = 10000u;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setTimeOutConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenGetCurrentModeFailsThenVerifyzesSchedulerSetTimeoutModeCallFails) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->preemptTimeout, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setConfig;
        setConfig.watchdogTimeout = minTimeoutModeHeartbeat;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenPreEmptTimeoutNoPermissionThenVerifyzesSchedulerSetTimeoutModeCallFails) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->preemptTimeout, true, S_IRUSR | S_IRGRP | S_IROTH);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setConfig;
        setConfig.watchdogTimeout = minTimeoutModeHeartbeat;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenTimeSliceDurationNoPermissionThenVerifyzesSchedulerSetTimeoutModeCallFails) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->timesliceDuration, true, S_IRUSR | S_IRGRP | S_IROTH);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setConfig;
        setConfig.watchdogTimeout = minTimeoutModeHeartbeat;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeThenVerifyzesSchedulerSetTimesliceModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = 1000u;
        setConfig.yieldTimeout = 1000u;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(needReboot);
        auto getConfig = fixtureGetTimesliceModeProperties(handle, false);
        EXPECT_EQ(getConfig.interval, setConfig.interval);
        EXPECT_EQ(getConfig.yieldTimeout, setConfig.yieldTimeout);
        auto mode = fixtureGetCurrentMode(handle);
        EXPECT_EQ(mode, ZES_SCHED_MODE_TIMESLICE);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeWhenIntervalIsLessThanMinimumThenVerifyzesSchedulerSetTimesliceModeCallFails) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = minTimeoutInMicroSeconds - 1;
        setConfig.yieldTimeout = 1000u;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeWhenNoAccessToTimeSliceDurationThenVerifyzesSchedulerSetTimesliceModeCallFails) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->timesliceDuration, true, S_IRUSR | S_IRGRP | S_IROTH);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = minTimeoutInMicroSeconds;
        setConfig.yieldTimeout = 1000u;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeWhenNoAccessToHeartBeatIntervalThenVerifyzesSchedulerSetTimesliceModeCallFails) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->heartbeatInterval, true, S_IRUSR | S_IRGRP | S_IROTH);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = minTimeoutInMicroSeconds;
        setConfig.yieldTimeout = 1000u;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetExclusiveModeThenVerifyzesSchedulerSetExclusiveModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        ze_result_t result = zesSchedulerSetExclusiveMode(handle, &needReboot);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(needReboot);
        auto mode = fixtureGetCurrentMode(handle);
        EXPECT_EQ(mode, ZES_SCHED_MODE_EXCLUSIVE);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetExclusiveModeWhenPreEmptTimeoutNotAvailableThenVerifyzesSchedulerSetExclusiveModeCallFails) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->preemptTimeout, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        ze_result_t result = zesSchedulerSetExclusiveMode(handle, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetExclusiveModeWhenTimeSliceDurationNotAvailableThenVerifyzesSchedulerSetExclusiveModeCallFails) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->timesliceDuration, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        ze_result_t result = zesSchedulerSetExclusiveMode(handle, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetExclusiveModeWhenTimeSliceDurationHasNoPermissionsThenVerifyzesSchedulerSetExclusiveModeCallFails) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->timesliceDuration, true, S_IRUSR | S_IRGRP | S_IROTH);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        ze_result_t result = zesSchedulerSetExclusiveMode(handle, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetCurrentModeWhenPreEmptTimeOutNotAvailableThenFailureIsReturned) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->preemptTimeout, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_sched_mode_t mode;
        ze_result_t result = zesSchedulerGetCurrentMode(handle, &mode);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetCurrentModeWhenTimeSliceDurationNotAvailableThenFailureIsReturned) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->timesliceDuration, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_sched_mode_t mode;
        ze_result_t result = zesSchedulerGetCurrentMode(handle, &mode);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetCurrentModeWhenTimeSliceDurationHasNoPermissionThenFailureIsReturned) {
    for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
        pSysfsAccess->setFileProperties(engineName, pSysfsAccess->timesliceDuration, true, S_IRGRP | S_IROTH | S_IWUSR);
    }
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_sched_mode_t mode;
        ze_result_t result = zesSchedulerGetCurrentMode(handle, &mode);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetComputeUnitDebugModeThenUnsupportedFeatureIsReturned) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReload;
        ze_result_t result = zesSchedulerSetComputeUnitDebugMode(handle, &needReload);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesWithDefaultsWhenSysfsNodeIsAbsentThenFailureIsReturned) {
    pSysfsAccess->mockReadFileFailureError = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_sched_timeout_properties_t config;
        ze_result_t result = zesSchedulerGetTimeoutModeProperties(handle, true, &config);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimesliceModePropertiesWithDefaultsWhenSysfsNodeIsAbsentThenFailureIsReturned) {
    pSysfsAccess->mockReadFileFailureError = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_sched_timeslice_properties_t config;
        ze_result_t result = zesSchedulerGetTimesliceModeProperties(handle, true, &config);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeWhenSysfsNodeIsAbsentThenFailureIsReturned) {
    pSysfsAccess->mockWriteFileStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = 1000u;
        setConfig.yieldTimeout = 1000u;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeWhenSysfsNodeWithoutPermissionsThenFailureIsReturned) {
    pSysfsAccess->mockWriteFileStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = 1000u;
        setConfig.yieldTimeout = 1000u;

        pSysfsAccess->mockWriteFileStatus = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidDeviceHandleWhenCallingzesSchedulerGetPropertiesThenVerifyzesSchedulerGetPropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_sched_properties_t properties = {};
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_TRUE(properties.canControl);
        EXPECT_LE(properties.engines, ZES_ENGINE_TYPE_FLAG_RENDER);
        EXPECT_EQ(properties.supportedModes, static_cast<uint32_t>((1 << ZES_SCHED_MODE_TIMEOUT) | (1 << ZES_SCHED_MODE_TIMESLICE) | (1 << ZES_SCHED_MODE_EXCLUSIVE)));
    }
}

TEST_F(SysmanDeviceSchedulerFixtureI915, GivenValidObjectsOfClassSchedulerImpAndSchedulerHandleContextThenDuringObjectReleaseCheckDestructorBranches) {
    for (auto &handle : pSysmanDeviceImp->pSchedulerHandleContext->handleList) {
        auto pSchedulerImp = static_cast<L0::Sysman::SchedulerImp *>(handle.get());
        pSchedulerImp->pOsScheduler = nullptr;
        handle = nullptr;
    }
}

class SysmanDeviceSchedulerFixtureXe : public SysmanDeviceSchedulerFixture {

  protected:
    std::unique_ptr<MockSchedulerSysfsAccessXe> pSysfsAccess;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOld = nullptr;

    uint64_t defaultTimeout = 650u;
    uint64_t defaultTimeslice = 1u;
    uint64_t defaultHeartbeat = 3000u;
    uint64_t defaultMaximumJobTimeout = 4000u;
    uint64_t timeout = 640u;
    uint64_t timeslice = 1u;
    uint64_t heartbeat = 2500u;
    uint64_t expectedDefaultHeartbeatTimeoutMicroSecs = defaultHeartbeat * convertMilliToMicro;
    uint64_t expectedHeartbeatTimeoutMicroSecs = heartbeat * convertMilliToMicro;
    uint64_t expectedDefaultTimeoutMicroSecs = defaultTimeout;
    uint64_t expectedDefaultTimesliceMicroSecs = defaultTimeslice;
    uint64_t expectedTimeoutMicroSecs = timeout;
    uint64_t expectedTimesliceMicroSecs = timeslice;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pLinuxSysmanImp->pSysmanKmdInterface.reset();
        pLinuxSysmanImp->pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockSchedulerSysfsAccessXe>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        for (const auto &engineName : pSysfsAccess->listOfMockedEngines) {
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->defaultpreemptTimeout, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->defaulttimesliceDuration, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->defaultheartbeatInterval, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->preemptTimeout, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->timesliceDuration, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->heartbeatInterval, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            pSysfsAccess->setFileProperties(engineName, pSysfsAccess->defaultMaximumJobTimeout, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);

            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->defaultpreemptTimeout, defaultTimeout);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->defaulttimesliceDuration, defaultTimeslice);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->defaultheartbeatInterval, defaultHeartbeat);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->preemptTimeout, timeout);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->timesliceDuration, timeslice);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->heartbeatInterval, heartbeat);
            pSysfsAccess->write(pSysfsAccess->engineDir + "/" + engineName + "/" + pSysfsAccess->defaultMaximumJobTimeout, heartbeat);
        }
        pSysmanDeviceImp->pSchedulerHandleContext->handleList.clear();
        device = pSysmanDevice;
        getSchedHandles(0);
    }

    void TearDown() override {
        pSysfsAccess->engineSchedFilePropertiesMap.clear();
        pSysfsAccess->cleanUpMap();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerGetCurrentModeThenVerifyzesSchedulerGetCurrentModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        auto mode = fixtureGetCurrentMode(handle);
        EXPECT_EQ(mode, ZES_SCHED_MODE_TIMESLICE);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesThenVerifyzesSchedulerGetTimeoutModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        auto config = fixtureGetTimeoutModeProperties(handle, false);
        EXPECT_EQ(config.watchdogTimeout, expectedHeartbeatTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesThenVerifyzesSchedulerGetTimeoutModePropertiesForDifferingValues) {
    auto handles = getSchedHandles(handleComponentCount);
    pSysfsAccess->write(pSysfsAccess->engineDir + "/" + "vcs1" + "/" + pSysfsAccess->heartbeatInterval, (heartbeat + 5));

    for (auto handle : handles) {
        zes_sched_properties_t properties = {};
        zes_sched_timeout_properties_t config;
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            ze_result_t result = zesSchedulerGetTimeoutModeProperties(handle, false, &config);
            EXPECT_NE(ZE_RESULT_SUCCESS, result);
        }
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesWithDefaultsThenVerifyzesSchedulerGetTimeoutModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        auto config = fixtureGetTimeoutModeProperties(handle, true);
        EXPECT_EQ(config.watchdogTimeout, expectedDefaultHeartbeatTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimescliceModePropertiesThenVerifyzesSchedulerGetTimescliceModePropertiesForDifferingPreemptTimeoutValues) {
    auto handles = getSchedHandles(handleComponentCount);
    pSysfsAccess->write(pSysfsAccess->engineDir + "/" + "vcs1" + "/" + pSysfsAccess->preemptTimeout, (timeout + 5));

    for (auto handle : handles) {
        zes_sched_properties_t properties = {};
        zes_sched_timeslice_properties_t config;
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            ze_result_t result = zesSchedulerGetTimesliceModeProperties(handle, false, &config);
            EXPECT_NE(ZE_RESULT_SUCCESS, result);
        }
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimescliceModePropertiesThenVerifyzesSchedulerGetTimescliceModePropertiesForDifferingTimesliceDurationValues) {
    auto handles = getSchedHandles(handleComponentCount);
    pSysfsAccess->write(pSysfsAccess->engineDir + "/" + "vcs1" + "/" + pSysfsAccess->timesliceDuration, (timeslice + 5));

    for (auto handle : handles) {
        zes_sched_properties_t properties = {};
        zes_sched_timeslice_properties_t config;
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (properties.engines == ZES_ENGINE_TYPE_FLAG_MEDIA) {
            ze_result_t result = zesSchedulerGetTimesliceModeProperties(handle, false, &config);
            EXPECT_NE(ZE_RESULT_SUCCESS, result);
        }
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimesliceModePropertiesThenVerifyzesSchedulerGetTimesliceModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        auto config = fixtureGetTimesliceModeProperties(handle, false);
        EXPECT_EQ(config.interval, expectedTimesliceMicroSecs);
        EXPECT_EQ(config.yieldTimeout, expectedTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimesliceModePropertiesWithDefaultsThenVerifyzesSchedulerGetTimesliceModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        auto config = fixtureGetTimesliceModeProperties(handle, true);
        EXPECT_EQ(config.interval, expectedDefaultTimesliceMicroSecs);
        EXPECT_EQ(config.yieldTimeout, expectedDefaultTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeThenVerifyzesSchedulerSetTimeoutModeCallReturnsError) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setConfig;
        setConfig.watchdogTimeout = 10000u;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeThenVerifyzesSchedulerSetTimesliceModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = 1000u;
        setConfig.yieldTimeout = 1000u;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(needReboot);
        auto getConfig = fixtureGetTimesliceModeProperties(handle, false);
        EXPECT_EQ(getConfig.interval, setConfig.interval);
        EXPECT_EQ(getConfig.yieldTimeout, setConfig.yieldTimeout);
        auto mode = fixtureGetCurrentMode(handle);
        EXPECT_EQ(mode, ZES_SCHED_MODE_TIMESLICE);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenValidDeviceHandleWhenCallingzesSchedulerSetExclusiveModeThenVerifyzesSchedulerSetExclusiveModeCallReturnsError) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ze_bool_t needReboot;
        ze_result_t result = zesSchedulerSetExclusiveMode(handle, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixtureXe, GivenDestinationOrSourceUnitsAreNotSupportedWhenConvertSysfsValueUnitIsCalledThenNoConversionIsDone) {

    uint64_t sourceValue = 100;
    uint64_t dstValue = 0;
    pLinuxSysmanImp->pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::unAvailable, SysfsValueUnit::milli, sourceValue, dstValue);
    EXPECT_EQ(sourceValue, dstValue);
}

TEST_F(SysmanMultiDeviceFixture, GivenValidDevicePointerWhenGettingSchedPropertiesThenValidSchedPropertiesRetrieved) {
    zes_sched_properties_t properties = {};
    std::vector<std::string> listOfEngines;
    L0::Sysman::LinuxSchedulerImp *pLinuxSchedulerImp = new L0::Sysman::LinuxSchedulerImp(pOsSysman, ZES_ENGINE_TYPE_FLAG_RENDER, listOfEngines,
                                                                                          true, 3);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSchedulerImp->getProperties(properties));
    EXPECT_EQ(properties.subdeviceId, 3u);
    EXPECT_EQ(properties.onSubdevice, true);
    delete pLinuxSchedulerImp;
}

class SysmanMultiDeviceSchedulerFixtureXe : public SysmanMultiDeviceFixture {

  protected:
    MockSchedulerNeoDrm *pMockSchedulerDrm = nullptr;
    Drm *pOriginalDrm = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        pLinuxSysmanImp->pSysmanKmdInterface.reset();
        pLinuxSysmanImp->pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());
        MockSchedulerNeoDrm *pDrm = new MockSchedulerNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockSchedulerNeoDrm>(pDrm));

        pSysmanDeviceImp->pSchedulerHandleContext->handleList.clear();
        device = pSysmanDevice;
        auto drm = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
        pMockSchedulerDrm = static_cast<MockSchedulerNeoDrm *>(drm);
        pMockSchedulerDrm->sysmanQueryEngineInfo();
    }

    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_sched_handle_t> getSchedHandles(uint32_t count) {
        std::vector<zes_sched_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumSchedulers(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        handles.resize(count);
        return handles;
    }
};

TEST_F(SysmanMultiDeviceSchedulerFixtureXe, WhenSchedulerHandlesAreEnumeratedCorrectEnginesAreDetected) {
    auto schedHandles = getSchedHandles(1024);
    const uint64_t enabledEngines = ZES_ENGINE_TYPE_FLAG_RENDER | ZES_ENGINE_TYPE_FLAG_DMA |
                                    ZES_ENGINE_TYPE_FLAG_MEDIA | ZES_ENGINE_TYPE_FLAG_COMPUTE |
                                    ZES_ENGINE_TYPE_FLAG_OTHER;

    for (const auto &handle : schedHandles) {
        zes_sched_properties_t properties{};
        EXPECT_EQ(zesSchedulerGetProperties(handle, &properties), ZE_RESULT_SUCCESS);
        EXPECT_TRUE(properties.canControl);
        EXPECT_TRUE(properties.onSubdevice);
        EXPECT_LT(properties.subdeviceId, 2u);
        EXPECT_NE(properties.engines & enabledEngines, 0u);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0

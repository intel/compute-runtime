/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_sysfs_scheduler_prelim.h"

extern bool sysmanUltsEnable;

using namespace NEO;

namespace L0 {
namespace ult {
constexpr uint32_t handleComponentCount = 5u;
constexpr uint32_t handleComponentCountForMultiDeviceFixture = 4u;
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

class SysmanDeviceSchedulerFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<MockSchedulerSysfsAccess> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    PublicLinuxSchedulerImp *pLinuxSchedulerImp = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockSchedulerSysfsAccess>();
        pSysfsAccess->deviceNames = {"card0", "controlD64", "renderD128"};
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
                 [=](std::string engineName) {
                     pSysfsAccess->setFileProperties(engineName, defaultPreemptTimeoutMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
                     pSysfsAccess->setFileProperties(engineName, defaultTimesliceDurationMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
                     pSysfsAccess->setFileProperties(engineName, defaultHeartbeatIntervalMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
                     pSysfsAccess->setFileProperties(engineName, preemptTimeoutMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
                     pSysfsAccess->setFileProperties(engineName, timesliceDurationMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
                     pSysfsAccess->setFileProperties(engineName, heartbeatIntervalMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);

                     pSysfsAccess->write(engineDir + "/" + engineName + "/" + defaultPreemptTimeoutMilliSecs, defaultTimeoutMilliSecs);
                     pSysfsAccess->write(engineDir + "/" + engineName + "/" + defaultTimesliceDurationMilliSecs, defaultTimesliceMilliSecs);
                     pSysfsAccess->write(engineDir + "/" + engineName + "/" + defaultHeartbeatIntervalMilliSecs, defaultHeartbeatMilliSecs);
                     pSysfsAccess->write(engineDir + "/" + engineName + "/" + preemptTimeoutMilliSecs, timeoutMilliSecs);
                     pSysfsAccess->write(engineDir + "/" + engineName + "/" + timesliceDurationMilliSecs, timesliceMilliSecs);
                     pSysfsAccess->write(engineDir + "/" + engineName + "/" + heartbeatIntervalMilliSecs, heartbeatMilliSecs);
                     pSysfsAccess->write(enableEuDebug, 0);
                 });
        std::string dummy;
        pSysfsAccess->setFileProperties(dummy, enableEuDebug, true, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
        pSysfsAccess->write(enableEuDebug, 0);

        pSysmanDeviceImp->pSchedulerHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;

        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        getSchedHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pSysfsAccess->cleanUpMap();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        SysmanDeviceFixture::TearDown();
    }

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

    void setComputeUnitDebugModeMock(zes_sched_handle_t hScheduler) {
        auto pSchedulerImp = static_cast<SchedulerImp *>(Scheduler::fromHandle(hScheduler));
        auto pOsScheduler = static_cast<PublicLinuxSchedulerImp *>((pSchedulerImp->pOsScheduler).get());

        EXPECT_EQ(ZE_RESULT_SUCCESS, pOsScheduler->setExclusiveModeImp());
        uint64_t val = 1;
        pSysfsAccess->write(enableEuDebug, val);
        EXPECT_EQ(ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG, fixtureGetCurrentMode(hScheduler));
    }
};

TEST_F(SysmanDeviceSchedulerFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumSchedulersAndSysfsCanReadReturnsErrorThenZeroCountIsReturned) {

    pSysfsAccess->mockGetScanDirEntryError = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto pSchedulerHandleContextTest = std::make_unique<SchedulerHandleContext>(pOsSysman);
    pSchedulerHandleContextTest->init(deviceHandles);
    EXPECT_EQ(0u, static_cast<uint32_t>(pSchedulerHandleContextTest->handleList.size()));
}

TEST_F(SysmanDeviceSchedulerFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumSchedulersAndSysfsCanReadReturnsIncorrectPermissionThenZeroCountIsReturned) {
    pSysfsAccess->setEngineDirectoryPermission(0);
    auto pSchedulerHandleContextTest = std::make_unique<SchedulerHandleContext>(pOsSysman);
    pSchedulerHandleContextTest->init(deviceHandles);
    EXPECT_EQ(0u, static_cast<uint32_t>(pSchedulerHandleContextTest->handleList.size()));
}

TEST_F(SysmanDeviceSchedulerFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumSchedulersThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {
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

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetCurrentModeThenVerifyzesSchedulerGetCurrentModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        auto mode = fixtureGetCurrentMode(handle);
        EXPECT_EQ(mode, ZES_SCHED_MODE_TIMESLICE);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesThenVerifyzesSchedulerGetTimeoutModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        auto config = fixtureGetTimeoutModeProperties(handle, false);
        EXPECT_EQ(config.watchdogTimeout, expectedHeartbeatTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenSomeInvalidSchedulerModeWhenCheckingForCurrentModeThenAPIReportUnknownMode) {
    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    auto pSchedulerImp = static_cast<SchedulerImp *>(Scheduler::fromHandle(handles[0]));
    auto pOsScheduler = static_cast<PublicLinuxSchedulerImp *>((pSchedulerImp->pOsScheduler).get());
    uint64_t timeslice = 0, timeout = 0, heartbeat = 3000;
    pOsScheduler->setPreemptTimeout(timeout);
    pOsScheduler->setTimesliceDuration(timeslice);
    pOsScheduler->setHeartbeatInterval(heartbeat);
    zes_sched_mode_t mode;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesSchedulerGetCurrentMode(handles[0], &mode));
    EXPECT_EQ(ZES_SCHED_MODE_FORCE_UINT32, mode);
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesThenVerifyzesSchedulerGetTimeoutModePropertiesForDifferingValues) {
    auto handles = getSchedHandles(handleComponentCount);

    pSysfsAccess->write(engineDir + "/" + "vcs1" + "/" + heartbeatIntervalMilliSecs, (heartbeatMilliSecs + 5));

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesWithDefaultsThenVerifyzesSchedulerGetTimeoutModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        auto config = fixtureGetTimeoutModeProperties(handle, true);
        EXPECT_EQ(config.watchdogTimeout, expectedDefaultHeartbeatTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimesliceModePropertiesThenVerifyzesSchedulerGetTimesliceModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        auto config = fixtureGetTimesliceModeProperties(handle, false);
        EXPECT_EQ(config.interval, expectedTimesliceMicroSecs);
        EXPECT_EQ(config.yieldTimeout, expectedTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimescliceModePropertiesThenVerifyzesSchedulerGetTimescliceModePropertiesForDifferingPreemptTimeoutValues) {
    auto handles = getSchedHandles(handleComponentCount);

    pSysfsAccess->write(engineDir + "/" + "vcs1" + "/" + preemptTimeoutMilliSecs, (timeoutMilliSecs + 5));

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimescliceModePropertiesThenVerifyzesSchedulerGetTimescliceModePropertiesForDifferingTimesliceDurationValues) {
    auto handles = getSchedHandles(handleComponentCount);

    pSysfsAccess->write(engineDir + "/" + "vcs1" + "/" + timesliceDurationMilliSecs, (timesliceMilliSecs + 5));

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesThenVerifyzesSchedulerGetTimeoutModePropertiesForReadFileFailureFileUnavailable) {
    auto handles = getSchedHandles(handleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesThenVerifyzesSchedulerGetTimeoutModePropertiesForReadFileFailureInsufficientPermissions) {
    auto handles = getSchedHandles(handleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimescliceModePropertiesThenVerifyzesSchedulerGetTimescliceModePropertiesForReadFileFailureDueToUnavailable) {

    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimescliceModePropertiesThenVerifyzesSchedulerGetTimescliceModePropertiesForReadFileFailureDueToInsufficientPermissions) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimesliceModePropertiesWithDefaultsThenVerifyzesSchedulerGetTimesliceModePropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        auto config = fixtureGetTimesliceModeProperties(handle, true);
        EXPECT_EQ(config.interval, expectedDefaultTimesliceMicroSecs);
        EXPECT_EQ(config.yieldTimeout, expectedDefaultTimeoutMicroSecs);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeThenVerifyzesSchedulerSetTimeoutModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanDeviceSchedulerFixture, GivenCurrentModeIsDebugModeWhenCallingzesSchedulerSetTimeoutModeThenVerifyCallSucceeds) {
    VariableBackup<ProcfsAccess *> backup(&pLinuxSysmanImp->pProcfsAccess);
    auto pMockSchedulerProcfsAccess = new MockSchedulerProcfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pMockSchedulerProcfsAccess;

    // First set compute unit debug mode
    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    setComputeUnitDebugModeMock(handles[0]);

    // Change mode to ZES_SCHED_MODE_EXCLUSIVE and validate
    ze_bool_t needReboot;
    zes_sched_timeout_properties_t setConfig;
    setConfig.watchdogTimeout = 10000u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesSchedulerSetTimeoutMode(handles[0], &setConfig, &needReboot));
    EXPECT_FALSE(needReboot);
    auto getConfig = fixtureGetTimeoutModeProperties(handles[0], false);
    EXPECT_EQ(getConfig.watchdogTimeout, setConfig.watchdogTimeout);
    auto mode = fixtureGetCurrentMode(handles[0]);
    EXPECT_EQ(mode, ZES_SCHED_MODE_TIMEOUT);

    delete pMockSchedulerProcfsAccess;
}

TEST_F(SysmanDeviceSchedulerFixture, GivenCurrentModeIsDebugModeWhenSettingTimeoutModeAndDebugModeCantBeChangedThenVerifyCallFails) {
    VariableBackup<ProcfsAccess *> backup(&pLinuxSysmanImp->pProcfsAccess);
    auto pMockSchedulerProcfsAccess = new MockSchedulerProcfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pMockSchedulerProcfsAccess;
    pMockSchedulerProcfsAccess->listProcessesResult = ZE_RESULT_ERROR_NOT_AVAILABLE; // Expect failure when calling gpuProcessCleanup()

    // First set compute unit debug mode
    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    setComputeUnitDebugModeMock(handles[0]);

    // Change mode to ZES_SCHED_MODE_EXCLUSIVE and validate
    ze_bool_t needReboot;
    zes_sched_timeout_properties_t setConfig;
    setConfig.watchdogTimeout = 10000u;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesSchedulerSetTimeoutMode(handles[0], &setConfig, &needReboot));
    EXPECT_FALSE(needReboot);
    EXPECT_EQ(ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG, fixtureGetCurrentMode(handles[0]));

    delete pMockSchedulerProcfsAccess;
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenTimeoutLessThanMinimumThenVerifyzesSchedulerSetTimeoutModeCallFails) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setConfig;
        setConfig.watchdogTimeout = minTimeoutModeHeartbeat - 1;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenCurrentModeIsTimeoutModeThenVerifyzesSchedulerSetTimeoutModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setTimeOutConfig;
        setTimeOutConfig.watchdogTimeout = 10000u;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setTimeOutConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        result = zesSchedulerSetTimeoutMode(handle, &setTimeOutConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenHeartBeatSettingFailsThenVerifyzesSchedulerSetTimeoutModeCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, heartbeatIntervalMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setTimeOutConfig;
        setTimeOutConfig.watchdogTimeout = 10000u;
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesSchedulerSetTimeoutMode(handle, &setTimeOutConfig, &needReboot));
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenGetCurrentModeFailsThenVerifyzesSchedulerSetTimeoutModeCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, preemptTimeoutMilliSecs, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setConfig;
        setConfig.watchdogTimeout = minTimeoutModeHeartbeat;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenPreEmptTimeoutNoPermissionThenVerifyzesSchedulerSetTimeoutModeCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, preemptTimeoutMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setConfig;
        setConfig.watchdogTimeout = minTimeoutModeHeartbeat;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimeoutModeWhenTimeSliceDurationNoPermissionThenVerifyzesSchedulerSetTimeoutModeCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, timesliceDurationMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeout_properties_t setConfig;
        setConfig.watchdogTimeout = minTimeoutModeHeartbeat;
        ze_result_t result = zesSchedulerSetTimeoutMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeThenVerifyzesSchedulerSetTimesliceModeCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanDeviceSchedulerFixture, GivenCurrentModeIsDebugModeWhenCallingzesSchedulerSetTimesliceModeThenVerifyCallSucceeds) {
    VariableBackup<ProcfsAccess *> backup(&pLinuxSysmanImp->pProcfsAccess);
    auto pMockSchedulerProcfsAccess = new MockSchedulerProcfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pMockSchedulerProcfsAccess;

    // First set compute unit debug mode
    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    setComputeUnitDebugModeMock(handles[0]);

    // Change mode to ZES_SCHED_MODE_EXCLUSIVE and validate
    ze_bool_t needReboot;
    zes_sched_timeslice_properties_t setConfig;
    setConfig.interval = 1000u;
    setConfig.yieldTimeout = 1000u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesSchedulerSetTimesliceMode(handles[0], &setConfig, &needReboot));
    EXPECT_FALSE(needReboot);
    auto getConfig = fixtureGetTimesliceModeProperties(handles[0], false);
    EXPECT_EQ(getConfig.interval, setConfig.interval);
    EXPECT_EQ(getConfig.yieldTimeout, setConfig.yieldTimeout);
    auto mode = fixtureGetCurrentMode(handles[0]);
    EXPECT_EQ(mode, ZES_SCHED_MODE_TIMESLICE);

    delete pMockSchedulerProcfsAccess;
}

TEST_F(SysmanDeviceSchedulerFixture, GivenCurrentModeIsDebugModeWhenSettingTimesliceModeAndDebugModeCantBeChangedThenVerifyCallFails) {
    VariableBackup<ProcfsAccess *> backup(&pLinuxSysmanImp->pProcfsAccess);
    auto pMockSchedulerProcfsAccess = new MockSchedulerProcfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pMockSchedulerProcfsAccess;
    pMockSchedulerProcfsAccess->listProcessesResult = ZE_RESULT_ERROR_NOT_AVAILABLE; // Expect failure when calling gpuProcessCleanup()

    // First set compute unit debug mode
    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    setComputeUnitDebugModeMock(handles[0]);

    // Change mode to ZES_SCHED_MODE_EXCLUSIVE and validate
    ze_bool_t needReboot;
    zes_sched_timeslice_properties_t setConfig;
    setConfig.interval = 1000u;
    setConfig.yieldTimeout = 1000u;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesSchedulerSetTimesliceMode(handles[0], &setConfig, &needReboot));
    EXPECT_FALSE(needReboot);
    EXPECT_EQ(ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG, fixtureGetCurrentMode(handles[0]));

    delete pMockSchedulerProcfsAccess;
}

TEST_F(SysmanDeviceSchedulerFixture, GivenGetCurrentModeFailsWhenCallingzesSchedulerSetTimesliceModeThenVerifyzesSchedulerSetTimesliceModeCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, preemptTimeoutMilliSecs, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = 1000u;
        setConfig.yieldTimeout = 1000u;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot));
        EXPECT_FALSE(needReboot);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeWhenIntervalIsLessThanMinimumThenVerifyzesSchedulerSetTimesliceModeCallFails) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = minTimeoutInMicroSeconds - 1;
        setConfig.yieldTimeout = 1000u;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeWhenNoAccessToTimeSliceDurationThenVerifyzesSchedulerSetTimesliceModeCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, timesliceDurationMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = minTimeoutInMicroSeconds;
        setConfig.yieldTimeout = 1000u;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenNoAccessToHeartBeatIntervalWhenSettingTimesliceModeThenThenVerifyzesSchedulerSetTimesliceModeCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, heartbeatIntervalMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = minTimeoutInMicroSeconds;
        setConfig.yieldTimeout = 1000u;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenHeartBeatIntervalFileNotPresentWhenSettingHeartbeatIntervalThenThenCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, heartbeatIntervalMilliSecs, false, S_IRUSR | S_IRGRP | S_IROTH);
             });

    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    auto pSchedulerImp = static_cast<SchedulerImp *>(Scheduler::fromHandle(handles[0]));
    auto pOsScheduler = static_cast<PublicLinuxSchedulerImp *>((pSchedulerImp->pOsScheduler).get());
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pOsScheduler->setHeartbeatInterval(2000));
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetExclusiveModeThenVerifyCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        ze_result_t result = zesSchedulerSetExclusiveMode(handle, &needReboot);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(needReboot);
        auto mode = fixtureGetCurrentMode(handle);
        EXPECT_EQ(mode, ZES_SCHED_MODE_EXCLUSIVE);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenCurrentModeIsDebugModeWhenCallingzesSchedulerSetExclusiveModeThenVerifyCallSucceeds) {
    VariableBackup<ProcfsAccess *> backup(&pLinuxSysmanImp->pProcfsAccess);
    auto pMockSchedulerProcfsAccess = new MockSchedulerProcfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pMockSchedulerProcfsAccess;

    // First set compute unit debug mode
    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    ze_bool_t needReload;
    setComputeUnitDebugModeMock(handles[0]);

    // Change mode to ZES_SCHED_MODE_EXCLUSIVE and validate
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesSchedulerSetExclusiveMode(handles[0], &needReload));
    EXPECT_FALSE(needReload);
    EXPECT_EQ(ZES_SCHED_MODE_EXCLUSIVE, fixtureGetCurrentMode(handles[0]));

    delete pMockSchedulerProcfsAccess;
}

TEST_F(SysmanDeviceSchedulerFixture, GivenCurrentModeIsDebugModeWhenSettingExclusiveModeAndDebugModeCantBeChangedThenVerifyCallFails) {
    VariableBackup<ProcfsAccess *> backup(&pLinuxSysmanImp->pProcfsAccess);
    auto pMockSchedulerProcfsAccess = new MockSchedulerProcfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pMockSchedulerProcfsAccess;
    pMockSchedulerProcfsAccess->listProcessesResult = ZE_RESULT_ERROR_NOT_AVAILABLE; // Expect failure when calling gpuProcessCleanup()

    // First set compute unit debug mode
    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    ze_bool_t needReload;
    setComputeUnitDebugModeMock(handles[0]);

    // Change mode to ZES_SCHED_MODE_EXCLUSIVE and validate
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesSchedulerSetExclusiveMode(handles[0], &needReload));
    EXPECT_FALSE(needReload);
    EXPECT_EQ(ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG, fixtureGetCurrentMode(handles[0]));

    delete pMockSchedulerProcfsAccess;
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetExclusiveModeWhenPreEmptTimeoutNotAvailableThenVerifyzesSchedulerSetExclusiveModeCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, preemptTimeoutMilliSecs, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        ze_result_t result = zesSchedulerSetExclusiveMode(handle, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetExclusiveModeWhenTimeSliceDurationNotAvailableThenVerifyzesSchedulerSetExclusiveModeCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, timesliceDurationMilliSecs, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        ze_result_t result = zesSchedulerSetExclusiveMode(handle, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetExclusiveModeWhenTimeSliceDurationHasNoPermissionsThenVerifyzesSchedulerSetExclusiveModeCallFails) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, timesliceDurationMilliSecs, true, S_IRUSR | S_IRGRP | S_IROTH);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        ze_result_t result = zesSchedulerSetExclusiveMode(handle, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetCurrentModeWhenPreEmptTimeOutNotAvailableThenFailureIsReturned) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, preemptTimeoutMilliSecs, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_sched_mode_t mode;
        ze_result_t result = zesSchedulerGetCurrentMode(handle, &mode);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetCurrentModeWhenTimeSliceDurationNotAvailableThenFailureIsReturned) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, timesliceDurationMilliSecs, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_sched_mode_t mode;
        ze_result_t result = zesSchedulerGetCurrentMode(handle, &mode);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetCurrentModeWhenHeartBeatIntervalNotAvailableThenFailureIsReturned) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, heartbeatIntervalMilliSecs, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_sched_mode_t mode;
        ze_result_t result = zesSchedulerGetCurrentMode(handle, &mode);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetCurrentModeWhenTimeSliceDurationHasNoPermissionThenFailureIsReturned) {
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, timesliceDurationMilliSecs, true, S_IRGRP | S_IROTH | S_IWUSR);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_sched_mode_t mode;
        ze_result_t result = zesSchedulerGetCurrentMode(handle, &mode);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetComputeUnitDebugModeThenSuccessIsReturned) {
    VariableBackup<ProcfsAccess *> backup(&pLinuxSysmanImp->pProcfsAccess);
    auto pMockSchedulerProcfsAccess = new MockSchedulerProcfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pMockSchedulerProcfsAccess;

    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    uint64_t val = 0;
    pSysfsAccess->read(enableEuDebug, val);
    EXPECT_EQ(val, 0u);

    ze_bool_t needReload;
    val = 0;
    pSysfsAccess->write(enableEuDebug, val);
    ze_result_t result = zesSchedulerSetComputeUnitDebugMode(handles[0], &needReload);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    pSysfsAccess->read(enableEuDebug, val);
    EXPECT_EQ(val, 1u);
    delete pMockSchedulerProcfsAccess;
}

TEST_F(SysmanDeviceSchedulerFixture, GivenGpuProcessCleanupFailedWhenCallingzesSchedulerSetComputeUnitDebugModeThenErrorIsReturned) {
    VariableBackup<ProcfsAccess *> backup(&pLinuxSysmanImp->pProcfsAccess);
    auto pMockSchedulerProcfsAccess = new MockSchedulerProcfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pMockSchedulerProcfsAccess;
    pMockSchedulerProcfsAccess->listProcessesResult = ZE_RESULT_ERROR_NOT_AVAILABLE; // Expect failure when calling gpuProcessCleanup()

    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    uint64_t val = 0;
    pSysfsAccess->read(enableEuDebug, val);
    EXPECT_EQ(val, 0u);

    ze_bool_t needReload;
    val = 0;
    pSysfsAccess->write(enableEuDebug, val);
    ze_result_t result = zesSchedulerSetComputeUnitDebugMode(handles[0], &needReload);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    pSysfsAccess->read(enableEuDebug, val);
    EXPECT_EQ(val, 0u);
    delete pMockSchedulerProcfsAccess;
}

TEST_F(SysmanDeviceSchedulerFixture, GivenEuDebugNodeWriteFailsWhenCallingzesSchedulerSetComputeUnitDebugModeThenErrorIsReturned) {
    VariableBackup<ProcfsAccess *> backup(&pLinuxSysmanImp->pProcfsAccess);
    auto pMockSchedulerProcfsAccess = new MockSchedulerProcfsAccess;
    pLinuxSysmanImp->pProcfsAccess = pMockSchedulerProcfsAccess;

    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    uint64_t val = 0;
    pSysfsAccess->read(enableEuDebug, val);
    EXPECT_EQ(val, 0u);

    ze_bool_t needReload;
    val = 0;
    pSysfsAccess->write(enableEuDebug, val);
    pSysfsAccess->writeCalled = 0;                                             // Lets initialize write methods call times to zero from this point for this test
    pSysfsAccess->writeResult.clear();                                         // Initialize write Results
    pSysfsAccess->writeResult.push_back(ZE_RESULT_SUCCESS);                    // This write call would be success for setting exclusive mode
    pSysfsAccess->writeResult.push_back(ZE_RESULT_SUCCESS);                    // This write call would be success for setting exclusive mode
    pSysfsAccess->writeResult.push_back(ZE_RESULT_SUCCESS);                    // This write call would be success for setting exclusive mode
    pSysfsAccess->writeResult.push_back(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE); // Failure while writing eu debug enable node
    ze_result_t result = zesSchedulerSetComputeUnitDebugMode(handles[0], &needReload);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, result);
    pSysfsAccess->read(enableEuDebug, val);
    EXPECT_EQ(val, 0u);
    delete pMockSchedulerProcfsAccess;
}

TEST_F(SysmanDeviceSchedulerFixture, GivenNodeRequiredToEnableEuDebugNotPresentWhenCheckingForDebugModeThenCallReturnsFalse) {
    auto handles = getSchedHandles(handleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    auto pSchedulerImp = static_cast<SchedulerImp *>(Scheduler::fromHandle(handles[0]));
    auto pOsScheduler = static_cast<PublicLinuxSchedulerImp *>((pSchedulerImp->pOsScheduler).get());
    std::string dummy;
    pSysfsAccess->setFileProperties(dummy, enableEuDebug, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    EXPECT_FALSE(pOsScheduler->isComputeUnitDebugModeEnabled());
}

TEST_F(SysmanDeviceSchedulerFixture, GivenPrelimEnableEuDebugNodeNotAvailableWhenCallingzesSchedulerSetComputeUnitDebugModeThenErrorReturned) {
    std::string dummy;
    pSysfsAccess->setFileProperties(dummy, enableEuDebug, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReload;
        ze_result_t result = zesSchedulerSetComputeUnitDebugMode(handle, &needReload);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenExclusiveModeSetFailWhenCallingzesSchedulerSetComputeUnitDebugModeThenErrorReturned1) {
    // Fail exclusive mode set due to preemptTimeoutMilliSecs file not available
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, preemptTimeoutMilliSecs, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReload;
        ze_result_t result = zesSchedulerSetComputeUnitDebugMode(handle, &needReload);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenExclusiveModeSetFailWhenCallingzesSchedulerSetComputeUnitDebugModeThenErrorReturned2) {
    // Fail exclusive mode set due to timesliceDurationMilliSecs file not available
    for_each(listOfMockedEngines.begin(), listOfMockedEngines.end(),
             [=](std::string engineName) {
                 pSysfsAccess->setFileProperties(engineName, timesliceDurationMilliSecs, false, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
             });
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReload;
        ze_result_t result = zesSchedulerSetComputeUnitDebugMode(handle, &needReload);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimeoutModePropertiesWithDefaultsWhenSysfsNodeIsAbsentThenFailureIsReturned) {

    pSysfsAccess->mockGetValueForError = true;

    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_sched_timeout_properties_t config;
        ze_result_t result = zesSchedulerGetTimeoutModeProperties(handle, true, &config);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetTimesliceModePropertiesWithDefaultsWhenSysfsNodeIsAbsentThenFailureIsReturned) {

    pSysfsAccess->mockGetValueForError = true;

    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_sched_timeslice_properties_t config;
        ze_result_t result = zesSchedulerGetTimesliceModeProperties(handle, true, &config);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeWhenSysfsNodeIsAbsentThenFailureIsReturned) {

    pSysfsAccess->mockGetValueForErrorWhileWrite = true;

    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = 1000u;
        setConfig.yieldTimeout = 1000u;
        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerSetTimesliceModeWhenSysfsNodeWithoutPermissionsThenFailureIsReturned) {

    pSysfsAccess->mockGetValueForErrorWhileWrite = true;

    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        ze_bool_t needReboot;
        zes_sched_timeslice_properties_t setConfig;
        setConfig.interval = 1000u;
        setConfig.yieldTimeout = 1000u;

        pSysfsAccess->writeCalled = 0;     // Lets initialize write methods call times to zero from this point for this test
        pSysfsAccess->writeResult.clear(); // Initialize write Results
        pSysfsAccess->writeResult.push_back(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);

        ze_result_t result = zesSchedulerSetTimesliceMode(handle, &setConfig, &needReboot);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, result);
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidDeviceHandleWhenCallingzesSchedulerGetPropertiesThenVerifyzesSchedulerGetPropertiesCallSucceeds) {
    auto handles = getSchedHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_sched_properties_t properties = {};
        ze_result_t result = zesSchedulerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_TRUE(properties.canControl);
        EXPECT_LE(properties.engines, ZES_ENGINE_TYPE_FLAG_RENDER);
        EXPECT_EQ(properties.supportedModes, static_cast<uint32_t>((1 << ZES_SCHED_MODE_TIMEOUT) | (1 << ZES_SCHED_MODE_TIMESLICE) | (1 << ZES_SCHED_MODE_EXCLUSIVE)));
    }
}

TEST_F(SysmanDeviceSchedulerFixture, GivenValidObjectsOfClassSchedulerImpAndSchedulerHandleContextThenDuringObjectReleaseCheckDestructorBranches) {
    for (auto &handle : pSysmanDeviceImp->pSchedulerHandleContext->handleList) {
        auto pSchedulerImp = static_cast<SchedulerImp *>(handle.get());
        pSchedulerImp->pOsScheduler = nullptr;
        handle = nullptr;
    }
}

TEST_F(SysmanMultiDeviceFixture, GivenValidDevicePointerWhenGettingSchedPropertiesThenValidSchedPropertiesRetrieved) {
    zes_sched_properties_t properties = {};
    std::vector<std::string> listOfEngines;
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(device)->getProperties(&deviceProperties);
    LinuxSchedulerImp *pLinuxSchedulerImp = new LinuxSchedulerImp(pOsSysman, ZES_ENGINE_TYPE_FLAG_COMPUTE, listOfEngines,
                                                                  deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSchedulerImp->getProperties(properties));
    EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
    EXPECT_EQ(properties.onSubdevice, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
    delete pLinuxSchedulerImp;
}

class SysmanMultiDeviceSchedulerFixture : public SysmanMultiDeviceFixture {

  protected:
    std::vector<ze_device_handle_t> deviceHandles;
    std::unique_ptr<MockSchedulerNeoDrm> pDrm;
    Drm *pOriginalDrm = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();
        pDrm = std::make_unique<MockSchedulerNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(neoDevice->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        pOriginalDrm = pLinuxSysmanImp->pDrm;
        pLinuxSysmanImp->pDrm = pDrm.get();

        pSysmanDeviceImp->pSchedulerHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;

        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pDrm->sysmanQueryEngineInfo();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pDrm = pOriginalDrm;
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_sched_handle_t> getSchedHandles(uint32_t count) {
        std::vector<zes_sched_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumSchedulers(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanMultiDeviceSchedulerFixture, GivenValidSchedulerHandleContextWhenInitializingForIncorrectDistanceInfoVerifyInvalidEngineTypeIsNotReturned) {
    auto pSchedulerHandleContextTest = std::make_unique<SchedulerHandleContext>(pOsSysman);
    pSchedulerHandleContextTest->init(deviceHandles);
    for (auto &handle : pSchedulerHandleContextTest->handleList) {
        zes_sched_properties_t properties = {};
        ze_result_t result = zesSchedulerGetProperties(handle->toHandle(), &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_TRUE(ZES_ENGINE_TYPE_FLAG_RENDER == properties.engines ||
                    ZES_ENGINE_TYPE_FLAG_COMPUTE == properties.engines ||
                    ZES_ENGINE_TYPE_FLAG_MEDIA == properties.engines ||
                    ZES_ENGINE_TYPE_FLAG_DMA == properties.engines ||
                    ZES_ENGINE_TYPE_FLAG_OTHER == properties.engines)
            << "Where Engines: " << properties.engines
            << " not equal ZES_ENGINE_TYPE_FLAG_RENDER : " << ZES_ENGINE_TYPE_FLAG_RENDER
            << " not equal ZES_ENGINE_TYPE_FLAG_COMPUTE : " << ZES_ENGINE_TYPE_FLAG_COMPUTE
            << " not equal ZES_ENGINE_TYPE_FLAG_DMA : " << ZES_ENGINE_TYPE_FLAG_DMA
            << " not equal ZES_ENGINE_TYPE_FLAG_MEDIA : " << ZES_ENGINE_TYPE_FLAG_MEDIA
            << " not equal ZES_ENGINE_TYPE_FLAG_OTHER : " << ZES_ENGINE_TYPE_FLAG_OTHER
            << ".";
    }
}

TEST_F(SysmanMultiDeviceSchedulerFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumSchedulersThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumSchedulers(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCountForMultiDeviceFixture);

    uint32_t testcount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumSchedulers(device->toHandle(), &testcount, NULL));
    EXPECT_EQ(testcount, count);

    count = 0;
    std::vector<zes_sched_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumSchedulers(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCountForMultiDeviceFixture);
}

TEST_F(SysmanMultiDeviceSchedulerFixture, GivenquerEngineInfoReturnsFalseWhenCallingzesDeviceEnumSchedulersThenZeroCountIsReturnedAndVerifyCallSucceeds) {

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumSchedulers(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCountForMultiDeviceFixture);
}

TEST_F(SysmanMultiDeviceSchedulerFixture, GivenComponentCountZeroWithEngineInfoResetWhenCallingzesDeviceEnumSchedulersThenZeroCountIsReturnedAndVerifyCallSucceeds) {
    pDrm->resetEngineInfo();
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumSchedulers(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, 0u);
}

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_execution_environment.h"

#include "level_zero/sysman/test/unit_tests/sources/global_operations/linux/mock_global_operations.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

namespace L0 {

namespace Sysman {

namespace ult {
class SysmanGlobalOperationsFixtureXe : public SysmanDeviceFixture {
  protected:
    MockGlobalOperationsFsAccess *pFsAccess = nullptr;
    MockGlobalOperationsProcfsAccess *pProcfsAccess = nullptr;
    MockGlobalOperationsSysfsAccess *pSysfsAccess = nullptr;
    L0::Sysman::SysmanDeviceImp *device = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime = MockOSTime::create();
        pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime->setDeviceTimerResolution();

        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysfsAccess = new MockGlobalOperationsSysfsAccess();
        pProcfsAccess = new MockGlobalOperationsProcfsAccess();
        pFsAccess = new MockGlobalOperationsFsAccess();
        pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
        pSysmanKmdInterface->pProcfsAccess.reset(pProcfsAccess);
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

        auto pDrmLocal = new DrmGlobalOpsMock(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrmLocal->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        auto &osInterfaceLocal = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterfaceLocal->setDriverModel(std::unique_ptr<DrmGlobalOpsMock>(pDrmLocal));

        pLinuxSysmanImp->pSysfsAccess = pSysmanKmdInterface->pSysfsAccess.get();
        pLinuxSysmanImp->pProcfsAccess = pSysmanKmdInterface->pProcfsAccess.get();
        pLinuxSysmanImp->pFsAccess = pSysmanKmdInterface->pFsAccess.get();
        pFsAccess->mockReadVal = driverVersion;
        device = pSysmanDeviceImp;
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
    void initGlobalOps() {
        zes_device_state_t deviceState = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    }
};

TEST_F(SysmanGlobalOperationsFixtureXe, GivenValidDeviceHandleWhenCallingDeviceGetStateThenVerifyDeviceIsNotWedged) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    zes_device_state_t deviceState = {};
    ze_result_t result = zesDeviceGetState(pSysmanDevice, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, deviceState.reset & ZES_RESET_REASON_FLAG_WEDGED);
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenValidDeviceHandleWhileRetrievingInformationAboutHostProcessesUsingDeviceThenSuccessIsReturned) {

    pProcfsAccess->ourDevicePid = pProcfsAccess->extraPid;
    pProcfsAccess->ourDeviceFd = pProcfsAccess->extraFd;
    pProcfsAccess->ourDeviceFd1 = pProcfsAccess->extraFd1;
    pProcfsAccess->mockListProcessCall.push_back(DEVICE_IN_USE);
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesDeviceProcessesGetState(device, &count, nullptr));
    EXPECT_EQ(count, 1u);
    std::vector<zes_process_state_t> processes(count);
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesDeviceProcessesGetState(device, &count, processes.data()));
    EXPECT_EQ(processes[0].processId, static_cast<uint32_t>(pProcfsAccess->extraPid));
    constexpr int64_t expectedEngines = ZES_ENGINE_TYPE_FLAG_DMA | ZES_ENGINE_TYPE_FLAG_COMPUTE;
    EXPECT_EQ(processes[0].engines, expectedEngines);
    uint64_t expectedMemSize = (120 * MemoryConstants::megaByte) + (50 * MemoryConstants::kiloByte) + 534 + ((50 * MemoryConstants::megaByte));
    uint64_t expectedSharedSize = (120 * MemoryConstants::megaByte) + (80 * MemoryConstants::megaByte) + (120 * MemoryConstants::kiloByte) + 689;
    EXPECT_EQ(processes[0].memSize, expectedMemSize);
    EXPECT_EQ(processes[0].sharedSize, expectedSharedSize);
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenSrcVersionFileIsPresentWhenCallingZesDeviceGetPropertiesForCheckingDriverVersionThenZesDeviceGetPropertiesCallSucceedsAndDriverVersionIsReturned) {
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    pFsAccess->mockReadVal = srcVersion;
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == srcVersion.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenSrcVersionFileIsAbsentWhenCallingZesDeviceGetPropertiesForCheckingDriverVersionThenZesDeviceGetPropertiesCallSucceedsAndUnknownDriverVersionIsReturned) {
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == unknown.compare(properties.driverVersion));
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenDeviceInFdoModeWhenCallingDeviceGetStateWithExtensionThenAllFlagsAreSet) {
    // Set device in FDO mode via sysfs mock - this is sufficient to set all three flags
    pSysfsAccess->mockFdoModeValue = "enabled";

    zes_device_state_t deviceState = {};
    deviceState.stype = ZES_STRUCTURE_TYPE_DEVICE_STATE;
    zes_intel_device_state_exp_t extState = {};
    extState.stype = ZES_INTEL_STRUCTURE_TYPE_DEVICE_STATE_EXP;
    extState.pNext = nullptr;
    deviceState.pNext = &extState;

    ze_result_t result = zesDeviceGetState(device, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t expectedFlags = ZES_INTEL_DEVICE_STATE_FLAG_EXP_WEDGED | ZES_INTEL_DEVICE_STATE_FLAG_EXP_SURVIVABILITY | ZES_INTEL_DEVICE_STATE_FLAG_EXP_FLASH_OVERRIDE;
    EXPECT_EQ(expectedFlags, extState.flags);
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenDeviceInSurvivabilityModeButNotFdoWhenCallingDeviceGetStateWithExtensionThenWedgedAndSurvivabilityFlagsAreSet) {
    pSysfsAccess->mockFdoModeValue = "disabled";
    pSysfsAccess->mockSurvivabilityModeValue = "Runtime";

    zes_device_state_t deviceState = {};
    deviceState.stype = ZES_STRUCTURE_TYPE_DEVICE_STATE;
    zes_intel_device_state_exp_t extState = {};
    extState.stype = ZES_INTEL_STRUCTURE_TYPE_DEVICE_STATE_EXP;
    extState.pNext = nullptr;
    deviceState.pNext = &extState;

    ze_result_t result = zesDeviceGetState(device, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // In survivability mode (not FDO), wedged and survivability flags should be set
    uint32_t expectedFlags = ZES_INTEL_DEVICE_STATE_FLAG_EXP_WEDGED | ZES_INTEL_DEVICE_STATE_FLAG_EXP_SURVIVABILITY;
    EXPECT_EQ(expectedFlags, extState.flags);
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenDeviceOnlyWedgedWhenCallingDeviceGetStateWithExtensionThenOnlyWedgedFlagIsSet) {
    pSysfsAccess->mockFdoModeValue = "disabled";
    pSysfsAccess->mockSurvivabilityModeValue = "";
    pLinuxSysmanImp->isDeviceInWedgedState = true;

    zes_device_state_t deviceState = {};
    deviceState.stype = ZES_STRUCTURE_TYPE_DEVICE_STATE;
    zes_intel_device_state_exp_t extState = {};
    extState.stype = ZES_INTEL_STRUCTURE_TYPE_DEVICE_STATE_EXP;
    extState.pNext = nullptr;
    deviceState.pNext = &extState;

    ze_result_t result = zesDeviceGetState(device, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // Only wedged flag should be set
    uint32_t expectedFlags = ZES_INTEL_DEVICE_STATE_FLAG_EXP_WEDGED;
    EXPECT_EQ(expectedFlags, extState.flags);
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenDeviceInNormalStateWhenCallingDeviceGetStateWithExtensionThenNoFlagsAreSet) {
    pSysfsAccess->mockFdoModeValue = "disabled";
    pSysfsAccess->mockSurvivabilityModeValue = "";
    pLinuxSysmanImp->isDeviceInWedgedState = false;

    zes_device_state_t deviceState = {};
    deviceState.stype = ZES_STRUCTURE_TYPE_DEVICE_STATE;
    zes_intel_device_state_exp_t extState = {};
    extState.stype = ZES_INTEL_STRUCTURE_TYPE_DEVICE_STATE_EXP;
    extState.pNext = nullptr;
    deviceState.pNext = &extState;

    ze_result_t result = zesDeviceGetState(device, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // No flags should be set in normal state
    EXPECT_EQ(0u, extState.flags);
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenNullExtensionPointerWhenCallingDeviceGetStateThenSuccessIsReturned) {
    pSysfsAccess->mockFdoModeValue = "disabled";
    pSysfsAccess->mockSurvivabilityModeValue = "";
    pLinuxSysmanImp->isDeviceInWedgedState = false;

    zes_device_state_t deviceState = {};
    deviceState.stype = ZES_STRUCTURE_TYPE_DEVICE_STATE;
    deviceState.pNext = nullptr;

    ze_result_t result = zesDeviceGetState(device, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenExtensionInPNextChainWhenCallingDeviceGetStateThenExtensionIsPopulated) {
    pSysfsAccess->mockFdoModeValue = "enabled";

    zes_device_state_t deviceState = {};
    deviceState.stype = ZES_STRUCTURE_TYPE_DEVICE_STATE;
    zes_intel_device_state_exp_t extState = {};
    extState.stype = ZES_INTEL_STRUCTURE_TYPE_DEVICE_STATE_EXP;
    extState.pNext = nullptr;
    extState.flags = 0xFFFFFFFF; // Set to non-zero to verify it gets initialized

    deviceState.pNext = &extState;

    ze_result_t result = zesDeviceGetState(device, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // Verify flags were initialized and then set correctly
    uint32_t expectedFlags = ZES_INTEL_DEVICE_STATE_FLAG_EXP_WEDGED | ZES_INTEL_DEVICE_STATE_FLAG_EXP_SURVIVABILITY | ZES_INTEL_DEVICE_STATE_FLAG_EXP_FLASH_OVERRIDE;
    EXPECT_EQ(expectedFlags, extState.flags);
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenNonMatchingExtensionInPNextChainWhenCallingDeviceGetStateThenExtensionIsSkipped) {
    pSysfsAccess->mockFdoModeValue = "enabled";

    zes_device_state_t deviceState = {};
    deviceState.stype = ZES_STRUCTURE_TYPE_DEVICE_STATE;

    zes_intel_device_state_exp_t extState = {};
    extState.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
    extState.pNext = nullptr;
    extState.flags = 0xFFFFFFFF; // Set to verify it doesn't get modified

    deviceState.pNext = &extState;

    ze_result_t result = zesDeviceGetState(device, &deviceState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0xFFFFFFFFu, extState.flags);
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenSurvivabilityModeStringIsBootWhenCallingIsDeviceInSurvivabilityModeThenTrueIsReturned) {
    pSysfsAccess->mockSurvivabilityModeValue = "Boot";
    bool result = pSysmanKmdInterface->isDeviceInSurvivabilityMode();
    EXPECT_TRUE(result);
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenSurvivabilityModeStringIsEmptyWhenCallingIsDeviceInSurvivabilityModeThenFalseIsReturned) {
    pSysfsAccess->mockSurvivabilityModeValue = "";
    bool result = pSysmanKmdInterface->isDeviceInSurvivabilityMode();
    EXPECT_FALSE(result);
}

TEST_F(SysmanGlobalOperationsFixtureXe, GivenSysfsReadFailsWhenCallingIsDeviceInSurvivabilityModeThenFalseIsReturned) {
    pSysfsAccess->mockReadError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    bool result = pSysmanKmdInterface->isDeviceInSurvivabilityMode();
    EXPECT_FALSE(result);
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenSysfsReturnsOkWhenCallingZesIntelDeviceGetHealthExpThenOkStatusIsReturned) {
    pSysfsAccess->mockGpuHealthVal = "ok";
    zes_intel_device_health_status_exp_t health = ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_FORCE_UINT32;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDeviceGetHealthExp(device->toHandle(), &health));
    EXPECT_EQ(ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_OK, health);
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenSysfsReturnsWarningWhenCallingZesIntelDeviceGetHealthExpThenWarningStatusIsReturned) {
    pSysfsAccess->mockGpuHealthVal = "warning";
    zes_intel_device_health_status_exp_t health = ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_FORCE_UINT32;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDeviceGetHealthExp(device->toHandle(), &health));
    EXPECT_EQ(ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_WARNING, health);
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenSysfsReturnsCriticalWhenCallingZesIntelDeviceGetHealthExpThenCriticalStatusIsReturned) {
    pSysfsAccess->mockGpuHealthVal = "critical";
    zes_intel_device_health_status_exp_t health = ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_FORCE_UINT32;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDeviceGetHealthExp(device->toHandle(), &health));
    EXPECT_EQ(ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_CRITICAL, health);
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenSysfsReturnsFailedWhenCallingZesIntelDeviceGetHealthExpThenFailedStatusIsReturned) {
    pSysfsAccess->mockGpuHealthVal = "failed";
    zes_intel_device_health_status_exp_t health = ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_FORCE_UINT32;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDeviceGetHealthExp(device->toHandle(), &health));
    EXPECT_EQ(ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_FAILED, health);
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenSysfsReadFailsWhenCallingZesIntelDeviceGetHealthExpThenErrorIsReturned) {
    pSysfsAccess->mockReadError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_intel_device_health_status_exp_t health = ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_FORCE_UINT32;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelDeviceGetHealthExp(device->toHandle(), &health));
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenSysfsReturnsUnknownStringWhenCallingZesIntelDeviceGetHealthExpThenUnknownErrorIsReturned) {
    pSysfsAccess->mockGpuHealthVal = "unknown_value";
    zes_intel_device_health_status_exp_t health = ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_FORCE_UINT32;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelDeviceGetHealthExp(device->toHandle(), &health));
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenOkHealthStatusWhenCallingZesIntelDeviceSetHealthExpThenOkIsWrittenToSysfs) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDeviceSetHealthExp(device->toHandle(), ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_OK, nullptr, 0, nullptr));
    EXPECT_EQ("ok", pSysfsAccess->mockGpuHealthWrittenVal);
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenWarningHealthStatusWhenCallingZesIntelDeviceSetHealthExpThenWarningIsWrittenToSysfs) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDeviceSetHealthExp(device->toHandle(), ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_WARNING, nullptr, 0, nullptr));
    EXPECT_EQ("warning", pSysfsAccess->mockGpuHealthWrittenVal);
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenCriticalHealthStatusWhenCallingZesIntelDeviceSetHealthExpThenCriticalIsWrittenToSysfs) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDeviceSetHealthExp(device->toHandle(), ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_CRITICAL, nullptr, 0, nullptr));
    EXPECT_EQ("critical", pSysfsAccess->mockGpuHealthWrittenVal);
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenFailedHealthStatusWhenCallingZesIntelDeviceSetHealthExpThenFailedIsWrittenToSysfs) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDeviceSetHealthExp(device->toHandle(), ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_FAILED, nullptr, 0, nullptr));
    EXPECT_EQ("failed", pSysfsAccess->mockGpuHealthWrittenVal);
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenInvalidHealthEnumWhenCallingZesIntelDeviceSetHealthExpThenInvalidArgumentIsReturned) {
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesIntelDeviceSetHealthExp(device->toHandle(), static_cast<zes_intel_device_health_status_exp_t>(0xFF), nullptr, 0, nullptr));
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenSysfsWriteFailsWhenCallingZesIntelDeviceSetHealthExpThenErrorIsPropagated) {
    pSysfsAccess->mockWriteError = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesIntelDeviceSetHealthExp(device->toHandle(), ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_OK, nullptr, 0, nullptr));
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenReasonStringExceeds256CharsWhenCallingZesIntelDeviceSetHealthExpThenInvalidArgumentIsReturned) {
    std::string longReason(257, 'x');
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesIntelDeviceSetHealthExp(device->toHandle(), ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_OK, longReason.c_str(), 0, nullptr));
}

TEST_F(SysmanGlobalOperationsFixtureXe,
       GivenValidReasonStringWhenCallingZesIntelDeviceSetHealthExpThenSuccessIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDeviceSetHealthExp(device->toHandle(), ZES_INTEL_DEVICE_HEALTH_STATUS_EXP_OK, "scheduled maintenance", 0, nullptr));
    EXPECT_EQ("ok", pSysfsAccess->mockGpuHealthWrittenVal);
}

} // namespace ult
} // namespace Sysman
} // namespace L0

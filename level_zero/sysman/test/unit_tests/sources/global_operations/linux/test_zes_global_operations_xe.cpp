/*
 * Copyright (C) 2024 Intel Corporation
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
        zes_device_state_t deviceState;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    }
};

TEST_F(SysmanGlobalOperationsFixtureXe, GivenValidDeviceHandleWhenCallingDeviceGetStateThenVerifyDeviceIsNotWedged) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    zes_device_state_t deviceState;
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

} // namespace ult
} // namespace Sysman
} // namespace L0

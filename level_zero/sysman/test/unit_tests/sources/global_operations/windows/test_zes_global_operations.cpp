/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/api/global_operations/windows/sysman_os_global_operations_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/global_operations/windows/mock_global_operations.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanGlobalOperationsFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::OsGlobalOperations *pOsGlobalOperationsPrev = nullptr;
    L0::Sysman::GlobalOperations *pGlobalOperationsPrev = nullptr;
    L0::Sysman::GlobalOperationsImp *pGlobalOperationsImp;

    std::unique_ptr<MockGlobalOpsKmdSysManager> pKmdSysManager = nullptr;
    L0::Sysman::KmdSysManager *pOriginalKmdSysManager = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime = MockOSTime::create();
        pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime->setDeviceTimerResolution();
    }

    void init(bool allowSetCalls) {
        pKmdSysManager.reset(new MockGlobalOpsKmdSysManager);

        pKmdSysManager->allowSetCalls = allowSetCalls;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pGlobalOperationsImp = static_cast<L0::Sysman::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
    }

    void TearDown() override {
        if (nullptr != pGlobalOperationsImp->pOsGlobalOperations) {
            delete pGlobalOperationsImp->pOsGlobalOperations;
        }
        pGlobalOperationsImp->pOsGlobalOperations = pOsGlobalOperationsPrev;
        pGlobalOperationsImp = nullptr;
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanGlobalOperationsFixture, GivenForceTrueAndDeviceInUseWhenCallingResetThenSuccessIsReturned) {
    init(true);
    ze_result_t result = zesDeviceReset(pSysmanDevice->toHandle(), true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingZesDeviceGetStateThenFailureIsReturned) {
    init(true);
    zes_device_state_t pState = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetState(pSysmanDevice->toHandle(), &pState));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingZesDeviceProcessesGetStateThenFailureIsReturned) {
    init(true);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceProcessesGetState(pSysmanDevice->toHandle(), &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenProcessStartsMidResetWhenCallingResetThenSuccessIsReturned) {
    init(false);
    ze_result_t result = zesDeviceReset(pSysmanDevice->toHandle(), true);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingZesDeviceResetExtThenUnsupportedFeatureErrorIsReturned) {
    init(true);
    ze_result_t result = zesDeviceResetExt(pSysmanDevice->toHandle(), nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingZesDeviceGetSubDevicePropertiesExpThenUnsupportedIsReturned) {
    init(true);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetSubDevicePropertiesExp(pSysmanDevice->toHandle(), &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDriverGetDeviceByUuidExpWithInvalidUuidThenInvalidArgumentErrorIsReturned) {
    init(true);
    zes_uuid_t uuid = {};
    zes_device_handle_t phDevice;
    ze_bool_t onSubdevice;
    uint32_t subdeviceId;
    ze_result_t result = zesDriverGetDeviceByUuidExp(driverHandle.get(), uuid, &phDevice, &onSubdevice, &subdeviceId);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDriverGetDeviceByUuidExpWithValidUuidThenSuccessIsReturned) {
    init(true);
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    zes_uuid_t uuid;
    zes_device_handle_t phDevice;
    ze_bool_t onSubdevice;
    uint32_t subdeviceId;
    std::copy_n(std::begin(properties.core.uuid.id), ZE_MAX_DEVICE_UUID_SIZE, std::begin(uuid.id));
    result = zesDriverGetDeviceByUuidExp(driverHandle.get(), uuid, &phDevice, &onSubdevice, &subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingGetPropertiesThenCorrectCorePropertiesAreReturned) {
    init(true);
    auto pHwInfo = pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->platform.usDeviceID = 0x1234;
    const std::string deviceName = "IntelTestDevice";
    pHwInfo->capabilityTable.deviceName = &deviceName[0];

    zes_device_properties_t properties = {};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.core.type, ZE_DEVICE_TYPE_GPU);
    EXPECT_EQ(properties.core.vendorId, static_cast<uint32_t>(0x8086));
    EXPECT_EQ(properties.core.deviceId, static_cast<uint32_t>(pHwInfo->platform.usDeviceID));
    EXPECT_EQ(properties.core.coreClockRate, static_cast<uint32_t>(pHwInfo->capabilityTable.maxRenderFrequency));
    EXPECT_EQ(properties.core.maxHardwareContexts, static_cast<uint32_t>(1024 * 64));
    EXPECT_EQ(properties.core.maxCommandQueuePriority, 0u);
    EXPECT_EQ(properties.core.numThreadsPerEU, static_cast<uint32_t>(pHwInfo->gtSystemInfo.ThreadCount / pHwInfo->gtSystemInfo.EUCount));
    EXPECT_EQ(properties.core.numEUsPerSubslice, static_cast<uint32_t>(pHwInfo->gtSystemInfo.MaxEuPerSubSlice));
    EXPECT_EQ(properties.core.numSlices, static_cast<uint32_t>(pHwInfo->gtSystemInfo.SliceCount));
    EXPECT_EQ(properties.core.timestampValidBits, static_cast<uint32_t>(pHwInfo->capabilityTable.timestampValidBits));
    EXPECT_EQ(properties.core.kernelTimestampValidBits, static_cast<uint32_t>(pHwInfo->capabilityTable.kernelTimestampValidBits));
    EXPECT_TRUE(0 == deviceName.compare(properties.core.name));

    // Check if default device name is proper when HwInfo deviceName is null
    std::stringstream expectedDeviceName;
    expectedDeviceName << "Intel(R) Graphics";
    expectedDeviceName << " [0x" << std::hex << std::setw(4) << std::setfill('0') << pSysmanDevice->getHardwareInfo().platform.usDeviceID << "]";

    pHwInfo->capabilityTable.deviceName = "";
    result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(0 == expectedDeviceName.str().compare(properties.core.name));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingGetPropertiesThenCorrectTimerResolutionInCorePropertiesAreReturned) {
    init(true);
    pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime.reset(new NEO::MockOSTimeWithConstTimestamp());
    double mockedTimerResolution = pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().osTime->getDynamicDeviceTimerResolution();

    zes_device_properties_t properties = {};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.core.timerResolution, static_cast<uint64_t>(mockedTimerResolution));

    properties = {};
    ze_device_properties_t coreProperties = {};
    coreProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
    properties.core = coreProperties;
    result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.core.timerResolution, static_cast<uint64_t>(1000000000.0 / mockedTimerResolution));

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseCyclesPerSecondTimer.set(1);
    properties = {};
    result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.core.timerResolution, static_cast<uint64_t>(1000000000.0 / mockedTimerResolution));
}

TEST_F(SysmanGlobalOperationsFixture, WhenGettingDevicePropertiesThenSubslicesPerSliceIsBasedOnSubslicesSupported) {
    init(true);
    auto pHwInfo = pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->gtSystemInfo.MaxSubSlicesSupported = 48;
    pHwInfo->gtSystemInfo.MaxSlicesSupported = 3;
    pHwInfo->gtSystemInfo.SubSliceCount = 8;
    pHwInfo->gtSystemInfo.SliceCount = 1;

    zes_device_properties_t properties = {};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(8u, properties.core.numSubslicesPerSlice);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDebugApiUsedSetWhenGettingDevicePropertiesThenSubslicesPerSliceIsBasedOnMaxSubslicesSupported) {
    init(true);
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebugApiUsed.set(1);
    auto pHwInfo = pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->gtSystemInfo.MaxSubSlicesSupported = 48;
    pHwInfo->gtSystemInfo.MaxSlicesSupported = 3;
    pHwInfo->gtSystemInfo.SubSliceCount = 8;
    pHwInfo->gtSystemInfo.SliceCount = 1;

    zes_device_properties_t properties = {};
    ze_result_t result = zesDeviceGetProperties(pSysmanDevice->toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(16u, properties.core.numSubslicesPerSlice);
}

class SysmanGlobalOperationsUuidFixture : public SysmanDeviceFixture {
  public:
    L0::Sysman::GlobalOperationsImp *pGlobalOperationsImp;
    L0::Sysman::SysmanDeviceImp *device = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime = MockOSTime::create();
        pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef().osTime->setDeviceTimerResolution();
        pGlobalOperationsImp = static_cast<L0::Sysman::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        device = pSysmanDeviceImp;
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    void initGlobalOps() {
        zes_device_state_t deviceState;
        zesDeviceGetState(device, &deviceState);
    }
};

TEST_F(SysmanGlobalOperationsUuidFixture, GivenValidDeviceHandleWhenCallingGenerateUuidFromPciBusInfoThenValidUuidIsReturned) {
    initGlobalOps();

    auto pHwInfo = pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->platform.usDeviceID = 0x1234;
    pHwInfo->platform.usRevId = 0x1;

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    NEO::PhysicalDevicePciBusInfo pciBusInfo = {};
    pciBusInfo.pciDomain = 0x5678;
    pciBusInfo.pciBus = 0x9;
    pciBusInfo.pciDevice = 0xA;
    pciBusInfo.pciFunction = 0xB;

    bool result = pGlobalOperationsImp->pOsGlobalOperations->generateUuidFromPciBusInfo(pciBusInfo, uuid);
    EXPECT_EQ(true, result);
    uint8_t *pUuid = (uint8_t *)uuid.data();
    EXPECT_EQ(*((uint16_t *)pUuid), (uint16_t)0x8086);
    EXPECT_EQ(*((uint16_t *)(pUuid + 2)), (uint16_t)pHwInfo->platform.usDeviceID);
    EXPECT_EQ(*((uint16_t *)(pUuid + 4)), (uint16_t)pHwInfo->platform.usRevId);
    EXPECT_EQ(*((uint16_t *)(pUuid + 6)), (uint16_t)pciBusInfo.pciDomain);
    EXPECT_EQ((*(pUuid + 8)), (uint8_t)pciBusInfo.pciBus);
    EXPECT_EQ((*(pUuid + 9)), (uint8_t)pciBusInfo.pciDevice);
    EXPECT_EQ((*(pUuid + 10)), (uint8_t)pciBusInfo.pciFunction);
}

TEST_F(SysmanGlobalOperationsUuidFixture, GivenValidDeviceHandleWhenCallingGetUuidMultipleTimesThenSameUuidIsReturned) {
    initGlobalOps();

    auto pHwInfo = pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->platform.usDeviceID = 0x1234;
    pHwInfo->platform.usRevId = 0x1;

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid = {};

    bool result = pGlobalOperationsImp->pOsGlobalOperations->getUuid(uuid);
    EXPECT_EQ(true, result);

    uint8_t *pUuid = (uint8_t *)uuid.data();
    EXPECT_EQ(*((uint16_t *)pUuid), (uint16_t)0x8086);
    EXPECT_EQ(*((uint16_t *)(pUuid + 2)), (uint16_t)pHwInfo->platform.usDeviceID);
    EXPECT_EQ(*((uint16_t *)(pUuid + 4)), (uint16_t)pHwInfo->platform.usRevId);

    uuid = {};
    result = pGlobalOperationsImp->pOsGlobalOperations->getUuid(uuid);
    EXPECT_EQ(true, result);

    pUuid = (uint8_t *)uuid.data();
    EXPECT_EQ(*((uint16_t *)pUuid), (uint16_t)0x8086);
    EXPECT_EQ(*((uint16_t *)(pUuid + 2)), (uint16_t)pHwInfo->platform.usDeviceID);
    EXPECT_EQ(*((uint16_t *)(pUuid + 4)), (uint16_t)pHwInfo->platform.usRevId);
}

TEST_F(SysmanGlobalOperationsUuidFixture, GivenValidDeviceHandleWithInvalidPciDomainWhenCallingGenerateUuidFromPciBusInfoThenFalseIsReturned) {
    initGlobalOps();

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    NEO::PhysicalDevicePciBusInfo pciBusInfo = {};
    pciBusInfo.pciDomain = std::numeric_limits<uint32_t>::max();

    bool result = pGlobalOperationsImp->pOsGlobalOperations->generateUuidFromPciBusInfo(pciBusInfo, uuid);
    EXPECT_EQ(false, result);
}

TEST_F(SysmanGlobalOperationsUuidFixture, GivenNullOsInterfaceObjectWhenRetrievingUuidThenFalseIsReturned) {
    initGlobalOps();

    auto &rootDeviceEnvironment = (pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef());
    auto prevOsInterface = std::move(rootDeviceEnvironment.osInterface);
    rootDeviceEnvironment.osInterface = nullptr;

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    bool result = pGlobalOperationsImp->pOsGlobalOperations->getUuid(uuid);
    EXPECT_EQ(false, result);
    rootDeviceEnvironment.osInterface = std::move(prevOsInterface);
}

TEST_F(SysmanGlobalOperationsUuidFixture, GivenNullOsInterfaceObjectWhenRetrievingDeviceInfoByUuidThenFalseIsReturned) {
    initGlobalOps();

    auto &rootDeviceEnvironment = (pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef());
    auto prevOsInterface = std::move(rootDeviceEnvironment.osInterface);
    rootDeviceEnvironment.osInterface = nullptr;
    zes_uuid_t uuid = {};
    ze_bool_t onSubdevice = false;
    uint32_t subdeviceId = 0;
    bool result = pGlobalOperationsImp->pOsGlobalOperations->getDeviceInfoByUuid(uuid, &onSubdevice, &subdeviceId);
    EXPECT_FALSE(result);
    rootDeviceEnvironment.osInterface = std::move(prevOsInterface);
}

TEST_F(SysmanGlobalOperationsUuidFixture, GivenInvalidPciBusInfoWhenRetrievingUuidThenFalseIsReturned) {
    class SysmanMockWddm : public NEO::WddmMock {
      public:
        SysmanMockWddm(RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment) {}
        PhysicalDevicePciBusInfo getPciBusInfo() const override {
            PhysicalDevicePciBusInfo pciBusInfo = {};
            pciBusInfo.pciDomain = std::numeric_limits<uint32_t>::max();
            return pciBusInfo;
        }
    };
    initGlobalOps();

    auto &rootDeviceEnvironment = (pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef());

    rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(new SysmanMockWddm(rootDeviceEnvironment)));
    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid;
    bool result = pGlobalOperationsImp->pOsGlobalOperations->getUuid(uuid);
    EXPECT_EQ(false, result);
}

using SysmanDevicePropertiesFixture = SysmanGlobalOperationsUuidFixture;

TEST_F(SysmanDevicePropertiesFixture,
       GivenValidDeviceHandleWhenCallingGetPropertiesThenDeviceTypeIsGpuInCoreProperties) {
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_DEVICE_TYPE_GPU, properties.core.type);
}

TEST_F(SysmanDevicePropertiesFixture,
       GivenValidDeviceHandleWhenCallingGetPropertiesAndOnDemandPageFaultNotSupportedThenFlagIsNotSetInCoreProperties) {
    auto mockHardwareInfo = device->getHardwareInfo();
    mockHardwareInfo.capabilityTable.supportsOnDemandPageFaults = false;
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->setHwInfoAndInitHelpers(&mockHardwareInfo);
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(properties.core.flags & ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING);
}

HWTEST2_F(SysmanDevicePropertiesFixture,
          GivenValidDeviceHandleWhenCallingGetPropertiesnAndIsNotIntegratedDeviceThenFlagIsNotSetInCoreProperties, IsXeHpgCore) {
    auto mockHardwareInfo = device->getHardwareInfo();
    mockHardwareInfo.capabilityTable.isIntegratedDevice = false;
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->setHwInfoAndInitHelpers(&mockHardwareInfo);
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};

    ze_result_t result = zesDeviceGetProperties(device, &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(properties.core.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
}

} // namespace ult
} // namespace Sysman
} // namespace L0

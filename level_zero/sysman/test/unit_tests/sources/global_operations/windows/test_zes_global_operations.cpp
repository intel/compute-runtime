/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetStateThenFailureIsReturned) {
    init(true);
    zes_device_state_t pState = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetState(pSysmanDevice->toHandle(), &pState));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceProcessesGetStateThenFailureIsReturned) {
    init(true);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceProcessesGetState(pSysmanDevice->toHandle(), &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenProcessStartsMidResetWhenCallingResetThenSuccessIsReturned) {
    init(false);
    ze_result_t result = zesDeviceReset(pSysmanDevice->toHandle(), true);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingzesDeviceResetExtThenUnsupportedFeatureErrorIsReturned) {
    init(true);
    ze_result_t result = zesDeviceResetExt(pSysmanDevice->toHandle(), nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

class SysmanGlobalOperationsUuidFixture : public SysmanDeviceFixture {
  public:
    L0::Sysman::GlobalOperationsImp *pGlobalOperationsImp;
    L0::Sysman::SysmanDeviceImp *device = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
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

} // namespace ult
} // namespace Sysman
} // namespace L0

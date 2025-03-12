/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_product_helper.h"

#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleWhenCallingSysmanFromHandleThenValidSysmanDeviceIsReturned) {
    zes_device_handle_t validHandle = pSysmanDevice->toHandle();
    SysmanDevice *sysmanDevice = SysmanDevice::fromHandle(validHandle);
    EXPECT_NE(nullptr, sysmanDevice);
    EXPECT_EQ(pSysmanDevice, sysmanDevice);
}

TEST_F(SysmanDeviceFixture, GivenInvalidSysmanDeviceHandleWhenCallingSysmanFromHandleThenNullptrIsReturned) {
    zes_device_handle_t invalidHandle = nullptr;
    EXPECT_EQ(nullptr, SysmanDevice::fromHandle(invalidHandle));
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleAndGivenGlobalSysmanDriverIsNullptrWhenCallingSysmanFromHandleThenValidSysmanDeviceIsReturned) {
    zes_device_handle_t validHandle = pSysmanDevice->toHandle();
    L0::Sysman::globalSysmanDriver = nullptr;
    EXPECT_EQ(nullptr, SysmanDevice::fromHandle(validHandle));
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleAndGivenGlobalSysmanDriverIsNotNullptrWhenCallingSysmanFromHandleThenValidSysmanDeviceIsReturned) {
    zes_device_handle_t validHandle = pSysmanDevice->toHandle();
    EXPECT_EQ(pSysmanDevice, SysmanDevice::fromHandle(validHandle));
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleWhenCallingDiagnosticsGetThenSuccessIsReturned) {
    zes_device_handle_t validHandle = pSysmanDevice->toHandle();
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, SysmanDevice::diagnosticsGet(validHandle, &count, nullptr));
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleWhenCallingFabricPortGetThenSuccessIsReturned) {
    zes_device_handle_t validHandle = pSysmanDevice->toHandle();
    uint32_t count = 0;
    zes_fabric_port_handle_t *phPort = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, SysmanDevice::fabricPortGet(validHandle, &count, phPort));
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleWhenCallingPowerGetCardDomainThenUnsupportedErrorIsReturned) {
    zes_device_handle_t validHandle = pSysmanDevice->toHandle();
    zes_pwr_handle_t powerHandle = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, SysmanDevice::powerGetCardDomain(validHandle, &powerHandle));
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleWhenCallingSchedulerGetThenSuccessIsReturned) {
    zes_device_handle_t validHandle = pSysmanDevice->toHandle();
    uint32_t count = 0;
    zes_sched_handle_t *phScheduler = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, SysmanDevice::schedulerGet(validHandle, &count, phScheduler));
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleWhenCallingStandbyGetThenSuccessIsReturned) {
    zes_device_handle_t validHandle = pSysmanDevice->toHandle();
    uint32_t count = 0;
    zes_standby_handle_t *phStandby = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, SysmanDevice::standbyGet(validHandle, &count, phStandby));
}

TEST_F(SysmanDeviceFixture, GivenInvalidSysmanDeviceHandleWhenCallingSysmanDeviceFunctionsThenUninitializedErrorIsReturned) {
    zes_device_handle_t invalidHandle = nullptr;
    uint32_t count = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::performanceGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::powerGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::powerGetCardDomain(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::frequencyGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::fabricPortGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::temperatureGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::standbyGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetProperties(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetSubDeviceProperties(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::processesGetState(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceReset(invalidHandle, false));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetState(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::engineGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetProperties(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetState(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetBars(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::pciGetStats(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::schedulerGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::rasGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::memoryGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::fanGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::diagnosticsGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::firmwareGet(invalidHandle, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEventRegister(invalidHandle, uint32_t(0)));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEccAvailable(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEccConfigurable(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceGetEccState(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceSetEccState(invalidHandle, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceResetExt(invalidHandle, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::fabricPortGetMultiPortThroughput(invalidHandle, count, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, SysmanDevice::deviceEnumEnabledVF(invalidHandle, &count, nullptr));
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleWithInvalidPciDomainWhenCallingGenerateUuidFromPciBusInfoThenFalseIsReturned) {
    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid{};
    NEO::PhysicalDevicePciBusInfo pciBusInfo = {};
    pciBusInfo.pciDomain = std::numeric_limits<uint32_t>::max();
    bool result = pWddmSysmanImp->generateUuidFromPciBusInfo(pciBusInfo, uuid);
    EXPECT_EQ(false, result);
}

TEST_F(SysmanDeviceFixture, GivenNullOsInterfaceObjectWhenRetrievingUuidsOfDeviceThenNoUuidsAreReturned) {
    auto execEnv = new NEO::ExecutionEnvironment();
    execEnv->prepareRootDeviceEnvironments(1);
    execEnv->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    execEnv->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();
    execEnv->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModel>());

    auto pSysmanDeviceImp = std::make_unique<L0::Sysman::SysmanDeviceImp>(execEnv, 0);
    auto pWddmSysmanImp = static_cast<PublicWddmSysmanImp *>(pSysmanDeviceImp->pOsSysman);

    auto &rootDeviceEnvironment = (pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironmentRef());
    rootDeviceEnvironment.osInterface = nullptr;

    std::vector<std::string> uuids;
    pWddmSysmanImp->getDeviceUuids(uuids);
    EXPECT_EQ(0u, uuids.size());
}

TEST_F(SysmanDeviceFixture, GivenInvalidPciBusInfoWhenRetrievingUuidThenFalseIsReturned) {
    auto execEnv = new NEO::ExecutionEnvironment();
    execEnv->prepareRootDeviceEnvironments(1);
    execEnv->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    execEnv->rootDeviceEnvironments[0]->osInterface = std::make_unique<NEO::OSInterface>();

    auto driverModel = std::make_unique<NEO::MockDriverModel>();
    driverModel->pciSpeedInfo = {1, 1, 1};
    PhysicalDevicePciBusInfo pciBusInfo = {};
    pciBusInfo.pciDomain = std::numeric_limits<uint32_t>::max();
    driverModel->pciBusInfo = pciBusInfo;
    execEnv->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::move(driverModel));

    auto pSysmanDeviceImp = std::make_unique<L0::Sysman::SysmanDeviceImp>(execEnv, 0);
    auto pWddmSysmanImp = static_cast<PublicWddmSysmanImp *>(pSysmanDeviceImp->pOsSysman);

    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid{};
    bool result = pWddmSysmanImp->getUuid(uuid);
    EXPECT_FALSE(result);
}

TEST_F(SysmanDeviceFixture, GivenValidWddmSysmanImpWhenRetrievingUuidThenTrueIsReturned) {
    std::array<uint8_t, NEO::ProductHelper::uuidSize> uuid{};
    bool result = pWddmSysmanImp->getUuid(uuid);
    EXPECT_TRUE(result);
}

} // namespace ult
} // namespace Sysman
} // namespace L0

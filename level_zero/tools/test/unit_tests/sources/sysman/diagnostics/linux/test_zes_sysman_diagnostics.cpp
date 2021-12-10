/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/diagnostics/linux/mock_zes_sysman_diagnostics.h"

extern bool sysmanUltsEnable;

using ::testing::_;
namespace L0 {
namespace ult {

class ZesDiagnosticsFixture : public SysmanDeviceFixture {

  protected:
    zes_diag_handle_t hSysmanDiagnostics = {};
    std::unique_ptr<Mock<DiagnosticsFwInterface>> pMockFwInterface;
    FirmwareUtil *pFwUtilInterfaceOld = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pMockFwInterface = std::make_unique<NiceMock<Mock<DiagnosticsFwInterface>>>();
        pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface.get();
        ON_CALL(*pMockFwInterface.get(), fwDeviceInit())
            .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<DiagnosticsFwInterface>::mockFwDeviceInit));
        ON_CALL(*pMockFwInterface.get(), getFirstDevice(_))
            .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<DiagnosticsFwInterface>::mockGetFirstDevice));
        ON_CALL(*pMockFwInterface.get(), fwSupportedDiagTests(_))
            .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<DiagnosticsFwInterface>::mockFwSupportedDiagTests));
        for (const auto &handle : pSysmanDeviceImp->pDiagnosticsHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        std::vector<ze_device_handle_t> deviceHandles;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pSysmanDeviceImp->pDiagnosticsHandleContext->init(deviceHandles);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
    }

    std::vector<zes_diag_handle_t> get_diagnostics_handles(uint32_t &count) {
        std::vector<zes_diag_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
    void clear_and_reinit_handles(std::vector<ze_device_handle_t> &deviceHandles) {
        for (const auto &handle : pSysmanDeviceImp->pDiagnosticsHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.clear();
        pSysmanDeviceImp->pDiagnosticsHandleContext->supportedDiagTests.clear();
        uint32_t subDeviceCount = 0;

        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
    }
};

TEST_F(ZesDiagnosticsFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumDiagnosticTestSuitesThenZeroCountIsReturnedAndVerifyzesDeviceEnumDiagnosticTestSuitesCallSucceeds) {
    std::vector<zes_diag_handle_t> diagnosticsHandle{};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    diagnosticsHandle.resize(count);
    result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, diagnosticsHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
    uint32_t subDeviceCount = 0;
    std::vector<ze_device_handle_t> deviceHandles;
    // We received a device handle. Check for subdevices in this device
    Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
    if (subDeviceCount == 0) {
        deviceHandles.resize(1, device->toHandle());
    } else {
        deviceHandles.resize(subDeviceCount, nullptr);
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
    }
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0], deviceHandles[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 1u);

    testCount = count;

    diagnosticsHandle.resize(testCount);
    result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &testCount, diagnosticsHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, diagnosticsHandle.data());
    EXPECT_EQ(testCount, 1u);

    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenFwInterfaceAsNullWhenCallingzesDeviceEnumDiagnosticTestSuitesThenZeroCountIsReturnedAndVerifyzesDeviceEnumDiagnosticTestSuitesCallSucceeds) {
    auto tempFwInterface = pLinuxSysmanImp->pFwUtilInterface;
    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    std::vector<zes_diag_handle_t> diagnosticsHandle{};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);
    pLinuxSysmanImp->pFwUtilInterface = tempFwInterface;
}

TEST_F(ZesDiagnosticsFixture, GivenFailedFirmwareInitializationWhenInitializingDiagnosticsContextThenexpectNoHandles) {
    std::vector<ze_device_handle_t> deviceHandles;
    clear_and_reinit_handles(deviceHandles);
    ON_CALL(*pMockFwInterface.get(), fwDeviceInit())
        .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<DiagnosticsFwInterface>::mockFwDeviceInitFail));
    pSysmanDeviceImp->pDiagnosticsHandleContext->init(deviceHandles);

    EXPECT_EQ(0u, pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.size());
}

TEST_F(ZesDiagnosticsFixture, GivenSupportedTestsWhenInitializingDiagnosticsContextThenExpectHandles) {
    std::vector<ze_device_handle_t> deviceHandles;
    clear_and_reinit_handles(deviceHandles);
    pSysmanDeviceImp->pDiagnosticsHandleContext->supportedDiagTests.push_back(mockSupportedDiagTypes[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->init(deviceHandles);
    EXPECT_EQ(1u, pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.size());
}

TEST_F(ZesDiagnosticsFixture, GivenFirmwareInitializationFailureThenCreateHandleMustFail) {
    std::vector<ze_device_handle_t> deviceHandles;
    clear_and_reinit_handles(deviceHandles);
    ON_CALL(*pMockFwInterface.get(), fwDeviceInit())
        .WillByDefault(::testing::Invoke(pMockFwInterface.get(), &Mock<DiagnosticsFwInterface>::mockFwDeviceInitFail));
    pSysmanDeviceImp->pDiagnosticsHandleContext->init(deviceHandles);
    EXPECT_EQ(0u, pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.size());
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenGettingDiagnosticsPropertiesThenCallSucceeds) {
    std::vector<ze_device_handle_t> deviceHandles;
    clear_and_reinit_handles(deviceHandles);
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0], deviceHandles[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);

    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDiagnosticsGetProperties(handle, &properties));
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenGettingDiagnosticsTestThenCallSucceeds) {
    std::vector<ze_device_handle_t> deviceHandles;
    clear_and_reinit_handles(deviceHandles);
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0], deviceHandles[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);

    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_test_t tests = {};
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDiagnosticsGetTests(handle, &count, &tests));

    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenRunningDiagnosticsTestThenCallSucceeds) {
    std::vector<ze_device_handle_t> deviceHandles;
    clear_and_reinit_handles(deviceHandles);
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0], deviceHandles[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);

    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDiagnosticsRunTests(handle, start, end, &results));
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    delete ptestDiagnosticsImp;
}

} // namespace ult
} // namespace L0

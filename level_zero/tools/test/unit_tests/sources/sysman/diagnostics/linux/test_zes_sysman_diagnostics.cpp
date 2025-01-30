/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/diagnostics/linux/mock_zes_sysman_diagnostics.h"

extern bool sysmanUltsEnable;
namespace L0 {
namespace ult {

static int mockFileDescriptor = 123;
static int mockGtPciConfigFd = 124;

inline static int openMockDiag(const char *pathname, int flags) {
    if (strcmp(pathname, mockRealPathConfig.c_str()) == 0) {
        return mockFileDescriptor;
    } else if (strcmp(pathname, mockdeviceDirConfig.c_str()) == 0) {
        return mockGtPciConfigFd;
    }
    return -1;
}
void mockSleepFunctionSecs(int64_t secs) {
    return;
}

inline static int openMockDiagFail(const char *pathname, int flags) {
    return -1;
}

inline static int gtPciConfigOpenFail(const char *pathname, int flags) {
    if (strcmp(pathname, mockRealPathConfig.c_str()) == 0) {
        return mockFileDescriptor;
    } else {
        return -1;
    }
}

ssize_t preadMockDiag(int fd, void *buf, size_t count, off_t offset) {
    uint8_t *mockBuf = static_cast<uint8_t *>(buf);
    if (fd == mockGtPciConfigFd) {
        mockBuf[0x006] = 0x24;
        mockBuf[0x034] = 0x40;
        mockBuf[0x040] = 0x0d;
        mockBuf[0x041] = 0x50;
        mockBuf[0x050] = 0x10;
        mockBuf[0x051] = 0x70;
        mockBuf[0x052] = 0x90;
        mockBuf[0x070] = 0x10;
        mockBuf[0x071] = 0xac;
        mockBuf[0x072] = 0xa0;
        mockBuf[0x0ac] = 0x10;
        mockBuf[0x0b8] = 0x11;
        mockBuf[0x100] = 0x0e;
        mockBuf[0x102] = 0x24;
        mockBuf[0x103] = 0x42;
        mockBuf[0x420] = 0x15;
        mockBuf[0x422] = 0x01;
        mockBuf[0x423] = 0x22;
        mockBuf[0x425] = 0xf0;
        mockBuf[0x426] = 0x3f;
        mockBuf[0x428] = 0x22;
        mockBuf[0x429] = 0x11;
        mockBuf[0x220] = 0x24;
        mockBuf[0x222] = 0x24;
        mockBuf[0x223] = 0x24;
        mockBuf[0x320] = 0x10;
        mockBuf[0x322] = 0x01;
        mockBuf[0x323] = 0x40;
        mockBuf[0x400] = 0x18;
        mockBuf[0x402] = 0x01;
    }
    return count;
}

ssize_t mockGtConfigPreadInvalid(int fd, void *buf, size_t count, off_t offset) {
    return count;
}

ssize_t mockGtConfigPreadFail(int fd, void *buf, size_t count, off_t offset) {
    if (fd == mockGtPciConfigFd) {
        return -1;
    }
    return count;
}

ssize_t mockGtConfigPwriteFail(int fd, const void *buf, size_t count, off_t offset) {
    if (fd == mockGtPciConfigFd) {
        return -1;
    }
    return count;
}

ssize_t pwriteMockDiag(int fd, const void *buf, size_t count, off_t offset) {
    return count;
}
class ZesDiagnosticsFixture : public SysmanDeviceFixture {

  protected:
    zes_diag_handle_t hSysmanDiagnostics = {};
    std::unique_ptr<MockDiagnosticsFwInterface> pMockDiagFwInterface;
    std::unique_ptr<MockDiagSysfsAccess> pMockSysfsAccess;
    std::unique_ptr<MockDiagFsAccess> pMockFsAccess;
    std::unique_ptr<MockDiagProcfsAccess> pMockDiagProcfsAccess;
    std::unique_ptr<MockGlobalOperationsEngineHandleContext> pEngineHandleContext;
    std::unique_ptr<MockDiagLinuxSysmanImp> pMockDiagLinuxSysmanImp;

    FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    SysfsAccess *pSysfsAccessOld = nullptr;
    FsAccess *pFsAccessOld = nullptr;
    ProcfsAccess *pProcfsAccessOld = nullptr;
    EngineHandleContext *pEngineHandleContextOld = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pEngineHandleContextOld = pSysmanDeviceImp->pEngineHandleContext;
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pFsAccessOld = pLinuxSysmanImp->pFsAccess;
        pProcfsAccessOld = pLinuxSysmanImp->pProcfsAccess;
        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;

        pEngineHandleContext = std::make_unique<MockGlobalOperationsEngineHandleContext>(pOsSysman);
        pMockDiagFwInterface = std::make_unique<MockDiagnosticsFwInterface>();
        pMockSysfsAccess = std::make_unique<MockDiagSysfsAccess>();
        pMockFsAccess = std::make_unique<MockDiagFsAccess>();
        pMockDiagProcfsAccess = std::make_unique<MockDiagProcfsAccess>();
        pMockDiagLinuxSysmanImp = std::make_unique<MockDiagLinuxSysmanImp>(pLinuxSysmanImp->getSysmanDeviceImp());
        pSysmanDeviceImp->pEngineHandleContext = pEngineHandleContext.get();
        pLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();
        pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();
        pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
        pLinuxSysmanImp->pFwUtilInterface = pMockDiagFwInterface.get();

        pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.clear();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pSysmanDeviceImp->pEngineHandleContext = pEngineHandleContextOld;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        pLinuxSysmanImp->pFsAccess = pFsAccessOld;
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccessOld;
        SysmanDeviceFixture::TearDown();
    }

    void clearAndReinitHandles() {
        pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.clear();
        pSysmanDeviceImp->pDiagnosticsHandleContext->supportedDiagTests.clear();
    }
};

TEST_F(ZesDiagnosticsFixture, GivenComponentCountZeroWhenCallingZesDeviceEnumDiagnosticTestSuitesThenZeroCountIsReturnedAndVerifyZesDeviceEnumDiagnosticTestSuitesCallSucceeds) {

    constexpr uint32_t mockDiagHandleCount = 0;
    std::vector<zes_diag_handle_t> diagnosticsHandle{};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockDiagHandleCount);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    diagnosticsHandle.resize(count);
    result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, diagnosticsHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockDiagHandleCount);
}

TEST_F(ZesDiagnosticsFixture, GivenOneSupportedDiagnosticTestWhenCallingZesDeviceEnumDiagnosticTestSuitesThenOneCountIsReturnedAndVerifyZesDeviceEnumDiagnosticTestSuitesCallSucceeds) {

    constexpr uint32_t mockDiagHandleCount = 1;
    std::vector<zes_diag_handle_t> diagnosticsHandle{};
    uint32_t count = 0;
    pSysmanDeviceImp->pDiagnosticsHandleContext->supportedDiagTests.push_back(mockSupportedDiagTypes[0]);
    ze_result_t result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockDiagHandleCount);

    uint32_t testCount = count;

    diagnosticsHandle.resize(testCount);
    result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &testCount, diagnosticsHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, diagnosticsHandle.data());
    EXPECT_EQ(testCount, mockDiagHandleCount);

    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenGettingDiagnosticsPropertiesThenCallSucceeds) {

    clearAndReinitHandles();
    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDiagnosticsGetProperties(handle, &properties));
    EXPECT_EQ(properties.haveTests, 0);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.name, mockSupportedDiagTypes[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(SysmanMultiDeviceFixture, GivenValidDevicePointerWhenGettingDiagnosticsPropertiesThenValidDiagPropertiesRetrieved) {
    zes_diag_properties_t properties = {};
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(device)->getProperties(&deviceProperties);
    std::unique_ptr<LinuxDiagnosticsImp> pLinuxDiagnosticsImp = std::make_unique<LinuxDiagnosticsImp>(pOsSysman, mockSupportedDiagTypes[0]);
    pLinuxDiagnosticsImp->osGetDiagProperties(&properties);
    EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
    EXPECT_EQ(properties.onSubdevice, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenGettingDiagnosticsTestThenCallSucceeds) {
    clearAndReinitHandles();
    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_test_t tests = {};
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDiagnosticsGetTests(handle, &count, &tests));
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenRunningDiagnosticsTestThenCallSucceeds) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    pPublicLinuxDiagnosticsImp->osDiagType = "MEMORY_PPR";
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_NO_ERRORS);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenRunningDiagnosticsTestAndFwRunDiagTestFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();

    pMockDiagFwInterface->setDiagResult(ZES_DIAG_RESULT_FORCE_UINT32);
    pMockDiagFwInterface->mockFwRunDiagTestsResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_FORCE_UINT32);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenListProcessFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagProcfsAccess->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_FORCE_UINT32);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenQuiescentingFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockSysfsAccess->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_FORCE_UINT32);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenInvalidateLmemFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockSysfsAccess->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_FORCE_UINT32);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenColdResetFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagFwInterface->setDiagResult(ZES_DIAG_RESULT_REBOOT_FOR_REPAIR);
    pMockDiagLinuxSysmanImp->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_REBOOT_FOR_REPAIR);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenWarmResetFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagLinuxSysmanImp->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_NO_ERRORS);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenWarmResetSucceedsAndInitDeviceFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagLinuxSysmanImp->setMockInitDeviceError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_NO_ERRORS);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenColdResetSucceedsAndInitDeviceFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagFwInterface->setDiagResult(ZES_DIAG_RESULT_REBOOT_FOR_REPAIR);
    pMockDiagLinuxSysmanImp->setMockInitDeviceError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    std::unique_ptr<DiagnosticsImp> ptestDiagnosticsImp = std::make_unique<DiagnosticsImp>(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(ptestDiagnosticsImp));
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_REBOOT_FOR_REPAIR);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenGPUProcessCleanupSucceedsThenCallSucceeds) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagProcfsAccess->ourDevicePid = getpid();
    pMockDiagLinuxSysmanImp->ourDevicePid = getpid();
    pMockDiagLinuxSysmanImp->ourDeviceFd = ::open("/dev/null", 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->gpuProcessCleanup(true));
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenGPUProcessCleanupFailsThenWaitForQuiescentCompletionsFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockSysfsAccess->setMockError(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
    pMockDiagProcfsAccess->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pPublicLinuxDiagnosticsImp->waitForQuiescentCompletion());
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenQuiescentFailsContinuouslyFailsThenWaitForQuiescentCompletionsFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockSysfsAccess->setErrorAfterCount(12, ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, pPublicLinuxDiagnosticsImp->waitForQuiescentCompletion());
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenInvalidateLmemFailsThenWaitForQuiescentCompletionsFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    VariableBackup<ProcfsAccess *> backup(&pMockDiagLinuxSysmanImp->pProcfsAccess);
    pMockDiagLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockSysfsAccess->setErrorAfterCount(1, ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pPublicLinuxDiagnosticsImp->waitForQuiescentCompletion());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetThenCallSucceeds) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerAndVfBarIsResizedWhenCallingWarmResetAndGtPciConfigOpenFailsThenCallReturnsFailure) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, gtPciConfigOpenFail);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerAndVfBarIsResizedWhenCallingWarmResetAndConfigHeaderIsInvalidThenCallReturnsFailure) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = mockGtConfigPreadInvalid;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerAndVfBarIsResizedWhenCallingWarmResetAndGtConfigPreadFailsThenCallReturnsFailure) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = mockGtConfigPreadFail;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerAndVfBarIsResizedWhenCallingWarmResetAndGtConfigPwriteFailsThenCallReturnsFailure) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = mockGtConfigPwriteFail;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerAndVfBarIsResizedWhenCallingWarmResetAndCardBusRemoveFailsThenCallReturnsFailure) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;

    pMockFsAccess->checkErrorAfterCount = 2;
    pMockFsAccess->mockWriteError = ZE_RESULT_ERROR_NOT_AVAILABLE;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerAndVfBarIsResizedWhenCallingWarmResetAndRootPortRescanFailsThenCallReturnsFailure) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;

    pMockFsAccess->checkErrorAfterCount = 3;
    pMockFsAccess->mockWriteError = ZE_RESULT_ERROR_NOT_AVAILABLE;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerAndVfBarIsResizedWhenCallingWarmResetThenCallSucceeds) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetfromDiagnosticsThenCallSucceeds) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;
    pLinuxSysmanImp->diagnosticsReset = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetfromHBMDiagnosticsThenCallSucceeds) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;
    pLinuxSysmanImp->diagnosticsReset = true;
    pLinuxSysmanImp->isMemoryDiagnostics = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerAndDelayForPPRWhenCallingWarmResetfromHBMDiagnosticsThenCallSucceeds) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DebugSetMemoryDiagnosticsDelay.set(7);
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;
    pLinuxSysmanImp->diagnosticsReset = true;
    pLinuxSysmanImp->isMemoryDiagnostics = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetAndRootPortConfigFileFailsToOpenThenCallFails) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiagFail);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetAndCardbusRemoveFailsThenCallFails) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;

    pMockFsAccess->mockWriteError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetAndRootPortRescanFailsThenCallFails) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.VfBarResourceAllocationWa.set(false);
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, openMockDiag);
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;

    pMockFsAccess->checkErrorAfterCount = 1;
    pMockFsAccess->mockWriteError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingColdResetThenCallSucceeds) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->osColdReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingColdResetAndListDirFailsThenCallFails) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    pMockFsAccess->mockListDirError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxSysmanImp->osColdReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingColdResetAndReadSlotAddressFailsThenCallFails) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    pMockFsAccess->mockReadError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxSysmanImp->osColdReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingColdResetandWriteFailsThenCallFails) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    pMockFsAccess->mockWriteError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxSysmanImp->osColdReset());
    pMockFsAccess->checkErrorAfterCount = 1;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxSysmanImp->osColdReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingColdResetandWrongSlotAddressIsReturnedThenCallFails) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    pMockFsAccess->setWrongMockAddress();
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, pLinuxSysmanImp->osColdReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingReleaseResourcesAndInitDeviceThenCallSucceeds) {
    pLinuxSysmanImp->diagnosticsReset = true;
    pLinuxSysmanImp->releaseDeviceResources();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->initDevice());
}

TEST_F(ZesDiagnosticsFixture, GivenSysmanImpPointerWhenCallingReleaseResourcesThenGfxPartitionIsRemovedForRootDevice) {
    pLinuxSysmanImp->diagnosticsReset = true;
    auto devicePtr = static_cast<DeviceImp *>(pLinuxSysmanImp->pDevice);
    auto executionEnvironment = devicePtr->getNEODevice()->getExecutionEnvironment();
    auto rootIndex = devicePtr->getNEODevice()->getRootDeviceIndex();

    pLinuxSysmanImp->releaseDeviceResources();

    EXPECT_EQ(nullptr, executionEnvironment->memoryManager->getGfxPartition(rootIndex));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->initDevice());
    EXPECT_NE(nullptr, executionEnvironment->memoryManager->getGfxPartition(rootIndex));
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleAndHandleCountZeroWhenCallingReInitThenZeroCountIsReturnedAndVerifyZesDeviceEnumDiagnosticTestSuitesSucceeds) {

    constexpr uint32_t mockDiagHandleCount = 0;
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockDiagHandleCount);

    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.clear();
    pSysmanDeviceImp->pDiagnosticsHandleContext->supportedDiagTests.clear();

    pLinuxSysmanImp->diagnosticsReset = false;
    pLinuxSysmanImp->reInitSysmanDeviceResources();

    count = 0;
    result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockDiagHandleCount);
}

}; // namespace ult
}; // namespace L0

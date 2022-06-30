/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_hw_config.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/diagnostics/linux/mock_zes_sysman_diagnostics.h"

extern bool sysmanUltsEnable;

using ::testing::_;
using ::testing::Matcher;
using ::testing::Return;

namespace L0 {
namespace ult {

static int mockFileDescriptor = 123;

inline static int openMockDiag(const char *pathname, int flags) {
    if (strcmp(pathname, mockRealPathConfig.c_str()) == 0) {
        return mockFileDescriptor;
    }
    return -1;
}
void mockSleepFunctionSecs(int64_t secs) {
    return;
}

inline static int openMockDiagFail(const char *pathname, int flags) {
    return -1;
}
inline static int closeMockDiag(int fd) {
    if (fd == mockFileDescriptor) {
        return 0;
    }
    return -1;
}
inline static int closeMockDiagFail(int fd) {
    return -1;
}

ssize_t preadMockDiag(int fd, void *buf, size_t count, off_t offset) {
    return count;
}

ssize_t pwriteMockDiag(int fd, const void *buf, size_t count, off_t offset) {
    return count;
}
class ZesDiagnosticsFixture : public SysmanDeviceFixture {

  protected:
    zes_diag_handle_t hSysmanDiagnostics = {};
    std::unique_ptr<Mock<DiagnosticsFwInterface>> pMockDiagFwInterface;
    std::unique_ptr<Mock<DiagSysfsAccess>> pMockSysfsAccess;
    std::unique_ptr<Mock<DiagFsAccess>> pMockFsAccess;
    std::unique_ptr<Mock<DiagProcfsAccess>> pMockDiagProcfsAccess;
    std::unique_ptr<MockGlobalOperationsEngineHandleContext> pEngineHandleContext;
    std::unique_ptr<MockDiagLinuxSysmanImp> pMockDiagLinuxSysmanImp;

    FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    SysfsAccess *pSysfsAccessOld = nullptr;
    FsAccess *pFsAccessOld = nullptr;
    ProcfsAccess *pProcfsAccessOld = nullptr;
    EngineHandleContext *pEngineHandleContextOld = nullptr;
    LinuxSysmanImp *pLinuxSysmanImpOld = nullptr;
    PRODUCT_FAMILY productFamily;

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
        pLinuxSysmanImpOld = pLinuxSysmanImp;

        pEngineHandleContext = std::make_unique<MockGlobalOperationsEngineHandleContext>(pOsSysman);
        pMockDiagFwInterface = std::make_unique<NiceMock<Mock<DiagnosticsFwInterface>>>();
        pMockSysfsAccess = std::make_unique<NiceMock<Mock<DiagSysfsAccess>>>();
        pMockFsAccess = std::make_unique<NiceMock<Mock<DiagFsAccess>>>();
        pMockDiagProcfsAccess = std::make_unique<NiceMock<Mock<DiagProcfsAccess>>>();
        pMockDiagLinuxSysmanImp = std::make_unique<MockDiagLinuxSysmanImp>(pLinuxSysmanImp->getSysmanDeviceImp());
        pSysmanDeviceImp->pEngineHandleContext = pEngineHandleContext.get();
        pLinuxSysmanImp->pProcfsAccess = pMockDiagProcfsAccess.get();
        pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();
        pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
        pLinuxSysmanImp->pFwUtilInterface = pMockDiagFwInterface.get();

        for (const auto &handle : pSysmanDeviceImp->pDiagnosticsHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.clear();
        productFamily = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getHardwareInfo().platform.eProductFamily;
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
        for (const auto &handle : pSysmanDeviceImp->pDiagnosticsHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.clear();
        pSysmanDeviceImp->pDiagnosticsHandleContext->supportedDiagTests.clear();
    }
};

TEST_F(ZesDiagnosticsFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumDiagnosticTestSuitesThenZeroCountIsReturnedAndVerifyzesDeviceEnumDiagnosticTestSuitesCallSucceeds) {
    if (productFamily != IGFX_PVC) {
        mockDiagHandleCount = 0;
    }
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
    if (productFamily == IGFX_PVC) {
        DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
        pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
        result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &count, nullptr);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(count, mockDiagHandleCount);

        testCount = count;

        diagnosticsHandle.resize(testCount);
        result = zesDeviceEnumDiagnosticTestSuites(device->toHandle(), &testCount, diagnosticsHandle.data());

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, diagnosticsHandle.data());
        EXPECT_EQ(testCount, mockDiagHandleCount);

        pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
        delete ptestDiagnosticsImp;
    }
}

TEST_F(ZesDiagnosticsFixture, GivenFailedFirmwareInitializationWhenInitializingDiagnosticsContextThenexpectNoHandles) {
    pMockDiagFwInterface->mockFwInitResult = ZE_RESULT_ERROR_UNINITIALIZED;
    clearAndReinitHandles();
    pSysmanDeviceImp->pDiagnosticsHandleContext->init();

    EXPECT_EQ(0u, pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.size());
    pMockDiagFwInterface->setFwInitRetVal(ZE_RESULT_SUCCESS);
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenGettingDiagnosticsPropertiesThenCallSucceeds) {

    clearAndReinitHandles();
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDiagnosticsGetProperties(handle, &properties));
    EXPECT_EQ(properties.haveTests, 0);
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.name, mockSupportedDiagTypes[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    delete ptestDiagnosticsImp;
}

TEST_F(SysmanMultiDeviceFixture, GivenValidDevicePointerWhenGettingDiagnosticsPropertiesThenValidDiagPropertiesRetrieved) {
    zes_diag_properties_t properties = {};
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(device)->getProperties(&deviceProperties);
    LinuxDiagnosticsImp *pLinuxDiagnosticsImp = new LinuxDiagnosticsImp(pOsSysman, mockSupportedDiagTypes[0]);
    pLinuxDiagnosticsImp->osGetDiagProperties(&properties);
    EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
    EXPECT_EQ(properties.onSubdevice, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE);

    delete pLinuxDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenGettingDiagnosticsTestThenCallSucceeds) {
    clearAndReinitHandles();
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_test_t tests = {};
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDiagnosticsGetTests(handle, &count, &tests));
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenRunningDiagnosticsTestThenCallSucceeds) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_NO_ERRORS);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    ptestDiagnosticsImp->pOsDiagnostics = nullptr;
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenRunningDiagnosticsTestAndFwRunDiagTestFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagFwInterface->setDiagResult(ZES_DIAG_RESULT_FORCE_UINT32);
    pMockDiagFwInterface->mockFwRunDiagTestsResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_FORCE_UINT32);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    ptestDiagnosticsImp->pOsDiagnostics = nullptr;
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenListProcessFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagProcfsAccess->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_FORCE_UINT32);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    ptestDiagnosticsImp->pOsDiagnostics = nullptr;
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenQuiescentingFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockSysfsAccess->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_FORCE_UINT32);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    ptestDiagnosticsImp->pOsDiagnostics = nullptr;
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenInvalidateLmemFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockSysfsAccess->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_FORCE_UINT32);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    ptestDiagnosticsImp->pOsDiagnostics = nullptr;
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenColdResetFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagFwInterface->setDiagResult(ZES_DIAG_RESULT_REBOOT_FOR_REPAIR);
    pMockDiagLinuxSysmanImp->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_REBOOT_FOR_REPAIR);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    ptestDiagnosticsImp->pOsDiagnostics = nullptr;
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenWarmResetFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagLinuxSysmanImp->setMockError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_NO_ERRORS);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    ptestDiagnosticsImp->pOsDiagnostics = nullptr;
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenWarmResetSucceedsAndInitDeviceFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagLinuxSysmanImp->setMockInitDeviceError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_NO_ERRORS);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    ptestDiagnosticsImp->pOsDiagnostics = nullptr;
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenColdResetSucceedsAndInitDeviceFailsThenCallFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagFwInterface->setDiagResult(ZES_DIAG_RESULT_REBOOT_FOR_REPAIR);
    pMockDiagLinuxSysmanImp->setMockInitDeviceError(ZE_RESULT_ERROR_NOT_AVAILABLE);
    DiagnosticsImp *ptestDiagnosticsImp = new DiagnosticsImp(pSysmanDeviceImp->pDiagnosticsHandleContext->pOsSysman, mockSupportedDiagTypes[0]);
    std::unique_ptr<OsDiagnostics> pOsDiagnosticsPrev = std::move(ptestDiagnosticsImp->pOsDiagnostics);
    ptestDiagnosticsImp->pOsDiagnostics = std::move(pPublicLinuxDiagnosticsImp);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(ptestDiagnosticsImp);
    auto handle = pSysmanDeviceImp->pDiagnosticsHandleContext->handleList[0]->toHandle();
    zes_diag_result_t results = ZES_DIAG_RESULT_FORCE_UINT32;
    uint32_t start = 0, end = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDiagnosticsRunTests(handle, start, end, &results));
    EXPECT_EQ(results, ZES_DIAG_RESULT_REBOOT_FOR_REPAIR);
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.pop_back();
    ptestDiagnosticsImp->pOsDiagnostics = nullptr;
    delete ptestDiagnosticsImp;
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenGPUProcessCleanupSucceedsThenCallSucceeds) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockDiagProcfsAccess->ourDevicePid = getpid();
    pMockDiagLinuxSysmanImp->ourDevicePid = getpid();
    pMockDiagLinuxSysmanImp->ourDeviceFd = ::open("/dev/null", 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPublicLinuxDiagnosticsImp->gpuProcessCleanup());
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenGPUProcessCleanupFailsThenWaitForQuiescentCompletionsFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

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
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pPublicLinuxDiagnosticsImp->pSleepFunctionSecs = mockSleepFunctionSecs;

    pMockSysfsAccess->setErrorAfterCount(12, ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, pPublicLinuxDiagnosticsImp->waitForQuiescentCompletion());
}

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandleWhenInvalidateLmemFailsThenWaitForQuiescentCompletionsFails) {

    clearAndReinitHandles();
    std::unique_ptr<PublicLinuxDiagnosticsImp> pPublicLinuxDiagnosticsImp = std::make_unique<PublicLinuxDiagnosticsImp>();

    pPublicLinuxDiagnosticsImp->pSysfsAccess = pMockSysfsAccess.get();
    pPublicLinuxDiagnosticsImp->pFwInterface = pMockDiagFwInterface.get();
    pPublicLinuxDiagnosticsImp->pProcfsAccess = pMockDiagProcfsAccess.get();
    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp = pMockDiagLinuxSysmanImp.get();

    pPublicLinuxDiagnosticsImp->pLinuxSysmanImp->pDevice = pLinuxSysmanImp->getDeviceHandle();

    pMockSysfsAccess->setErrorAfterCount(1, ZE_RESULT_ERROR_NOT_AVAILABLE);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pPublicLinuxDiagnosticsImp->waitForQuiescentCompletion());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetThenCallSucceeds) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    pLinuxSysmanImp->openFunction = openMockDiag;
    pLinuxSysmanImp->closeFunction = closeMockDiag;
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;
    pLinuxSysmanImp->pSleepFunctionSecs = mockSleepFunctionSecs;

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetAndRootPortConfigFileFailsToOpenThenCallFails) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    pLinuxSysmanImp->openFunction = openMockDiagFail;
    pLinuxSysmanImp->closeFunction = closeMockDiag;
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;
    pLinuxSysmanImp->pSleepFunctionSecs = mockSleepFunctionSecs;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetAndRootPortConfigFileFailsToCloseThenCallFails) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    pLinuxSysmanImp->openFunction = openMockDiag;
    pLinuxSysmanImp->closeFunction = closeMockDiagFail;
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;
    pLinuxSysmanImp->pSleepFunctionSecs = mockSleepFunctionSecs;

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetAndCardbusRemoveFailsThenCallFails) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    pLinuxSysmanImp->openFunction = openMockDiag;
    pLinuxSysmanImp->closeFunction = closeMockDiag;
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;
    pLinuxSysmanImp->pSleepFunctionSecs = mockSleepFunctionSecs;

    pMockFsAccess->mockWriteError = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxSysmanImp->osWarmReset());
}

TEST_F(ZesDiagnosticsFixture, GivenValidSysmanImpPointerWhenCallingWarmResetAndRootPortRescanFailsThenCallFails) {
    pLinuxSysmanImp->gtDevicePath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0";
    pLinuxSysmanImp->openFunction = openMockDiag;
    pLinuxSysmanImp->closeFunction = closeMockDiag;
    pLinuxSysmanImp->preadFunction = preadMockDiag;
    pLinuxSysmanImp->pwriteFunction = pwriteMockDiag;
    pLinuxSysmanImp->pSleepFunctionSecs = mockSleepFunctionSecs;

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

}; // namespace ult
}; // namespace L0

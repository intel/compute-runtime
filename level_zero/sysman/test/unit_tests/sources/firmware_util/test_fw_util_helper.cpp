/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/firmware_util/mock_fw_util_fixture.h"

#include <algorithm>

namespace L0 {

extern L0::Sysman::pIgscIfrGetStatusExt L0::Sysman::deviceIfrGetStatusExt;
extern L0::Sysman::pIgscIfrRunMemPPRTest L0::Sysman::deviceIfrRunMemPPRTest;
extern L0::Sysman::pIgscGfspHeciCmd gfspHeciCmd;
extern L0::Sysman::pIgscDeviceUpdateLateBindingConfig deviceUpdateLateBindingConfig;

namespace Sysman {
namespace ult {

constexpr static uint32_t mockMaxTileCount = 2;
static int mockMemoryHealthIndicator = IGSC_HEALTH_INDICATOR_HEALTHY;
static int mockGetFwVersion = IGSC_SUCCESS;
static int mockGetOpromCodeVersion = IGSC_SUCCESS;
static int mockGetOpromDataVersion = IGSC_SUCCESS;
static int mockGetPscVersion = IGSC_SUCCESS;

std::vector<uint32_t>
    mockSupportedHeciCmds = {GfspHeciConstants::Cmd::getEccConfigurationCmd16, GfspHeciConstants::Cmd::setEccConfigurationCmd15};
uint8_t mockEccCurrentState = 1;
uint8_t mockEccPendingState = 1;
uint8_t mockEccDefaultState = 1;
uint8_t mockEccHeciCmd16Val = 1;

void restoreEccMockVars() {
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.insert(mockSupportedHeciCmds.end(), {GfspHeciConstants::Cmd::getEccConfigurationCmd16, GfspHeciConstants::Cmd::setEccConfigurationCmd15});
    mockEccCurrentState = 1;
    mockEccPendingState = 1;
    mockEccDefaultState = 1;
    mockEccHeciCmd16Val = 1;
}

int mockDeviceIfrGetStatusExt(struct igsc_device_handle *handle, uint32_t *supportedTests, uint32_t *hwCapabilities, uint32_t *ifrApplied, uint32_t *prevErrors, uint32_t *pendingReset) {
    return 0;
}

int mockdeviceIfrRunMemPPRTest(struct igsc_device_handle *handle, uint32_t *status, uint32_t *pendingReset, uint32_t *errorCode) {
    return 0;
}

static inline int mockEccConfigGetSuccess(struct igsc_device_handle *handle, uint32_t gfspCmd, uint8_t *inBuffer, size_t inBufferSize, uint8_t *outBuffer, size_t outBufferSize, size_t *actualOutBufferSize) {
    return 0;
}

static inline int mockEccConfigGetFailure(struct igsc_device_handle *handle, uint32_t gfspCmd, uint8_t *inBuffer, size_t inBufferSize, uint8_t *outBuffer, size_t outBufferSize, size_t *actualOutBufferSize) {
    return -1;
}

static inline int mockCountTiles(struct igsc_device_handle *handle, uint32_t *numOfTiles) {
    *numOfTiles = mockMaxTileCount;
    return 0;
}

static inline int mockMemoryErrors(struct igsc_device_handle *handle, struct igsc_gfsp_mem_err *tiles) {
    return 0;
}

static inline int mockGetHealthIndicator(struct igsc_device_handle *handle, uint8_t *healthIndicator) {
    *healthIndicator = mockMemoryHealthIndicator;
    return 0;
}

static inline int mockGetHealthIndicatorFailure(struct igsc_device_handle *handle, uint8_t *healthIndicator) {
    return -1;
}

static int mockGetEccAvailable(struct igsc_device_handle *handle, uint32_t gfspCmd, uint8_t *inBuffer, size_t inBufferSize, uint8_t *outBuffer, size_t outBufferSize, size_t *actualOutBufferSize) {

    if (std::find(mockSupportedHeciCmds.begin(), mockSupportedHeciCmds.end(), gfspCmd) == mockSupportedHeciCmds.end()) {
        return IGSC_ERROR_NOT_SUPPORTED;
    }

    switch (gfspCmd) {
    case GfspHeciConstants::Cmd::getEccConfigurationCmd16:
        outBuffer[GfspHeciConstants::GetEccCmd16BytePostition::eccAvailable] = mockEccHeciCmd16Val;
        return IGSC_SUCCESS;
    case GfspHeciConstants::Cmd::getEccConfigurationCmd9:
        outBuffer[GfspHeciConstants::GetEccCmd9BytePostition::currentState] = mockEccCurrentState;
        outBuffer[GfspHeciConstants::GetEccCmd9BytePostition::pendingState] = mockEccPendingState;
        return IGSC_SUCCESS;
    default:
        return IGSC_ERROR_NOT_SUPPORTED;
    }
}

static int mockGetEccConfigurable(struct igsc_device_handle *handle, uint32_t gfspCmd, uint8_t *inBuffer, size_t inBufferSize, uint8_t *outBuffer, size_t outBufferSize, size_t *actualOutBufferSize) {

    if (std::find(mockSupportedHeciCmds.begin(), mockSupportedHeciCmds.end(), gfspCmd) == mockSupportedHeciCmds.end()) {
        return IGSC_ERROR_NOT_SUPPORTED;
    }

    switch (gfspCmd) {
    case GfspHeciConstants::Cmd::getEccConfigurationCmd16:
        outBuffer[GfspHeciConstants::GetEccCmd16BytePostition::eccConfigurable] = mockEccHeciCmd16Val;
        return IGSC_SUCCESS;
    case GfspHeciConstants::Cmd::getEccConfigurationCmd9:
        outBuffer[GfspHeciConstants::GetEccCmd9BytePostition::currentState] = mockEccCurrentState;
        outBuffer[GfspHeciConstants::GetEccCmd9BytePostition::pendingState] = mockEccPendingState;
        return IGSC_SUCCESS;
    default:
        return IGSC_ERROR_NOT_SUPPORTED;
    }
}

static int mockGetEccConfig(struct igsc_device_handle *handle, uint32_t gfspCmd, uint8_t *inBuffer, size_t inBufferSize, uint8_t *outBuffer, size_t outBufferSize, size_t *actualOutBufferSize) {

    if (std::find(mockSupportedHeciCmds.begin(), mockSupportedHeciCmds.end(), gfspCmd) == mockSupportedHeciCmds.end()) {
        return IGSC_ERROR_NOT_SUPPORTED;
    }

    switch (gfspCmd) {
    case GfspHeciConstants::Cmd::getEccConfigurationCmd16:
        outBuffer[GfspHeciConstants::GetEccCmd16BytePostition::eccCurrentState] = mockEccCurrentState;
        outBuffer[GfspHeciConstants::GetEccCmd16BytePostition::eccPendingState] = mockEccPendingState;
        outBuffer[GfspHeciConstants::GetEccCmd16BytePostition::eccDefaultState] = mockEccDefaultState;
        return IGSC_SUCCESS;
    case GfspHeciConstants::Cmd::getEccConfigurationCmd9:
        outBuffer[GfspHeciConstants::GetEccCmd9BytePostition::currentState] = mockEccCurrentState;
        outBuffer[GfspHeciConstants::GetEccCmd9BytePostition::pendingState] = mockEccPendingState;
        outBuffer[GfspHeciConstants::GetEccCmd16BytePostition::eccDefaultState] = 0xff;
        return IGSC_SUCCESS;
    case GfspHeciConstants::Cmd::setEccConfigurationCmd15:
        outBuffer[GfspHeciConstants::SetEccCmd15BytePostition::response] = inBuffer[GfspHeciConstants::SetEccCmd15BytePostition::request];
        return IGSC_SUCCESS;
    case GfspHeciConstants::Cmd::setEccConfigurationCmd8:
        outBuffer[GfspHeciConstants::SetEccCmd8BytePostition::responsePendingState] = inBuffer[GfspHeciConstants::SetEccCmd8BytePostition::setRequest];
        outBuffer[GfspHeciConstants::SetEccCmd8BytePostition::responseCurrentState] = mockEccCurrentState;
        return IGSC_SUCCESS;
    default:
        return IGSC_ERROR_NOT_SUPPORTED;
    }
}

static int mockIgscDeviceFwVersion(struct igsc_device_handle *handle,
                                   struct igsc_fw_version *version) {
    return mockGetFwVersion;
}

static int mockIgscDeviceOpromVersion(struct igsc_device_handle *handle,
                                      uint32_t opromType,
                                      struct igsc_oprom_version *version) {
    if (opromType == IGSC_OPROM_CODE) {
        return mockGetOpromCodeVersion;
    } else if (opromType == IGSC_OPROM_DATA) {
        return mockGetOpromDataVersion;
    } else {
        return IGSC_ERROR_NOT_SUPPORTED;
    }
}

static int mockIgscDevicePscVersion(struct igsc_device_handle *handle,
                                    struct igsc_psc_version *version) {
    return mockGetPscVersion;
}

TEST(FwStatusExtTest, GivenIFRWasSetWhenFirmwareUtilChecksIFRThenIFRStatusIsUpdated) {

    VariableBackup<decltype(L0::Sysman::deviceIfrGetStatusExt)> mockDeviceIfrGetStatusExt(&L0::Sysman::deviceIfrGetStatusExt, [](struct igsc_device_handle *handle, uint32_t *supportedTests, uint32_t *hwCapabilities, uint32_t *ifrApplied, uint32_t *prevErrors, uint32_t *pendingReset) -> int {
        *ifrApplied = (IGSC_IFR_REPAIRS_MASK_DSS_EN_REPAIR | IGSC_IFR_REPAIRS_MASK_ARRAY_REPAIR);
        return 0;
    });

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    bool mockStatus = false;
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());
    auto ret = pFwUtilImp->fwIfrApplied(mockStatus);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_TRUE(mockStatus);
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwStatusExtTest, GivenStatusCallFailsWhenFirmwareUtilChecksIFRThenStatusCallFails) {

    VariableBackup<decltype(L0::Sysman::deviceIfrGetStatusExt)> mockDeviceIfrGetStatusExt(&L0::Sysman::deviceIfrGetStatusExt, [](struct igsc_device_handle *handle, uint32_t *supportedTests, uint32_t *hwCapabilities, uint32_t *ifrApplied, uint32_t *prevErrors, uint32_t *pendingReset) -> int {
        return -1;
    });

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    bool mockStatus = false;
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());
    auto ret = pFwUtilImp->fwIfrApplied(mockStatus);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, ret);
    EXPECT_FALSE(mockStatus);
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwRunDiagTest, GivenValidSupportedDiagnosticsTestsParamWhenFirmwareUtilSupportedTestsAreRequestedThenSupportedTestsAreRun) {

    VariableBackup<decltype(L0::Sysman::deviceIfrGetStatusExt)> mockDeviceIfrGetStatusExt(&L0::Sysman::deviceIfrGetStatusExt, [](struct igsc_device_handle *handle, uint32_t *supportedTests, uint32_t *hwCapabilities, uint32_t *ifrApplied, uint32_t *prevErrors, uint32_t *pendingReset) -> int {
        *supportedTests = (IGSC_IFR_SUPPORTED_TESTS_MEMORY_PPR);
        return 0;
    });

    VariableBackup<decltype(L0::Sysman::deviceIfrRunMemPPRTest)> mockdeviceIfrRunMemPPRTest(&L0::Sysman::deviceIfrRunMemPPRTest, [](struct igsc_device_handle *handle, uint32_t *status, uint32_t *pendingReset, uint32_t *errorCode) -> int {
        *status = 0;
        return 0;
    });

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    std::vector<std::string> supportedDiagTests;
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());
    auto ret = pFwUtilImp->fwSupportedDiagTests(supportedDiagTests);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_STREQ("MEMORY_PPR", supportedDiagTests.at(0).c_str());

    zes_diag_result_t pDiagResult;
    ret = pFwUtilImp->fwRunDiagTests(supportedDiagTests[0], &pDiagResult);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(ZES_DIAG_RESULT_NO_ERRORS, pDiagResult);
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetProcAddressTest, GivenValidFwUtilMethodNameWhenFirmwareUtilIsInitalizedThenCorrectMethodsAreLoaded) {
    struct IFRmockOsLibrary : OsLibrary {
      public:
        ~IFRmockOsLibrary() override = default;
        void *getProcAddress(const std::string &procName) override {
            ifrFuncMap["igsc_ifr_get_status_ext"] = reinterpret_cast<void *>(&mockDeviceIfrGetStatusExt);
            ifrFuncMap["igsc_ifr_run_mem_ppr_test"] = reinterpret_cast<void *>(&mockdeviceIfrRunMemPPRTest);
            auto it = ifrFuncMap.find(procName);
            if (ifrFuncMap.end() == it) {
                return nullptr;
            } else {
                return it->second;
            }
            return nullptr;
        }
        bool isLoaded() override { return true; }
        std::string getFullPath() override {
            return std::string();
        }
        std::map<std::string, void *> ifrFuncMap;
    };
    uint16_t domain = 0;
    uint8_t bus = 0;
    uint8_t device = 0;
    uint8_t function = 0;
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(domain, bus, device, function);
    std::vector<std::string> supportedDiagTests;
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new IFRmockOsLibrary());
    EXPECT_TRUE(pFwUtilImp->loadEntryPointsExt());
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwEccTest, GivenFwEccConfigCallFailsWhenCallingFirmwareUtilSetAndGetEccThenCorrespondingCallFails) {

    struct IgscEccMockOsLibrary : public OsLibrary {
      public:
        ~IgscEccMockOsLibrary() override = default;
        void *getProcAddress(const std::string &procName) override {
            eccFuncMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockEccConfigGetFailure);
            auto it = eccFuncMap.find(procName);
            if (eccFuncMap.end() == it) {
                return nullptr;
            } else {
                return it->second;
            }
            return nullptr;
        }
        bool isLoaded() override {
            return false;
        }
        std::string getFullPath() override {
            return std::string();
        }
        std::map<std::string, void *> eccFuncMap;
    };
    uint16_t domain = 0;
    uint8_t bus = 0;
    uint8_t device = 0;
    uint8_t function = 0;
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(domain, bus, device, function);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new IgscEccMockOsLibrary());
    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    uint8_t defaultState = 0;
    uint8_t newState = 0;
    auto ret = pFwUtilImp->fwGetEccConfig(&currentState, &pendingState, &defaultState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);
    ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenValidFwUtilMethodWhenCallingFirmwareUtilSetAndGetEccThenCorrespondingCallSucceeds) {

    struct IgscEccMockOsLibrary : public OsLibrary {
      public:
        ~IgscEccMockOsLibrary() override = default;
        void *getProcAddress(const std::string &procName) override {
            eccFuncMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockEccConfigGetSuccess);
            auto it = eccFuncMap.find(procName);
            if (eccFuncMap.end() == it) {
                return nullptr;
            } else {
                return it->second;
            }
            return nullptr;
        }
        bool isLoaded() override {
            return false;
        }
        std::string getFullPath() override {
            return std::string();
        }
        std::map<std::string, void *> eccFuncMap;
    };
    uint16_t domain = 0;
    uint8_t bus = 0;
    uint8_t device = 0;
    uint8_t function = 0;
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(domain, bus, device, function);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new IgscEccMockOsLibrary());
    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    uint8_t defaultState = 0;
    uint8_t newState = 0;
    auto ret = pFwUtilImp->fwGetEccConfig(&currentState, &pendingState, &defaultState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenGetProcAddrCallFailsWhenFirmwareUtilChecksEccGetAndSetConfigThenCorrespondingCallFails) {

    uint16_t domain = 0;
    uint8_t bus = 0;
    uint8_t device = 0;
    uint8_t function = 0;
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(domain, bus, device, function);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());
    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    uint8_t defaultState = 0;
    uint8_t newState = 0;
    auto ret = pFwUtilImp->fwGetEccConfig(&currentState, &pendingState, &defaultState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);
    ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetMemErrorCountTest, GivenGetProcAddrCallFailsWhenMemoryErrorCountIsRequestedThenFailureIsReturned) {

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());
    zes_ras_error_type_t errorType = ZES_RAS_ERROR_TYPE_CORRECTABLE;
    uint32_t subDeviceCount = 1;
    uint32_t subDeviceId = 0;
    uint64_t errorCount = 0;
    auto ret = pFwUtilImp->fwGetMemoryErrorCount(errorType, subDeviceCount, subDeviceId, errorCount);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetMemErrorCountTest, GivenValidFwUtilMethodWhenMemoryErrorCountIsRequestedThenCorrespondingCallSucceeds) {

    struct IgscMemErrMockOsLibrary : public OsLibrary {
      public:
        ~IgscMemErrMockOsLibrary() override = default;
        void *getProcAddress(const std::string &procName) override {
            memErrFuncMap["igsc_gfsp_count_tiles"] = reinterpret_cast<void *>(&mockCountTiles);
            memErrFuncMap["igsc_gfsp_memory_errors"] = reinterpret_cast<void *>(&mockMemoryErrors);
            auto it = memErrFuncMap.find(procName);
            if (memErrFuncMap.end() == it) {
                return nullptr;
            } else {
                return it->second;
            }
            return nullptr;
        }
        bool isLoaded() override {
            return false;
        }
        std::string getFullPath() override {
            return std::string();
        }
        std::map<std::string, void *> memErrFuncMap;
    };
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new IgscMemErrMockOsLibrary());
    zes_ras_error_type_t errorType = ZES_RAS_ERROR_TYPE_CORRECTABLE;
    uint32_t subDeviceCount = 1;
    uint32_t subDeviceId = 0;
    uint64_t errorCount = 0;
    auto ret = pFwUtilImp->fwGetMemoryErrorCount(errorType, subDeviceCount, subDeviceId, errorCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetMemHealthIndicatorTest, GivenValidFwUtilMethodWhenGetMemoryHealthIndicatorReturnsDegradedThenVerifyMemoryIsDegraded) {

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_get_health_indicator"] = reinterpret_cast<void *>(&mockGetHealthIndicator);
    mockMemoryHealthIndicator = IGSC_HEALTH_INDICATOR_DEGRADED;

    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    zes_mem_health_t health = ZES_MEM_HEALTH_UNKNOWN;
    pFwUtilImp->fwGetMemoryHealthIndicator(&health);
    EXPECT_EQ(health, ZES_MEM_HEALTH_DEGRADED);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetMemHealthIndicatorTest, GivenValidFwUtilMethodWhenGetMemoryHealthIndicatorReturnsHealthyThenVerifyMemoryIsHealthy) {

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_get_health_indicator"] = reinterpret_cast<void *>(&mockGetHealthIndicator);
    mockMemoryHealthIndicator = IGSC_HEALTH_INDICATOR_HEALTHY;

    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    zes_mem_health_t health = ZES_MEM_HEALTH_UNKNOWN;
    pFwUtilImp->fwGetMemoryHealthIndicator(&health);
    EXPECT_EQ(health, ZES_MEM_HEALTH_OK);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetMemHealthIndicatorTest, GivenValidFwUtilMethodWhenGetMemoryHealthIndicatorReturnsCriticalThenVerifyMemoryHealthIsCritical) {

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_get_health_indicator"] = reinterpret_cast<void *>(&mockGetHealthIndicator);
    mockMemoryHealthIndicator = IGSC_HEALTH_INDICATOR_CRITICAL;

    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    zes_mem_health_t health = ZES_MEM_HEALTH_UNKNOWN;
    pFwUtilImp->fwGetMemoryHealthIndicator(&health);
    EXPECT_EQ(health, ZES_MEM_HEALTH_CRITICAL);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetMemHealthIndicatorTest, GivenValidFwUtilMethodWhenGetMemoryHealthIndicatorReturnsReplaceNeededThenVerifyMemoryNeedsReplace) {

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_get_health_indicator"] = reinterpret_cast<void *>(&mockGetHealthIndicator);
    mockMemoryHealthIndicator = IGSC_HEALTH_INDICATOR_REPLACE;

    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    zes_mem_health_t health = ZES_MEM_HEALTH_UNKNOWN;
    pFwUtilImp->fwGetMemoryHealthIndicator(&health);
    EXPECT_EQ(health, ZES_MEM_HEALTH_REPLACE);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetMemHealthIndicatorTest, GivenValidFwUtilMethodWhenGetMemoryHealthIndicatorReturnsUnknownThenVerifyMemoryHealthIsUnknown) {

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_get_health_indicator"] = reinterpret_cast<void *>(&mockGetHealthIndicator);
    mockMemoryHealthIndicator = -1;

    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    zes_mem_health_t health = ZES_MEM_HEALTH_UNKNOWN;
    pFwUtilImp->fwGetMemoryHealthIndicator(&health);
    EXPECT_EQ(health, ZES_MEM_HEALTH_UNKNOWN);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetMemHealthIndicatorTest, GivenFwGetHealthIndicatorFailsWhenMemoryHealthIndicatorIsRequestedThenVerifyMemoryHealthIsUnknown) {

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_get_health_indicator"] = reinterpret_cast<void *>(&mockGetHealthIndicatorFailure);

    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    zes_mem_health_t health = ZES_MEM_HEALTH_UNKNOWN;
    pFwUtilImp->fwGetMemoryHealthIndicator(&health);
    EXPECT_EQ(health, ZES_MEM_HEALTH_UNKNOWN);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetMemHealthIndicatorTest, GivenFwGetHealthIndicatorProcAddrIsNullWhenMemoryHealthIndicatorIsRequestedThenVerifyMemoryHealthIsUnknown) {

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();

    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    zes_mem_health_t health = ZES_MEM_HEALTH_UNKNOWN;
    pFwUtilImp->fwGetMemoryHealthIndicator(&health);
    EXPECT_EQ(health, ZES_MEM_HEALTH_UNKNOWN);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwUtilImpProgressTest, GivenFirmwareUtilImpWhenSettingFirmwareProgressPercentThenCorrectProgressIsReturned) {
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    uint32_t mockProgressPercent = 55;
    pFwUtilImp->updateFirmwareFlashProgress(mockProgressPercent);
    uint32_t percent = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFwUtilImp->getFlashFirmwareProgress(&percent));
    EXPECT_EQ(mockProgressPercent, percent);
    delete pFwUtilImp;
}

TEST(FwUtilImpProgressTest, GivenFirmwareUtilImpWhenSettingProgressThroughCallbackThenCorrectProgressIsReturned) {
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    uint32_t mockDone = 55;
    uint32_t mockTotal = 100;

    firmwareFlashProgressFunc(mockDone, mockTotal, pFwUtilImp);

    uint32_t percent = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFwUtilImp->getFlashFirmwareProgress(&percent));
    uint32_t percentCalculation = (uint32_t)((mockDone * 100) / mockTotal);
    EXPECT_EQ(percentCalculation, percent);
    delete pFwUtilImp;
}

TEST(FwUtilImpProgressTest, GivenFirmwareUtilImpAndNullContextWhenSettingProgressThroughCallbackThenZeroProgressIsReturned) {
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    uint32_t mockDone = 55;
    uint32_t mockTotal = 100;

    firmwareFlashProgressFunc(mockDone, mockTotal, nullptr);

    uint32_t percent = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFwUtilImp->getFlashFirmwareProgress(&percent));
    EXPECT_EQ((uint32_t)0, percent);
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd16IsSupportedThenWhenCallingFwGetEccAvailableSucessIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccAvailable);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::getEccConfigurationCmd16);
    ze_bool_t pAvailable;

    std::vector<uint8_t> possibleEccStates = {0, 1};

    for (auto iterEccAvailableStates : possibleEccStates) {
        mockEccHeciCmd16Val = iterEccAvailableStates;

        auto ret = pFwUtilImp->fwGetEccAvailable(&pAvailable);
        EXPECT_EQ(pAvailable, mockEccHeciCmd16Val);
        EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    }

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd16IsNotSupportedBut9IsSupportedThenWhenCallingFwGetEccAvailableSucessIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccAvailable);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::getEccConfigurationCmd9);

    ze_bool_t pAvailable;
    auto ret = pFwUtilImp->fwGetEccAvailable(&pAvailable);
    EXPECT_TRUE(pAvailable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd16IsNotSupportedBut9IsSupportedAndEccStateIsNoneThenWhenCallingFwGetEccAvailableCorrectReturnValueIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccAvailable);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);

    ze_bool_t pAvailable;
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::getEccConfigurationCmd9);
    mockEccCurrentState = 0xff;
    mockEccPendingState = 0x1;

    auto ret = pFwUtilImp->fwGetEccAvailable(&pAvailable);
    EXPECT_FALSE(pAvailable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    mockEccCurrentState = 0x1;
    mockEccPendingState = 0xff;
    ret = pFwUtilImp->fwGetEccAvailable(&pAvailable);
    EXPECT_FALSE(pAvailable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd9AndCmd16AreUnsupportedWhenCallingFwGetEccAvailableThenUninitializedIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfigurable);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(0xff);

    ze_bool_t pAvailable;
    auto ret = pFwUtilImp->fwGetEccAvailable(&pAvailable);
    EXPECT_FALSE(pAvailable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd16IsSupportedThenWhenCallingFwGetEccConfigurableSucessIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfigurable);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::getEccConfigurationCmd16);
    ze_bool_t pConfigurable;

    std::vector<uint8_t> possibleEccStates = {0, 1};

    for (auto iterEccConfigurableStates : possibleEccStates) {
        mockEccHeciCmd16Val = iterEccConfigurableStates;

        auto ret = pFwUtilImp->fwGetEccConfigurable(&pConfigurable);
        EXPECT_EQ(pConfigurable, mockEccHeciCmd16Val);
        EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    }

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd16IsNotSupportedBut9IsSupportedThenWhenCallingFwGetEccConfigurableSucessIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfigurable);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::getEccConfigurationCmd9);

    ze_bool_t pConfigurable;
    auto ret = pFwUtilImp->fwGetEccConfigurable(&pConfigurable);
    EXPECT_TRUE(pConfigurable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd16IsNotSupportedBut9IsSupportedAndEccStateIsNoneThenWhenCallingFwGetEccConfigurableCorrectReturnValueIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfigurable);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);

    ze_bool_t pConfigurable;
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::getEccConfigurationCmd9);
    mockEccCurrentState = 0xff;
    mockEccPendingState = 0x1;

    auto ret = pFwUtilImp->fwGetEccConfigurable(&pConfigurable);
    EXPECT_FALSE(pConfigurable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    mockEccCurrentState = 0x1;
    mockEccPendingState = 0xff;
    ret = pFwUtilImp->fwGetEccConfigurable(&pConfigurable);
    EXPECT_FALSE(pConfigurable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd9AndCmd16AreUnsupportedWhenCallingFwGetEccConfigurableThenUninitializedIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfigurable);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(0xff);

    ze_bool_t pConfigurable;
    auto ret = pFwUtilImp->fwGetEccConfigurable(&pConfigurable);
    EXPECT_FALSE(pConfigurable);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd16IsSupportedThenWhenCallingFwGetEccConfigSucessIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfig);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::getEccConfigurationCmd16);
    mockEccCurrentState = 1;
    mockEccPendingState = 1;
    mockEccDefaultState = 1;

    uint8_t currentState;
    uint8_t pendingState;
    uint8_t defaultState;
    auto ret = pFwUtilImp->fwGetEccConfig(&currentState, &pendingState, &defaultState);
    EXPECT_EQ(currentState, mockEccCurrentState);
    EXPECT_EQ(pendingState, mockEccPendingState);
    EXPECT_EQ(defaultState, mockEccDefaultState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd16IsNotSupportedBut9IsSupportedThenWhenCallingFwGetEccConfigSucessIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfig);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::getEccConfigurationCmd9);
    uint8_t currentState;
    uint8_t pendingState;
    uint8_t defaultState;

    std::vector<uint8_t> possibleEccStates = {0, 1, 0xff};

    for (auto iterCurrentState : possibleEccStates) {
        for (auto iterPendingState : possibleEccStates) {
            mockEccCurrentState = iterCurrentState;
            mockEccPendingState = iterPendingState;

            auto ret = pFwUtilImp->fwGetEccConfig(&currentState, &pendingState, &defaultState);
            EXPECT_EQ(currentState, mockEccCurrentState);
            EXPECT_EQ(pendingState, mockEccPendingState);
            EXPECT_EQ(defaultState, 0xff);
            EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
        }
    }

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenUnavailableHeciFunctionPointersWhenCallingEccMethodsThenFailureIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);

    ze_bool_t pAvailable;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFwUtilImp->fwGetEccAvailable(&pAvailable));

    ze_bool_t pConfigurable;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFwUtilImp->fwGetEccConfigurable(&pConfigurable));

    uint8_t currentState;
    uint8_t pendingState;
    uint8_t defaultState;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFwUtilImp->fwGetEccConfig(&currentState, &pendingState, &defaultState));

    uint8_t newState = 1;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState));

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd15IsSupportedThenWhenCallingFwSetEccConfigSucessIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfig);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::getEccConfigurationCmd16);
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::setEccConfigurationCmd15);
    mockEccCurrentState = 0;

    uint8_t currentState;
    uint8_t pendingState;
    uint8_t newState = 1;
    auto ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(currentState, mockEccCurrentState);
    EXPECT_EQ(pendingState, newState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd15IsSupportedThenWhenCallingFwSetEccConfigAndFwGetEccConfigFailsThenFailureReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfig);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(0xff);
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::setEccConfigurationCmd15);
    mockEccCurrentState = 0;

    uint8_t currentState;
    uint8_t pendingState;
    uint8_t newState = 1;
    auto ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciSetCommandsAreNotSupportedThenWhenCallingFwSetEccConfigFailureReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfig);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(0xff);
    mockEccCurrentState = 0;

    uint8_t currentState;
    uint8_t pendingState;
    uint8_t newState = 1;
    auto ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenHeciCmd15IsNotSupportedButCmd8IsSupportedThenWhenCallingFwSetEccConfigSucessIsReturned) {
    restoreEccMockVars();
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_gfsp_heci_cmd"] = reinterpret_cast<void *>(&mockGetEccConfig);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    mockSupportedHeciCmds.clear();
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::getEccConfigurationCmd9);
    mockSupportedHeciCmds.push_back(GfspHeciConstants::Cmd::setEccConfigurationCmd8);
    mockEccCurrentState = 0;

    uint8_t currentState;
    uint8_t pendingState;
    uint8_t newState = 1;
    auto ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(currentState, mockEccCurrentState);
    EXPECT_EQ(pendingState, newState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetSupportedFwTypesTest, GivenFirmwareUtilAndAllFwTypesSupportedWhenCallingGetSupportedFwTypesThenExpectValidFwTypes) {
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_device_fw_version"] = reinterpret_cast<void *>(&mockIgscDeviceFwVersion);
    osLibHandle->funcMap["igsc_device_oprom_version"] = reinterpret_cast<void *>(&mockIgscDeviceOpromVersion);
    osLibHandle->funcMap["igsc_device_psc_version"] = reinterpret_cast<void *>(&mockIgscDevicePscVersion);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);

    mockGetFwVersion = IGSC_SUCCESS;
    mockGetOpromCodeVersion = IGSC_SUCCESS;
    mockGetOpromDataVersion = IGSC_SUCCESS;
    mockGetPscVersion = IGSC_SUCCESS;

    std::vector<std::string> fwTypes{};
    pFwUtilImp->getDeviceSupportedFwTypes(fwTypes);
    EXPECT_EQ(fwTypes.size(), 3u);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetSupportedFwTypesTest, GivenFirmwareUtilAndAllFwTypesUnsupportedWhenCallingGetSupportedFwTypesThenExpectZeroFwTypes) {
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_device_fw_version"] = reinterpret_cast<void *>(&mockIgscDeviceFwVersion);
    osLibHandle->funcMap["igsc_device_oprom_version"] = reinterpret_cast<void *>(&mockIgscDeviceOpromVersion);
    osLibHandle->funcMap["igsc_device_psc_version"] = reinterpret_cast<void *>(&mockIgscDevicePscVersion);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);

    mockGetFwVersion = IGSC_ERROR_NOT_SUPPORTED;
    mockGetOpromCodeVersion = IGSC_ERROR_NOT_SUPPORTED;
    mockGetOpromDataVersion = IGSC_ERROR_NOT_SUPPORTED;
    mockGetPscVersion = IGSC_ERROR_NOT_SUPPORTED;

    std::vector<std::string> fwTypes{};
    pFwUtilImp->getDeviceSupportedFwTypes(fwTypes);
    EXPECT_EQ(fwTypes.size(), 0u);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetSupportedFwTypesTest, GivenFirmwareUtilAndOnlyOpromCodeSupportedWhenCallingGetSupportedFwTypesThenExpectOpromFwTypeIsUnsupported) {
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();
    osLibHandle->funcMap["igsc_device_fw_version"] = reinterpret_cast<void *>(&mockIgscDeviceFwVersion);
    osLibHandle->funcMap["igsc_device_oprom_version"] = reinterpret_cast<void *>(&mockIgscDeviceOpromVersion);
    osLibHandle->funcMap["igsc_device_psc_version"] = reinterpret_cast<void *>(&mockIgscDevicePscVersion);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);

    mockGetFwVersion = IGSC_SUCCESS;
    mockGetOpromCodeVersion = IGSC_SUCCESS;
    mockGetOpromDataVersion = IGSC_ERROR_NOT_SUPPORTED;
    mockGetPscVersion = IGSC_SUCCESS;

    std::vector<std::string> fwTypes{};
    pFwUtilImp->getDeviceSupportedFwTypes(fwTypes);
    EXPECT_EQ(fwTypes.size(), 2u);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwFlashLateBindingTest, GivenFirmwareUtilImpAndLateBindingIsSupportedWhenCallingFlashFirmwareThenCallSucceeds) {
    VariableBackup<decltype(L0::Sysman::deviceUpdateLateBindingConfig)> mockDeviceUpdateLateBindingConfig(&L0::Sysman::deviceUpdateLateBindingConfig, [](struct igsc_device_handle *handle, uint32_t type, uint32_t flags, uint8_t *payload, size_t payloadSize, uint32_t *status) -> int {
        *status = CSC_LATE_BINDING_STATUS_SUCCESS;
        return IGSC_SUCCESS;
    });

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());

    for (auto type : lateBindingFirmwareTypes) {
        auto ret = pFwUtilImp->flashFirmware(type, (void *)testImage, ZES_STRING_PROPERTY_SIZE);
        EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    }

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwFlashLateBindingTest, GivenFirmwareUtilImpAndLateBindingIsSupportedWhenCallingFlashFirmwareAndIgscIsBusyThenCallFailsWithProperReturnCode) {
    VariableBackup<decltype(L0::Sysman::deviceUpdateLateBindingConfig)> mockDeviceUpdateLateBindingConfig(&L0::Sysman::deviceUpdateLateBindingConfig, [](struct igsc_device_handle *handle, uint32_t type, uint32_t flags, uint8_t *payload, size_t payloadSize, uint32_t *status) -> int {
        *status = CSC_LATE_BINDING_STATUS_SUCCESS;
        return IGSC_ERROR_BUSY;
    });

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());

    for (auto type : lateBindingFirmwareTypes) {
        auto ret = pFwUtilImp->flashFirmware(type, (void *)testImage, ZES_STRING_PROPERTY_SIZE);
        EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, ret);
    }

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwFlashLateBindingTest, GivenFirmwareUtilImpAndLateBindingIsSupportedWhenCallingFlashFirmwareAndStatusRetunedIsInvalidThenCallFailsWithProperReturnCode) {
    VariableBackup<decltype(L0::Sysman::deviceUpdateLateBindingConfig)> mockDeviceUpdateLateBindingConfig(&L0::Sysman::deviceUpdateLateBindingConfig, [](struct igsc_device_handle *handle, uint32_t type, uint32_t flags, uint8_t *payload, size_t payloadSize, uint32_t *status) -> int {
        *status = CSC_LATE_BINDING_STATUS_TIMEOUT;
        return IGSC_SUCCESS;
    });

    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());

    for (auto type : lateBindingFirmwareTypes) {
        auto ret = pFwUtilImp->flashFirmware(type, (void *)testImage, ZES_STRING_PROPERTY_SIZE);
        EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, ret);
    }

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwFlashLateBindingTest, GivenFirmwareUtilImpAndLateBindingIsSupportedWhenCallingGetLateBindingSupportedFwTypesThenProperFwTypesAreReturned) {
    L0::Sysman::FirmwareUtilImp *pFwUtilImp = new L0::Sysman::FirmwareUtilImp(0, 0, 0, 0);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());
    std::vector<std ::string> deviceSupportedFwTypes = {};
    pFwUtilImp->getLateBindingSupportedFwTypes(deviceSupportedFwTypes);
    EXPECT_EQ(deviceSupportedFwTypes.size(), lateBindingFirmwareTypes.size());
    for (auto type : deviceSupportedFwTypes) {
        EXPECT_NE(std::find(lateBindingFirmwareTypes.begin(), lateBindingFirmwareTypes.end(), type), lateBindingFirmwareTypes.end());
    }
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

} // namespace ult
} // namespace Sysman
} // namespace L0

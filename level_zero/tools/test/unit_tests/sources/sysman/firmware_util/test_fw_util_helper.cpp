/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/source/sysman/firmware_util/firmware_util_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/firmware_util/mock_fw_util_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {

extern pIgscIfrGetStatusExt deviceIfrGetStatusExt;
extern pIgscIfrRunMemPPRTest deviceIfrRunMemPPRTest;
extern pIgscGetEccConfig getEccConfig;
extern pIgscSetEccConfig setEccConfig;
namespace ult {

constexpr static uint32_t mockMaxTileCount = 2;
static int mockMemoryHealthIndicator = IGSC_HEALTH_INDICATOR_HEALTHY;

int mockDeviceIfrGetStatusExt(struct igsc_device_handle *handle, uint32_t *supportedTests, uint32_t *hwCapabilities, uint32_t *ifrApplied, uint32_t *prevErrors, uint32_t *pendingReset) {
    return 0;
}

int mockDeviceIfrRunMemPPRTest(struct igsc_device_handle *handle, uint32_t *status, uint32_t *pendingReset, uint32_t *errorCode) {
    return 0;
}

static inline int mockEccConfigGetSuccess(struct igsc_device_handle *handle, uint8_t *currentState, uint8_t *pendingState) {
    return 0;
}

static inline int mockEccConfigSetSuccess(struct igsc_device_handle *handle, uint8_t newState, uint8_t *currentState, uint8_t *pendingState) {
    return 0;
}

static inline int mockEccConfigGetFailure(struct igsc_device_handle *handle, uint8_t *currentState, uint8_t *pendingState) {
    return -1;
}

static inline int mockEccConfigSetFailure(struct igsc_device_handle *handle, uint8_t newState, uint8_t *currentState, uint8_t *pendingState) {
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

TEST(FwStatusExtTest, GivenIFRWasSetWhenFirmwareUtilChecksIFRThenIFRStatusIsUpdated) {

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(deviceIfrGetStatusExt)> mockDeviceIfrGetStatusExt(&deviceIfrGetStatusExt, [](struct igsc_device_handle *handle, uint32_t *supportedTests, uint32_t *hwCapabilities, uint32_t *ifrApplied, uint32_t *prevErrors, uint32_t *pendingReset) -> int {
        *ifrApplied = (IGSC_IFR_REPAIRS_MASK_DSS_EN_REPAIR | IGSC_IFR_REPAIRS_MASK_ARRAY_REPAIR);
        return 0;
    });

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(deviceIfrGetStatusExt)> mockDeviceIfrGetStatusExt(&deviceIfrGetStatusExt, [](struct igsc_device_handle *handle, uint32_t *supportedTests, uint32_t *hwCapabilities, uint32_t *ifrApplied, uint32_t *prevErrors, uint32_t *pendingReset) -> int {
        return -1;
    });

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    VariableBackup<decltype(deviceIfrGetStatusExt)> mockDeviceIfrGetStatusExt(&deviceIfrGetStatusExt, [](struct igsc_device_handle *handle, uint32_t *supportedTests, uint32_t *hwCapabilities, uint32_t *ifrApplied, uint32_t *prevErrors, uint32_t *pendingReset) -> int {
        *supportedTests = (IGSC_IFR_SUPPORTED_TESTS_MEMORY_PPR);
        return 0;
    });

    VariableBackup<decltype(deviceIfrRunMemPPRTest)> mockDeviceIfrRunMemPPRTest(&deviceIfrRunMemPPRTest, [](struct igsc_device_handle *handle, uint32_t *status, uint32_t *pendingReset, uint32_t *errorCode) -> int {
        *status = 0;
        return 0;
    });

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

TEST(FwGetProcAddressTest, GivenValidFwUtilMethodNameWhenFirmwareUtilIsInitializedThenCorrectMethodsAreLoaded) {
    struct IFRmockOsLibrary : OsLibrary {
      public:
        ~IFRmockOsLibrary() override = default;
        void *getProcAddress(const std::string &procName) override {
            ifrFuncMap["igsc_ifr_get_status_ext"] = reinterpret_cast<void *>(&mockDeviceIfrGetStatusExt);
            ifrFuncMap["igsc_ifr_run_mem_ppr_test"] = reinterpret_cast<void *>(&mockDeviceIfrRunMemPPRTest);
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
    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(domain, bus, device, function);
    std::vector<std::string> supportedDiagTests;
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new IFRmockOsLibrary());
    EXPECT_TRUE(pFwUtilImp->loadEntryPointsExt());
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwEccTest, GivenFwEccConfigCallFailsWhenCallingFirmwareUtilSetAndGetEccThenCorrespondingCallFails) {

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }
    struct IgscEccMockOsLibrary : public OsLibrary {
      public:
        ~IgscEccMockOsLibrary() override = default;
        void *getProcAddress(const std::string &procName) override {
            eccFuncMap["igsc_ecc_config_get"] = reinterpret_cast<void *>(&mockEccConfigGetFailure);
            eccFuncMap["igsc_ecc_config_set"] = reinterpret_cast<void *>(&mockEccConfigSetFailure);
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
    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(domain, bus, device, function);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new IgscEccMockOsLibrary());
    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    uint8_t newState = 0;
    auto ret = pFwUtilImp->fwGetEccConfig(&currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, ret);
    ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, ret);
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenValidFwUtilMethodWhenCallingFirmwareUtilSetAndGetEccThenCorrespondingCallSucceeds) {

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }
    struct IgscEccMockOsLibrary : public OsLibrary {
      public:
        ~IgscEccMockOsLibrary() override = default;
        void *getProcAddress(const std::string &procName) override {
            eccFuncMap["igsc_ecc_config_get"] = reinterpret_cast<void *>(&mockEccConfigGetSuccess);
            eccFuncMap["igsc_ecc_config_set"] = reinterpret_cast<void *>(&mockEccConfigSetSuccess);
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
    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(domain, bus, device, function);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new IgscEccMockOsLibrary());
    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    uint8_t newState = 0;
    auto ret = pFwUtilImp->fwGetEccConfig(&currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(LinuxFwEccTest, GivenGetProcAddrCallFailsWhenFirmwareUtilChecksEccGetAndSetConfigThenCorrespondingCallFails) {

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    uint16_t domain = 0;
    uint8_t bus = 0;
    uint8_t device = 0;
    uint8_t function = 0;
    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(domain, bus, device, function);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockFwUtilOsLibrary());
    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    uint8_t newState = 0;
    auto ret = pFwUtilImp->fwGetEccConfig(&currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, ret);
    ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, ret);
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

TEST(FwGetMemErrorCountTest, GivenGetProcAddrCallFailsWhenMemoryErrorCountIsRequestedThenFailureIsReturned) {

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

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
    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
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

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
    MockFwUtilOsLibrary *osLibHandle = new MockFwUtilOsLibrary();

    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(osLibHandle);
    zes_mem_health_t health = ZES_MEM_HEALTH_UNKNOWN;
    pFwUtilImp->fwGetMemoryHealthIndicator(&health);
    EXPECT_EQ(health, ZES_MEM_HEALTH_UNKNOWN);

    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

} // namespace ult
} // namespace L0

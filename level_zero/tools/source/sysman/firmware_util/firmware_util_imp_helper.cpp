/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/tools/source/sysman/firmware_util/firmware_util_imp.h"

std::vector<std ::string> deviceSupportedFirmwareTypes = {"GSC", "OptionROM", "PSC"};

namespace L0 {

const std::string fwDeviceIfrGetStatusExt = "igsc_ifr_get_status_ext";
const std::string fwIafPscUpdate = "igsc_iaf_psc_update";
const std::string fwGfspMemoryErrors = "igsc_gfsp_memory_errors";
const std::string fwDeviceIfrRunArrayScanTest = "igsc_ifr_run_array_scan_test";
const std::string fwDeviceIfrRunMemPPRTest = "igsc_ifr_run_mem_ppr_test";
const std::string fwEccConfigGet = "igsc_ecc_config_get";
const std::string fwEccConfigSet = "igsc_ecc_config_set";

pIgscIfrGetStatusExt deviceIfrGetStatusExt;
pIgscIafPscUpdate iafPscUpdate;
pIgscGfspMemoryErrors gfspMemoryErrors;
pIgscIfrRunArrayScanTest deviceIfrRunArrayScanTest;
pIgscIfrRunMemPPRTest deviceIfrRunMemPPRTest;
pIgscGetEccConfig getEccConfig;
pIgscSetEccConfig setEccConfig;

ze_result_t FirmwareUtilImp::fwIfrApplied(bool &ifrStatus) {
    uint32_t supportedTests = 0; // Bitmap holding the tests supported on the platform
    uint32_t prevErrors = 0;     // Bitmap holding which errors were seen on this SOC in previous tests
    uint32_t pendingReset = 0;   //  Whether a reset is pending after running the test
    uint32_t ifrApplied = 0;     // Bitmap holding hw capabilities on the platform

    auto result = fwCallGetstatusExt(supportedTests, ifrApplied, prevErrors, pendingReset);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    // check if either DSS or Memory PPR test completed and fix was applied
    ifrStatus = ((ifrApplied & IGSC_IFR_REPAIRS_MASK_DSS_EN_REPAIR) || (ifrApplied & IGSC_IFR_REPAIRS_MASK_ARRAY_REPAIR));
    return ZE_RESULT_SUCCESS;
}

// fwCallGetstatusExt() is a helper function to get the status of IFR after the diagnostics tests are run
ze_result_t FirmwareUtilImp::fwCallGetstatusExt(uint32_t &supportedTests, uint32_t &ifrApplied, uint32_t &prevErrors, uint32_t &pendingReset) {
    uint32_t hwCapabilities = 0;
    ifrApplied = 0;
    prevErrors = 0;
    pendingReset = 0;
    supportedTests = 0;

    int ret = deviceIfrGetStatusExt(&fwDeviceHandle, &supportedTests, &hwCapabilities, &ifrApplied, &prevErrors, &pendingReset);
    if (ret) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwGetMemoryErrorCount(zes_ras_error_type_t type, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    gfspMemoryErrors = reinterpret_cast<pIgscGfspMemoryErrors>(libraryHandle->getProcAddress(fwGfspMemoryErrors));
    if (gfspMemoryErrors != nullptr) {
        uint32_t numOfTiles = (subDeviceCount == 0) ? 1 : subDeviceCount;
        auto size = sizeof(igsc_gfsp_mem_err) + subDeviceCount * sizeof(igsc_gfsp_tile_mem_err);
        std::vector<uint8_t> buf(size);
        igsc_gfsp_mem_err *tiles = reinterpret_cast<igsc_gfsp_mem_err *>(buf.data());
        tiles->num_of_tiles = numOfTiles; // set the number of tiles in the structure that will be passed as a buffer
        int ret = gfspMemoryErrors(&fwDeviceHandle, tiles);
        if (ret != IGSC_SUCCESS) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        if (tiles->num_of_tiles != subDeviceCount) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        if (type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            count = tiles->errors[subDeviceId].corr_err;
        }
        if (type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            count = tiles->errors[subDeviceId].uncorr_err;
        }
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_UNINITIALIZED;
}

ze_result_t FirmwareUtilImp::fwGetEccConfig(uint8_t *currentState, uint8_t *pendingState) {
    getEccConfig = reinterpret_cast<pIgscGetEccConfig>(libraryHandle->getProcAddress(fwEccConfigGet));
    if (getEccConfig != nullptr) {
        int ret = getEccConfig(&fwDeviceHandle, currentState, pendingState);
        if (ret != IGSC_SUCCESS) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_UNINITIALIZED;
}

ze_result_t FirmwareUtilImp::fwSetEccConfig(uint8_t newState, uint8_t *currentState, uint8_t *pendingState) {
    setEccConfig = reinterpret_cast<pIgscSetEccConfig>(libraryHandle->getProcAddress(fwEccConfigSet));
    if (setEccConfig != nullptr) {
        int ret = setEccConfig(&fwDeviceHandle, newState, currentState, pendingState);
        if (ret != IGSC_SUCCESS) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_UNINITIALIZED;
}

ze_result_t FirmwareUtilImp::fwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) {
    uint32_t supportedTests = 0;
    uint32_t prevErrors = 0;
    uint32_t pendingReset = 0;
    uint32_t ifrApplied = 0;
    auto result = fwCallGetstatusExt(supportedTests, ifrApplied, prevErrors, pendingReset);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    if (supportedTests == IGSC_IFR_SUPPORTED_TESTS_ARRAY_AND_SCAN) {
        supportedDiagTests.push_back("ARRAY_AND_SCAN");
    }
    if (supportedTests == IGSC_IFR_SUPPORTED_TESTS_MEMORY_PPR) {
        supportedDiagTests.push_back("MEMORY_PPR");
    }
    if (supportedTests == (IGSC_IFR_SUPPORTED_TESTS_ARRAY_AND_SCAN | IGSC_IFR_SUPPORTED_TESTS_MEMORY_PPR)) {
        supportedDiagTests.push_back("ARRAY_AND_SCAN");
        supportedDiagTests.push_back("MEMORY_PPR");
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwRunDiagTests(std::string &osDiagType, zes_diag_result_t *pDiagResult) {
    uint32_t status = 0;
    uint32_t extendedStatus = 0;
    uint32_t pendingReset = 0;
    uint32_t errorCode = 0;

    if (osDiagType.compare("ARRAY_AND_SCAN") == 0) {
        int ret = deviceIfrRunArrayScanTest(&fwDeviceHandle, &status, &extendedStatus, &pendingReset, &errorCode);
        if (ret) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        switch (extendedStatus) {
        case IGSC_IFR_EXT_STS_PASSED:
        case IGSC_IFR_EXT_STS_NO_REPAIR_NEEDED:
            *pDiagResult = ZES_DIAG_RESULT_NO_ERRORS;
            break;
        case IGSC_IFR_EXT_STS_REPAIRED_ARRAY:
        case IGSC_IFR_EXT_STS_REPAIRED_SUBSLICE:
        case IGSC_IFR_EXT_STS_REPAIRED_ARRAY_SUBSLICE:
            *pDiagResult = ZES_DIAG_RESULT_REBOOT_FOR_REPAIR;
            break;
        case IGSC_IFR_EXT_STS_REPAIR_NOT_SUPPORTED:
        case IGSC_IFR_EXT_STS_NO_RESORCES:
        case IGSC_IFR_EXT_STS_NON_SUBSLICE_IN_ARRAY:
        case IGSC_IFR_EXT_STS_NON_SUBSLICE_IN_SCAN:
            *pDiagResult = ZES_DIAG_RESULT_FAIL_CANT_REPAIR;
            break;
        case IGSC_IFR_EXT_STS_TEST_ERROR:
            *pDiagResult = ZES_DIAG_RESULT_ABORT;
            break;
        }
    }
    if (osDiagType.compare("MEMORY_PPR") == 0) {
        int ret = deviceIfrRunMemPPRTest(&fwDeviceHandle, &status, &pendingReset, &errorCode);
        if (ret) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        if (status == 1) {
            *pDiagResult = ZES_DIAG_RESULT_REBOOT_FOR_REPAIR;
        }
        if (status == 0) {
            *pDiagResult = ZES_DIAG_RESULT_NO_ERRORS;
        }
    }
    return ZE_RESULT_SUCCESS;
}

static void progressFunc(uint32_t done, uint32_t total, void *ctx) {
    uint32_t percent = (done * 100) / total;
    PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stdout, "Progess: %d/%d:%d/%\n", done, total, percent);
}

ze_result_t FirmwareUtilImp::pscGetVersion(std::string &fwVersion) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::fwFlashIafPsc(void *pImage, uint32_t size) {
    iafPscUpdate = reinterpret_cast<pIgscIafPscUpdate>(libraryHandle->getProcAddress(fwIafPscUpdate));

    if (iafPscUpdate == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    int ret = iafPscUpdate(&fwDeviceHandle, static_cast<const uint8_t *>(pImage), size, progressFunc, nullptr);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::flashFirmware(std::string fwType, void *pImage, uint32_t size) {
    if (fwType == deviceSupportedFirmwareTypes[0]) { // GSC
        return fwFlashGSC(pImage, size);
    }
    if (fwType == deviceSupportedFirmwareTypes[1]) { // OPROM
        return fwFlashOprom(pImage, size);
    }
    if (fwType == deviceSupportedFirmwareTypes[2]) { // PSC
        return fwFlashIafPsc(pImage, size);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void FirmwareUtilImp::getDeviceSupportedFwTypes(std::vector<std::string> &fwTypes) {
    fwTypes = deviceSupportedFirmwareTypes;
}

ze_result_t FirmwareUtilImp::getFwVersion(std::string fwType, std::string &firmwareVersion) {
    if (fwType == deviceSupportedFirmwareTypes[0]) { // GSC
        return fwGetVersion(firmwareVersion);
    }
    if (fwType == deviceSupportedFirmwareTypes[1]) { // OPROM
        return opromGetVersion(firmwareVersion);
    }
    if (fwType == deviceSupportedFirmwareTypes[2]) { // PSC
        return pscGetVersion(firmwareVersion);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool FirmwareUtilImp::loadEntryPointsExt() {
    bool ok = getSymbolAddr(fwDeviceIfrGetStatusExt, deviceIfrGetStatusExt);
    ok = ok && getSymbolAddr(fwDeviceIfrRunMemPPRTest, deviceIfrRunMemPPRTest);
    ok = ok && getSymbolAddr(fwDeviceIfrRunArrayScanTest, deviceIfrRunArrayScanTest);

    return ok;
}
} // namespace L0

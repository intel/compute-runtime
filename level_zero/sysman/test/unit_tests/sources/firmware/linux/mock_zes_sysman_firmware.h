/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/firmware/linux/sysman_os_firmware_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockFwHandlesCount = 3;
const std::string mockFwVersion("DG01->0->2026");
const std::string mockOpromVersion("OPROM CODE VERSION:123_OPROM DATA VERSION:456");
const std::string mockPscVersion("version 1 : 2021/09/15 00:43:12");
const std::string mockUnknownVersion("unknown");
std::vector<std::string> mockSupportedFirmwareTypes = {"GSC", "OptionROM", "PSC"};
std::vector<std::string> mockUnsupportedFwTypes = {"unknown"};
std::string mockEmpty = {};
class FirmwareInterface : public L0::Sysman::FirmwareUtil {};
class FirmwareFsAccess : public L0::Sysman::FsAccessInterface {};
class FirmwareSysfsAccess : public L0::Sysman::SysFsAccessInterface {};

struct MockFirmwareFsAccess : public FirmwareFsAccess {};

struct MockFirmwareSysfsAccess : public L0::Sysman::SysFsAccessInterface {

    ze_result_t readResult = ZE_RESULT_SUCCESS;
    ze_result_t scanDirEntriesResult = ZE_RESULT_SUCCESS;
    ze_bool_t isNullDirEntries = false;

    ze_result_t read(const std::string file, std::string &val) override {

        if (readResult != ZE_RESULT_SUCCESS) {
            return readResult;
        }

        if (!file.compare("device/iaf.31/pscbin_version") || !file.compare("device/iaf.0/pscbin_version")) {
            val = mockPscVersion;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t scanDirEntries(const std::string dir, std::vector<std::string> &list) override {
        if (scanDirEntriesResult != ZE_RESULT_SUCCESS) {
            return scanDirEntriesResult;
        }
        if (!isNullDirEntries) {
            if (!dir.compare("device/")) {
                list.push_back(std::string("unusedfile"));
                list.push_back(std::string("iaf.31"));
                list.push_back(std::string("iaf.0"));
            }
        } else {
            if (!dir.compare("device/")) {
                list.clear();
            }
        }
        return ZE_RESULT_SUCCESS;
    }

    MockFirmwareSysfsAccess() = default;
    ~MockFirmwareSysfsAccess() override = default;
};

struct MockFirmwareInterface : public FirmwareInterface {

    ze_result_t getFwVersionResult = ZE_RESULT_SUCCESS;
    ze_bool_t isFirmwareVersionsSupported = true;

    ze_result_t mockFwGetVersion(std::string &fwVersion) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockOpromGetVersion(std::string &fwVersion) {
        fwVersion = mockOpromVersion;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockPscGetVersion(std::string &fwVersion) {
        fwVersion = mockPscVersion;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t getFwVersion(std::string fwType, std::string &firmwareVersion) override {

        if (getFwVersionResult != ZE_RESULT_SUCCESS) {
            return getFwVersionResult;
        }

        if (fwType == "GSC") {
            firmwareVersion = mockFwVersion;
        } else if (fwType == "OptionROM") {
            firmwareVersion = mockOpromVersion;
        } else if (fwType == "PSC") {
            firmwareVersion = mockPscVersion;
        }
        return ZE_RESULT_SUCCESS;
    }

    void getDeviceSupportedFwTypes(std::vector<std::string> &fwTypes) override {
        if (isFirmwareVersionsSupported) {
            fwTypes = mockSupportedFirmwareTypes;
        }
    }

    void getLateBindingSupportedFwTypes(std::vector<std::string> &fwTypes) override {
        fwTypes.insert(fwTypes.end(), lateBindingFirmwareTypes.begin(), lateBindingFirmwareTypes.end());
    }

    MockFirmwareInterface() = default;

    ADDMETHOD_NOBASE(fwDeviceInit, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(getFlashFirmwareProgress, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t * pCompletionPercent));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwIfrApplied, ze_result_t, ZE_RESULT_SUCCESS, (bool &ifrStatus));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE(fwGetEccAvailable, ze_result_t, ZE_RESULT_SUCCESS, (ze_bool_t * pAvailable));
    ADDMETHOD_NOBASE(fwGetEccConfigurable, ze_result_t, ZE_RESULT_SUCCESS, (ze_bool_t * pConfigurable));
    ADDMETHOD_NOBASE(fwGetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t * currentState, uint8_t *pendingState, uint8_t *defaultState));
    ADDMETHOD_NOBASE(fwSetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t newState, uint8_t *currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE_VOIDRETURN(fwGetMemoryHealthIndicator, (zes_mem_health_t * health));
};

class PublicLinuxFirmwareImp : public L0::Sysman::LinuxFirmwareImp {
  public:
    using LinuxFirmwareImp::pFwInterface;
};

} // namespace ult
} // namespace Sysman
} // namespace L0

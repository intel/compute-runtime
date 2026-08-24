/*
 * Copyright (C) 2020-2026 Intel Corporation
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
const std::string mockLateBindingVersion("1.2.3.4");
const std::string mockUnknownVersion("unknown");
const std::vector<std::string> mockSupportedFirmwareTypes = {"GSC", "OptionROM", "PSC"};
const std::vector<std::string> mockUnsupportedFwTypes = {"unknown"};
const std::string mockEmpty = {};

// For FDO related tests
constexpr uint32_t mockFwHandlesCountFdo = 1;

// PCI BDF of the device under test: 0000:03:05.1
constexpr uint32_t mockPciDomain = 0;
constexpr uint32_t mockPciBus = 3;
constexpr uint32_t mockPciDevice = 5;
constexpr uint32_t mockPciFunction = 1;

// Names of the MTD devices as exposed by the KMD in /proc/mtd. They hold the id of the auxiliary
// device, which is derived from the PCI BDF, excluding the domain, by concatenating bus, device and
// function as hex and reading the result back as hex. Hence 03:05.1 -> 0x351 -> 849 and
// 04:00.0 -> 0x400 -> 1024
const std::string mockMtdDataDeviceName = "xe.nvm.849.DATA";
const std::string mockMtdOtherDeviceDataName = "xe.nvm.1024.DATA";

const std::vector<std::string> mockSupportedFirmwareTypesFdo = {"Flash_Override"};

class FirmwareInterface : public L0::Sysman::FirmwareUtil {};
class FirmwareFsAccess : public L0::Sysman::FsAccessInterface {};
class FirmwareSysfsAccess : public L0::Sysman::SysFsAccessInterface {};

struct MockFirmwareSysfsAccess : public L0::Sysman::SysFsAccessInterface {

    ze_result_t readResult = ZE_RESULT_SUCCESS;
    ze_result_t canReadResult = ZE_RESULT_SUCCESS;
    ze_result_t scanDirEntriesResult = ZE_RESULT_SUCCESS;
    ze_bool_t isNullDirEntries = false;
    std::string mockSurvivabilityModeValue = "";

    ze_result_t read(const std::string file, std::string &val) override {

        if (readResult != ZE_RESULT_SUCCESS) {
            return readResult;
        }

        if (!file.compare("device/iaf.31/pscbin_version") || !file.compare("device/iaf.0/pscbin_version")) {
            val = mockPscVersion;
        }
        if (!file.compare("device/lb_voltage_regulator_version") || !file.compare("device/lb_fan_control_version")) {
            val = mockLateBindingVersion;
        }

        if (!file.compare("survivability_mode")) {
            val = mockSurvivabilityModeValue;
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
    ze_result_t canRead(const std::string file) override {
        if (canReadResult != ZE_RESULT_SUCCESS) {
            return canReadResult;
        }
        return ZE_RESULT_SUCCESS;
    }

    MockFirmwareSysfsAccess() = default;
    ~MockFirmwareSysfsAccess() override = default;
};

struct MockFirmwareProcfsAccess : public L0::Sysman::ProcFsAccessInterface {

    MockFirmwareProcfsAccess() = default;
    ~MockFirmwareProcfsAccess() override = default;
};

struct MockFirmwareFsAccess : public L0::Sysman::FsAccessInterface {

    ze_result_t readResult = ZE_RESULT_SUCCESS;
    std::string mockFdoValue = "disabled";

    // Shapes of /proc/mtd which the flash flow has to handle
    enum class MtdMode {
        dataDevice,         // This device's DATA device only
        noMtdDevices,       // Header line only
        emptyMtdFile,       // No lines at all
        otherDeviceOnly,    // Another PCI device's DATA device only
        multipleDevices,    // This device's DATA device second, with a different size
        noDataDevice,       // Other regions of this device, but no DATA one
        mismatchedQuotes,   // DATA device names with mismatched quotes
        malformedMtdLine,   // Entry with missing fields
        malformedDeviceSize // DATA device entry with a non hexadecimal size
    };

    MtdMode mtdMode = MtdMode::dataDevice;
    std::string dataDeviceName = mockMtdDataDeviceName;

    static constexpr uint32_t mockMtdDataDeviceSize = 0x800000;
    static constexpr uint32_t mockMtdSecondDataDeviceSize = 0x400000;

    ze_result_t read(const std::string file, std::vector<std::string> &val) override {
        if (readResult != ZE_RESULT_SUCCESS) {
            return readResult;
        }

        if (!file.compare("/proc/mtd")) {
            val.clear();

            switch (mtdMode) {
            case MtdMode::noMtdDevices:
                val.push_back("dev: size erasesize name");
                return ZE_RESULT_SUCCESS;

            case MtdMode::emptyMtdFile:
                return ZE_RESULT_SUCCESS;

            case MtdMode::otherDeviceOnly:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd1: 00800000 00001000 \"" + mockMtdOtherDeviceDataName + "\"");
                return ZE_RESULT_SUCCESS;

            case MtdMode::multipleDevices:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd1: 00800000 00001000 \"" + mockMtdOtherDeviceDataName + "\"");
                val.push_back("mtd2: 00400000 00001000 \"" + dataDeviceName + "\"");
                return ZE_RESULT_SUCCESS;

            case MtdMode::noDataDevice:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd1: 00800000 00001000 \"xe.nvm.849.GSC\"");
                val.push_back("mtd2: 00800000 00001000 \"xe.nvm.849.DATA.BACKUP\"");
                return ZE_RESULT_SUCCESS;

            case MtdMode::mismatchedQuotes:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd1: 00800000 00001000 \"" + dataDeviceName);
                val.push_back("mtd2: 00800000 00001000 " + dataDeviceName + "\"");
                return ZE_RESULT_SUCCESS;

            case MtdMode::malformedMtdLine:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd1: incomplete");
                return ZE_RESULT_SUCCESS;

            case MtdMode::malformedDeviceSize:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd1: notasize 00001000 \"" + dataDeviceName + "\"");
                return ZE_RESULT_SUCCESS;

            case MtdMode::dataDevice:
            default:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd1: 00800000 00001000 \"" + dataDeviceName + "\"");
                return ZE_RESULT_SUCCESS;
            }
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t read(const std::string file, std::string &val) override {
        if (readResult != ZE_RESULT_SUCCESS) {
            return readResult;
        }

        if (file.find("/survivability_info/fdo_mode") != std::string::npos) {
            val = mockFdoValue;
            return readResult;
        }

        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    MockFirmwareFsAccess() = default;
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
    ADDMETHOD_NOBASE(getFlashFirmwareProgress, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t *pCompletionPercent));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwIfrApplied, ze_result_t, ZE_RESULT_SUCCESS, (bool &ifrStatus));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE(fwGetEccAvailable, ze_result_t, ZE_RESULT_SUCCESS, (ze_bool_t * pAvailable));
    ADDMETHOD_NOBASE(fwGetEccConfigurable, ze_result_t, ZE_RESULT_SUCCESS, (ze_bool_t * pConfigurable));
    ADDMETHOD_NOBASE(fwGetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t *currentState, uint8_t *pendingState, uint8_t *defaultState));
    ADDMETHOD_NOBASE(fwSetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t newState, uint8_t *currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE(fwSetGfspConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t gfspHeciCmdCode, std::vector<uint8_t> inBuf, std::vector<uint8_t> &outBuf));
    ADDMETHOD_NOBASE(fwGetGfspConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t gfspHeciCmdCode, std::vector<uint8_t> &outBuf));
    ADDMETHOD_NOBASE(fwGetSerialNumber, ze_result_t, ZE_RESULT_SUCCESS, (std::array<uint8_t, IGSC_MAX_OEM_SN_LENGTH> & serialNumber, uint16_t &serialNumberLen));
    ADDMETHOD_NOBASE_VOIDRETURN(fwGetMemoryHealthIndicator, (zes_mem_health_t * health));
};

class PublicLinuxFirmwareImp : public L0::Sysman::LinuxFirmwareImp {
  public:
    using LinuxFirmwareImp::pFwInterface;
};

} // namespace ult
} // namespace Sysman
} // namespace L0

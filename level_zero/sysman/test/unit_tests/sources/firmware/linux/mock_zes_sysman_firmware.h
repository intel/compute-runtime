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
const std::string mockFdoValue("enabled");
const std::string mockLateBindingVersion("1.2.3.4");
const std::string mockUnknownVersion("unknown");
const std::vector<std::string> mockSupportedFirmwareTypes = {"GSC", "OptionROM", "PSC"};
const std::vector<std::string> mockUnsupportedFwTypes = {"unknown"};
const std::string mockEmpty = {};

// For FDO related tests
constexpr uint32_t mockFwHandlesCountFdo = 1;
constexpr uint32_t mockPciBus = 3;
constexpr uint32_t mockPciDevice = 5;
constexpr uint32_t mockPciFunction = 1;
const std::vector<std::string> mockSupportedFirmwareTypesFdo = {"Flash_Override"};

class FirmwareInterface : public L0::Sysman::FirmwareUtil {};
class FirmwareFsAccess : public L0::Sysman::FsAccessInterface {};
class FirmwareSysfsAccess : public L0::Sysman::SysFsAccessInterface {};

struct MockFirmwareSysfsAccess : public L0::Sysman::SysFsAccessInterface {

    ze_result_t readResult = ZE_RESULT_SUCCESS;
    ze_result_t canReadResult = ZE_RESULT_SUCCESS;
    ze_result_t scanDirEntriesResult = ZE_RESULT_SUCCESS;
    ze_bool_t isNullDirEntries = false;

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

        if (!file.compare("device/survivability_info/fdo_mode")) {
            val = mockFdoValue;
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

    // Test control flags
    enum class MtdRegionMode {
        singleRegion,       // Default: Single MTD device with DESCRIPTOR and GSC regions
        multipleRegions,    // Multiple MTD devices (mtd3-6) with various region types
        noRegions,          // Empty /proc/mtd file with header only
        emptyMtdFile,       // Completely empty /proc/mtd file (no lines at all)
        bdfMisMatch,        // MTD entries that don't match expected device BDF
        noDescriptorRegion, // MTD entries without required DESCRIPTOR region
        mtdNumberNoColon,   // MTD entry without trailing colon (for coverage testing)
        shortDeviceName,    // MTD entry with device name too short for region extraction
        malformedQuotes,    // MTD entries with mismatched quotes (covers both quote conditions)
        malformedMtdLine    // MTD entries with insufficient fields (covers parsing failure)
    };

    MtdRegionMode regionMode = MtdRegionMode::singleRegion;

    // Mock constants for MTD device BDF
    const std::string mockProcMtdStringPrefix = "xe.nvm.";
    std::string expectedDeviceBdf = std::to_string(mockPciBus) + std::to_string(mockPciDevice) + std::to_string(mockPciFunction); // Formed from mockPciBus, mockPciDevice, mockPciFunction

    ze_result_t read(const std::string file, std::vector<std::string> &val) override {
        if (readResult != ZE_RESULT_SUCCESS) {
            return readResult;
        }

        if (!file.compare("/proc/mtd")) {
            val.clear();

            switch (regionMode) {
            case MtdRegionMode::noRegions:
                val.push_back("dev: size erasesize name"); // Only header
                return ZE_RESULT_SUCCESS;

            case MtdRegionMode::emptyMtdFile:
                // Completely empty file - no lines at all (tests mtdLines.empty() condition)
                return ZE_RESULT_SUCCESS;

            case MtdRegionMode::bdfMisMatch:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd0: 00800000 00001000 \"other.device.region1\"");
                val.push_back("mtd1: 00800000 00001000 \"another.device.region2\"");
                return ZE_RESULT_SUCCESS;

            case MtdRegionMode::multipleRegions:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd3: 00800000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".DESCRIPTOR\"");
                val.push_back("mtd4: 0054d000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".GSC\"");
                val.push_back("mtd5: 00200000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".OptionROM\"");
                val.push_back("mtd6: 00010000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".DAM\"");
                return ZE_RESULT_SUCCESS;

            case MtdRegionMode::noDescriptorRegion:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd4: 00800000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".GSC\"");
                val.push_back("mtd7: 00800000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".PADDING\"");
                return ZE_RESULT_SUCCESS;

            case MtdRegionMode::mtdNumberNoColon:
                val.push_back("dev: size erasesize name");
                val.push_back("mtd3 00800000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".DESCRIPTOR\"");
                val.push_back("mtd4: 00800000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".GSC\"");
                return ZE_RESULT_SUCCESS;

            case MtdRegionMode::shortDeviceName:
                val.push_back("dev: size erasesize name");
                // Device name exactly matches prefix (no region suffix) - tests the length check
                val.push_back("mtd3: 00800000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + "\"");
                // Device name with just the dot but no region - also too short
                val.push_back("mtd4: 00800000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".\"");
                // No valid DESCRIPTOR device - should cause flash operation to fail
                return ZE_RESULT_SUCCESS;

            case MtdRegionMode::malformedQuotes:
                val.push_back("dev: size erasesize name");
                // One entry starts with quote but doesn't end with quote - tests first condition
                val.push_back("mtd3: 00800000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".DESCRIPTOR");
                // Another entry ends with quote but doesn't start with quote - tests second condition
                val.push_back("mtd4: 00800000 00001000 " + mockProcMtdStringPrefix + expectedDeviceBdf + ".GSC\"");
                return ZE_RESULT_SUCCESS;

            case MtdRegionMode::malformedMtdLine:
                val.push_back("dev: size erasesize name");
                // Malformed MTD line with insufficient fields - tests parsing failure of iss >> mtdNumber >> size >> eraseSize >> name
                val.push_back("mtd3: incomplete"); // Only 2 fields instead of 4 - should fail parsing, no valid DESCRIPTOR found
                return ZE_RESULT_SUCCESS;

            case MtdRegionMode::singleRegion:
            default:
                // Single region case with DESCRIPTOR and GSC regions (can be small or large)
                val.push_back("dev: size erasesize name");
                val.push_back("mtd3: 00800000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".DESCRIPTOR\"");
                val.push_back("mtd4: 0054d000 00001000 \"" + mockProcMtdStringPrefix + expectedDeviceBdf + ".GSC\"");
                return ZE_RESULT_SUCCESS;
            }
        }

        return ZE_RESULT_SUCCESS;
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
    ADDMETHOD_NOBASE(fwSetGfspConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t gfspHeciCmdCode, std::vector<uint8_t> inBuf, std::vector<uint8_t> &outBuf));
    ADDMETHOD_NOBASE(fwGetGfspConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t gfspHeciCmdCode, std::vector<uint8_t> &outBuf));
    ADDMETHOD_NOBASE_VOIDRETURN(fwGetMemoryHealthIndicator, (zes_mem_health_t * health));
};

class PublicLinuxFirmwareImp : public L0::Sysman::LinuxFirmwareImp {
  public:
    using LinuxFirmwareImp::pFwInterface;
};

} // namespace ult
} // namespace Sysman
} // namespace L0

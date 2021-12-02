/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/firmware/linux/os_firmware_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

namespace L0 {
namespace ult {

constexpr uint32_t mockHandleCount = 2;
const std::string mockFwVersion("DG01->0->2026");
const std::string mockOpromVersion("OPROM CODE VERSION:123_OPROM DATA VERSION:456");
std::vector<std::string> mockSupportedFwTypes = {"GSC", "OptionROM"};
std::vector<std::string> mockUnsupportedFwTypes = {"unknown"};
std::string mockEmpty = {};
class FirmwareInterface : public FirmwareUtil {};
class FirmwareFsAccess : public FsAccess {};

template <>
struct Mock<FirmwareFsAccess> : public FirmwareFsAccess {
    MOCK_METHOD(ze_result_t, read, (const std::string file, std::vector<std::string> &val), (override));
    ze_result_t readValSuccess(const std::string file, std::vector<std::string> &val) {
        val.push_back("mtd3: 005ef000 00001000 \"i915-spi.42.auto.GSC\"");
        val.push_back("mtd5: 00200000 00001000 \"i915-spi.42.auto.OptionROM\"");
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t readMtdValSuccess(const std::string file, std::vector<std::string> &val) {
        val.push_back("mtd3: 005ef000 00001000 \"i915-spi.42.auto.GSC\"");
        val.push_back("mtd3: 005ef000 00001000 \"i915-spi.42.auto.GSC\"");
        return ZE_RESULT_SUCCESS;
    }
};

template <>
struct Mock<FirmwareInterface> : public FirmwareUtil {

    ze_result_t mockFwGetVersion(std::string &fwVersion) {
        fwVersion = mockFwVersion;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockOpromGetVersion(std::string &fwVersion) {
        fwVersion = mockOpromVersion;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockGetFwVersion(std::string fwType, std::string &firmwareVersion) {
        if (fwType == "GSC") {
            firmwareVersion = mockFwVersion;
        } else if (fwType == "OptionROM") {
            firmwareVersion = mockOpromVersion;
        }
        return ZE_RESULT_SUCCESS;
    }

    Mock<FirmwareInterface>() = default;

    MOCK_METHOD(ze_result_t, fwDeviceInit, (), (override));
    MOCK_METHOD(ze_result_t, getFirstDevice, (igsc_device_info * info), (override));
    MOCK_METHOD(ze_result_t, getFwVersion, (std::string fwType, std::string &firmwareVersion), (override));
    MOCK_METHOD(ze_result_t, flashFirmware, (std::string fwType, void *pImage, uint32_t size), (override));
    MOCK_METHOD(ze_result_t, fwIfrApplied, (bool &ifrStatus), (override));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    MOCK_METHOD(void, getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes), (override));
};

class PublicLinuxFirmwareImp : public L0::LinuxFirmwareImp {
  public:
    using LinuxFirmwareImp::pFwInterface;
};
} // namespace ult
} // namespace L0

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
    ze_result_t readValFailure(const std::string file, std::vector<std::string> &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
};

template <>
struct Mock<FirmwareInterface> : public FirmwareUtil {

    ze_result_t mockFwDeviceInit(void) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwDeviceInitFail(void) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    ze_result_t mockFwGetVersion(std::string &fwVersion) {
        fwVersion = mockFwVersion;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockOpromGetVersion(std::string &fwVersion) {
        fwVersion = mockOpromVersion;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockGetFirstDevice(igsc_device_info *info) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwFlash(void *pImage, uint32_t size) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwGetVersionFailed(std::string &fwVersion) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    Mock<FirmwareInterface>() = default;

    MOCK_METHOD(ze_result_t, fwDeviceInit, (), (override));
    MOCK_METHOD(ze_result_t, fwGetVersion, (std::string & fwVersion), (override));
    MOCK_METHOD(ze_result_t, opromGetVersion, (std::string & fwVersion), (override));
    MOCK_METHOD(ze_result_t, getFirstDevice, (igsc_device_info * info), (override));
    MOCK_METHOD(ze_result_t, fwFlashGSC, (void *pImage, uint32_t size), (override));
    MOCK_METHOD(ze_result_t, fwFlashOprom, (void *pImage, uint32_t size), (override));
    MOCK_METHOD(ze_result_t, fwIfrApplied, (bool &ifrStatus), (override));
    MOCK_METHOD(ze_result_t, fwSupportedDiagTests, (std::vector<std::string> & supportedDiagTests), (override));
    MOCK_METHOD(ze_result_t, fwRunDiagTests, (std::string & osDiagType, zes_diag_result_t *pResult, uint32_t subdeviceId), (override));
};

class PublicLinuxFirmwareImp : public L0::LinuxFirmwareImp {
  public:
    using LinuxFirmwareImp::pFwInterface;
};
} // namespace ult
} // namespace L0

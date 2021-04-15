/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/diagnostics/linux/os_diagnostics_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

namespace L0 {
namespace ult {

constexpr uint32_t mockDiagHandleCount = 1;
class DiagnosticsInterface : public FirmwareUtil {};

template <>
struct Mock<DiagnosticsInterface> : public FirmwareUtil {

    ze_result_t mockFwDeviceInit(void) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwDeviceInitFail(void) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    ze_result_t mockFwGetVersion(std::string &fwVersion) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockOpromGetVersion(std::string &fwVersion) {
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

    Mock<DiagnosticsInterface>() = default;

    MOCK_METHOD(ze_result_t, fwDeviceInit, (), (override));
    MOCK_METHOD(ze_result_t, fwGetVersion, (std::string & fwVersion), (override));
    MOCK_METHOD(ze_result_t, opromGetVersion, (std::string & fwVersion), (override));
    MOCK_METHOD(ze_result_t, getFirstDevice, (igsc_device_info * info), (override));
    MOCK_METHOD(ze_result_t, fwFlashGSC, (void *pImage, uint32_t size), (override));
    MOCK_METHOD(ze_result_t, fwFlashOprom, (void *pImage, uint32_t size), (override));
    MOCK_METHOD(ze_result_t, fwIfrApplied, (bool &ifrStatus), (override));
};

class PublicLinuxDiagnosticsImp : public L0::LinuxDiagnosticsImp {
  public:
    using LinuxDiagnosticsImp::pFwInterface;
};
} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2021 Intel Corporation
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

constexpr uint32_t mockDiagHandleCount = 2;
const std::string mockQuiescentGpuFile("quiesce_gpu");
const std::string mockinvalidateLmemFile("invalidate_lmem_mmaps");
const std::vector<std::string> mockSupportedDiagTypes = {"MOCKSUITE1", "MOCKSUITE2"};
class DiagnosticsInterface : public FirmwareUtil {};

template <>
struct Mock<DiagnosticsInterface> : public FirmwareUtil {

    ze_result_t mockFwDeviceInit(void) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwDeviceInitFail(void) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    ze_result_t mockGetFirstDevice(igsc_device_info *info) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) {
        supportedDiagTests.push_back(mockSupportedDiagTypes[0]);
        supportedDiagTests.push_back(mockSupportedDiagTypes[1]);
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwRunDiagTestsReturnSuccess(std::string &osDiagType, zes_diag_result_t *pResult, uint32_t subDeviceId) {
        *pResult = ZES_DIAG_RESULT_NO_ERRORS;
        return ZE_RESULT_SUCCESS;
    }

    Mock<DiagnosticsInterface>() = default;

    MOCK_METHOD(ze_result_t, fwDeviceInit, (), (override));
    MOCK_METHOD(ze_result_t, fwGetVersion, (std::string & fwVersion), (override));
    MOCK_METHOD(ze_result_t, opromGetVersion, (std::string & fwVersion), (override));
    MOCK_METHOD(ze_result_t, getFirstDevice, (igsc_device_info * info), (override));
    MOCK_METHOD(ze_result_t, fwFlashGSC, (void *pImage, uint32_t size), (override));
    MOCK_METHOD(ze_result_t, fwFlashOprom, (void *pImage, uint32_t size), (override));
    MOCK_METHOD(ze_result_t, fwIfrApplied, (bool &ifrStatus), (override));
    MOCK_METHOD(ze_result_t, fwSupportedDiagTests, (std::vector<std::string> & supportedDiagTests), (override));
    MOCK_METHOD(ze_result_t, fwRunDiagTests, (std::string & osDiagType, zes_diag_result_t *pResult, uint32_t subDeviceId), (override));
};

class DiagSysfsAccess : public SysfsAccess {};
template <>
struct Mock<DiagSysfsAccess> : public DiagSysfsAccess {

    ze_result_t mockwrite(const std::string file, const int val) {
        if (std::string::npos != file.find(mockQuiescentGpuFile)) {
            return ZE_RESULT_SUCCESS;
        } else if (std::string::npos != file.find(mockinvalidateLmemFile)) {
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }

    ze_result_t mockwriteFails(const std::string file, const int val) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    Mock<DiagSysfsAccess>() = default;

    MOCK_METHOD(ze_result_t, write, (const std::string file, const int val), (override));
};

class PublicLinuxDiagnosticsImp : public L0::LinuxDiagnosticsImp {
  public:
    using LinuxDiagnosticsImp::pFwInterface;
};
} // namespace ult
} // namespace L0

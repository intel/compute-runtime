/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"
#include "level_zero/tools/source/sysman/sysman.h"

#include "gmock/gmock.h"

using ::testing::_;
using namespace NEO;

namespace L0 {
namespace ult {

class FwUtilInterface : public FirmwareUtil {};
struct MockFwUtilInterface : public FwUtilInterface {

    MockFwUtilInterface() = default;

    ADDMETHOD_NOBASE(fwDeviceInit, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(getFirstDevice, ze_result_t, ZE_RESULT_SUCCESS, (igsc_device_info * info));
    ADDMETHOD_NOBASE(getFwVersion, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, std::string &firmwareVersion));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwIfrApplied, ze_result_t, ZE_RESULT_SUCCESS, (bool &ifrStatus));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE(fwGetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t * currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE(fwSetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t newState, uint8_t *currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));
};

class OsLibraryUtil : public OsLibrary {};
struct MockOsLibrary : public OsLibraryUtil {
  public:
    static bool mockLoad;
    MockOsLibrary(const std::string &name, std::string *errorValue) {
    }
    MockOsLibrary() {}
    ~MockOsLibrary() override = default;
    void *getProcAddress(const std::string &procName) override {
        return nullptr;
    }
    bool isLoaded() override {
        return false;
    }
    static OsLibrary *load(const std::string &name) {
        if (mockLoad == true) {
            auto ptr = new (std::nothrow) MockOsLibrary();
            return ptr;
        } else {
            return nullptr;
        }
    }
    std::map<std::string, void *> funcMap;
};

} // namespace ult
} // namespace L0

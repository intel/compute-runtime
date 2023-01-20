/*
 * Copyright (C) 2022-2023 Intel Corporation
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

using namespace NEO;

namespace L0 {
namespace ult {

struct MockFwUtilInterface : public FirmwareUtil {

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

struct MockFwUtilOsLibrary : public OsLibrary {
  public:
    static bool mockLoad;
    static bool getNonNullProcAddr;
    MockFwUtilOsLibrary(const std::string &name, std::string *errorValue) {
    }
    MockFwUtilOsLibrary() {}
    ~MockFwUtilOsLibrary() override = default;
    void *getProcAddress(const std::string &procName) override {
        if (getNonNullProcAddr) {
            return reinterpret_cast<void *>(&mockDeviceIteratorCreate);
        }
        return nullptr;
    }
    bool isLoaded() override {
        return false;
    }
    static OsLibrary *load(const std::string &name) {
        if (mockLoad == true) {
            auto ptr = new (std::nothrow) MockFwUtilOsLibrary();
            return ptr;
        } else {
            return nullptr;
        }
    }

    static inline int mockDeviceIteratorCreate(struct igsc_device_iterator **iter) {
        return -1;
    }

    std::map<std::string, void *> funcMap;
};

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/ecc/ecc_imp.h"
#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

namespace L0 {
namespace ult {

class EccFwInterface : public FirmwareUtil {};
template <>
struct Mock<EccFwInterface> : public EccFwInterface {
    ze_result_t mockFwGetEccConfigResult = ZE_RESULT_SUCCESS;
    ze_result_t mockFwSetEccConfigResult = ZE_RESULT_SUCCESS;
    ze_result_t mockFwDeviceInit = ZE_RESULT_SUCCESS;

    ze_bool_t mockSetConfig = true;
    uint8_t mockCurrentState = 0;
    uint8_t mockPendingState = 0;

    ze_result_t fwDeviceInit() {
        return mockFwDeviceInit;
    }

    ze_result_t fwGetEccConfig(uint8_t *currentState, uint8_t *pendingState) override {
        if (mockFwGetEccConfigResult != ZE_RESULT_SUCCESS) {
            return mockFwGetEccConfigResult;
        }

        *currentState = mockCurrentState;
        *pendingState = mockPendingState;

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t fwSetEccConfig(uint8_t newState, uint8_t *currentState, uint8_t *pendingState) override {
        if (mockFwSetEccConfigResult != ZE_RESULT_SUCCESS) {
            return mockFwSetEccConfigResult;
        }

        if (mockSetConfig == static_cast<ze_bool_t>(true)) {
            mockPendingState = newState;
        }
        *currentState = mockCurrentState;
        *pendingState = mockPendingState;

        return ZE_RESULT_SUCCESS;
    }

    ADDMETHOD_NOBASE(getFirstDevice, ze_result_t, ZE_RESULT_SUCCESS, (igsc_device_info * info));
    ADDMETHOD_NOBASE(getFwVersion, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, std::string &firmwareVersion));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwIfrApplied, ze_result_t, ZE_RESULT_SUCCESS, (bool &ifrStatus));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));

    Mock<EccFwInterface>() = default;
    ~Mock<EccFwInterface>() override = default;
};

} // namespace ult
} // namespace L0

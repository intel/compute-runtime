/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/ecc/sysman_ecc_imp.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct EccFwInterface : public L0::Sysman::FirmwareUtil {

    ze_result_t mockFwGetEccConfigResult = ZE_RESULT_SUCCESS;
    ze_result_t mockFwSetEccConfigResult = ZE_RESULT_SUCCESS;
    ze_result_t mockFwDeviceInit = ZE_RESULT_SUCCESS;

    ze_bool_t mockSetConfig = true;
    uint8_t mockCurrentState = 0;
    uint8_t mockPendingState = 0;

    ze_result_t fwDeviceInit() override {
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

    ADDMETHOD_NOBASE(getFwVersion, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, std::string &firmwareVersion));
    ADDMETHOD_NOBASE(getFlashFirmwareProgress, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t * pCompletionPercent));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwIfrApplied, ze_result_t, ZE_RESULT_SUCCESS, (bool &ifrStatus));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));
    ADDMETHOD_NOBASE_VOIDRETURN(fwGetMemoryHealthIndicator, (zes_mem_health_t * health));
};

} // namespace ult
} // namespace Sysman
} // namespace L0

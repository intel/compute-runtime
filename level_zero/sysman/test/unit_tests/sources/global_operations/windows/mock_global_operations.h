/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/sysman/source/api/global_operations/sysman_global_operations_imp.h"
#include "level_zero/sysman/source/api/global_operations/windows/sysman_os_global_operations_imp.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::string unknown("unknown");

struct MockGlobalOpsKmdSysManager : public MockKmdSysManager {

    void setGlobalOperationsProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pRequest);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderIn);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::GlobalOperation::TriggerDeviceLevelReset: {
            uint32_t *value = reinterpret_cast<uint32_t *>(pBuffer);
            *value = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }
};

struct MockGlobalOpsFwInterface : public L0::Sysman::FirmwareUtil {
    MockGlobalOpsFwInterface() = default;

    ADDMETHOD_NOBASE(fwDeviceInit, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(getFwVersion, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, std::string &firmwareVersion));
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
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));
    ADDMETHOD_NOBASE_VOIDRETURN(fwGetMemoryHealthIndicator, (zes_mem_health_t * health));
    ADDMETHOD_NOBASE_VOIDRETURN(getLateBindingSupportedFwTypes, (std::vector<std::string> & fwTypes));

    ze_result_t mockSerialNumberError = ZE_RESULT_SUCCESS;
    std::array<uint8_t, IGSC_MAX_OEM_SN_LENGTH> mockSerialNumberData = {};
    uint16_t mockSerialNumberLen = 0;

    ze_result_t fwGetSerialNumber(std::array<uint8_t, IGSC_MAX_OEM_SN_LENGTH> &serialNumber, uint16_t &serialNumberLen) override {
        if (mockSerialNumberError != ZE_RESULT_SUCCESS) {
            return mockSerialNumberError;
        }
        serialNumber = mockSerialNumberData;
        serialNumberLen = mockSerialNumberLen;
        return ZE_RESULT_SUCCESS;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0

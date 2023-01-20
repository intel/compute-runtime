/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/global_operations/global_operations_imp.h"
#include "level_zero/tools/source/sysman/global_operations/windows/os_global_operations_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace ult {

class GlobalOpsKmdSysManager : public Mock<MockKmdSysManager> {};

template <>
struct Mock<GlobalOpsKmdSysManager> : public GlobalOpsKmdSysManager {

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
    Mock() = default;
    ~Mock() override = default;
};

} // namespace ult
} // namespace L0

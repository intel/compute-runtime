/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_kmd_sys_manager.h"

#include "sysman/fan/fan_imp.h"

namespace L0 {
namespace ult {

class FanKmdSysManager : public Mock<MockKmdSysManager> {};

template <>
struct Mock<FanKmdSysManager> : public FanKmdSysManager {

    uint32_t mockFanMaxPoints = 10;
    uint32_t mockFanCurrentPulses = 523436;
    uint32_t mockFanCurrentFanPoints = 0;

    void getFanProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Fans::MaxFanControlPointsSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockFanMaxPoints;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Fans::CurrentFanSpeed: {
            if (fanSupported) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockFanCurrentPulses;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            } else {
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            }
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void setFanProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pRequest);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderIn);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Fans::CurrentNumOfControlPoints: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockFanCurrentFanPoints = *pValue;
            pResponse->outDataSize = 0;
            if ((mockFanCurrentFanPoints % 2 == 0) && (mockFanCurrentFanPoints > 0) && (mockFanCurrentFanPoints <= mockFanMaxPoints)) {
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            } else if (mockFanCurrentFanPoints == 0) {
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            } else {
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            }
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    Mock() = default;
    ~Mock() = default;
};

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/fan/sysman_fan_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockFanKmdSysManager : public MockKmdSysManager {
    union {
        struct
        {
            uint32_t TemperatureDegreesCelsius : 16;
            uint32_t FanSpeedPercent : 16;
        };
        uint32_t Data;
    } mockFanTempSpeed;
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
        case KmdSysman::Requests::Fans::CurrentNumOfControlPoints: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockFanCurrentFanPoints;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Fans::CurrentFanPoint: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockFanTempSpeed.FanSpeedPercent = 25;
            mockFanTempSpeed.TemperatureDegreesCelsius = 50;
            *pValue = mockFanTempSpeed.Data;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
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
        case KmdSysman::Requests::Fans::CurrentFanPoint: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockFanTempSpeed.Data = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0

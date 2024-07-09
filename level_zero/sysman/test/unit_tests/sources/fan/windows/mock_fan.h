/*
 * Copyright (C) 2024 Intel Corporation
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
    } mockFanTempSpeed[10], mockStockFanTable[10];
    uint32_t mockFanMaxPoints = 10;
    uint32_t mockFanCurrentPulses = 523436;
    uint32_t mockFanCurrentFanPoints = 0;
    uint32_t mockCurrentReadIndex = 0;
    uint32_t mockCurrentWriteIndex = 0;
    bool isFanTableSet = false;
    bool isStockFanTableAvailable = false;
    bool failMaxPointsGet = false;
    bool retZeroMaxPoints = false;
    bool smallStockTable = false;

    void getFanProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Fans::MaxFanControlPointsSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockFanMaxPoints;
            if (retZeroMaxPoints) {
                *pValue = 0;
            }
            if (!failMaxPointsGet) {
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            } else {
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
                pResponse->outDataSize = 0;
            }

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
            mockCurrentReadIndex = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Fans::CurrentFanPoint: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            if (isFanTableSet) {
                mockFanTempSpeed[mockCurrentReadIndex].FanSpeedPercent = mockCurrentReadIndex * 10;
                mockFanTempSpeed[mockCurrentReadIndex].TemperatureDegreesCelsius = mockCurrentReadIndex * 10 + 20;
                *pValue = mockFanTempSpeed[mockCurrentReadIndex].Data;
                mockCurrentReadIndex++;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            } else if (isStockFanTableAvailable) {
                mockStockFanTable[mockCurrentReadIndex].FanSpeedPercent = mockCurrentReadIndex * 10 - 5;
                mockStockFanTable[mockCurrentReadIndex].TemperatureDegreesCelsius = mockCurrentReadIndex * 10 + 15;
                *pValue = mockStockFanTable[mockCurrentReadIndex].Data;
                mockCurrentReadIndex++;
                if (smallStockTable && mockCurrentReadIndex == 6) {
                    // only return 5 points for smallStockTable
                    *pValue = 0;
                    pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
                    pResponse->outDataSize = 0;
                } else {
                    pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                    pResponse->outDataSize = sizeof(uint32_t);
                }
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
                mockCurrentWriteIndex = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            } else if (mockFanCurrentFanPoints == 0) {
                isFanTableSet = false;
                mockCurrentReadIndex = 0;
                mockCurrentWriteIndex = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            } else {
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            }
        } break;
        case KmdSysman::Requests::Fans::CurrentFanPoint: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockFanTempSpeed[mockCurrentWriteIndex++].Data = *pValue;
            isFanTableSet = true;
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

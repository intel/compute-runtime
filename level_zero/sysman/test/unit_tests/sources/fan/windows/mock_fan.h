/*
 * Copyright (C) 2024-2025 Intel Corporation
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
    uint32_t mockFanMaxPoints = 10;                   // Maximum number of fan control points
    uint32_t mockFanCurrentPulses = 523436;           // Current fan speed in pulses/RPM
    uint32_t mockFanCurrentFanPoints = 0;             // Default: no fan points set
    uint32_t mockCurrentReadIndex = 0;                // Default: start reading from beginning
    uint32_t mockCurrentWriteIndex = 0;               // Default: start writing from beginning
    uint32_t mockSupportedFanCount = 1;               // Number of supported fans
    uint32_t mockMaxFanSpeed = 3000;                  // Mock max RPM value
    uint32_t mockSupportedFanModeCapabilities = 0x02; // Default: singleSupported = 1
    bool isFanTableSet = false;                       // Default: no custom fan table set
    bool mockReturnNoStockTable = false;              // Default: don't force no stock table scenario
    bool mockReturnSmallStockTable = false;           // Default: don't force small stock table scenario

    // Controllable failure flags for each fan request type (similar to power mock pattern)
    uint32_t mockFanFailure[KmdSysman::Requests::Fans::MaxFanRequests] = {0};

    uint32_t getReturnCode(uint32_t fanRequestCode) {
        return mockFanFailure[fanRequestCode] ? KmdSysman::KmdSysmanFail : KmdSysman::KmdSysmanSuccess;
    }

    void getFanProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Fans::NumFanDomains: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockSupportedFanCount;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Fans::SupportedFanModeCapabilities: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            // Set configurable fan mode capabilities
            *pValue = mockSupportedFanModeCapabilities;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Fans::MaxFanSpeedSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxFanSpeed;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Fans::MaxFanControlPointsSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);

            // Use explicit scenario flags for clearer test control
            if (mockReturnNoStockTable) {
                *pValue = 0; // No stock table available
            } else if (mockReturnSmallStockTable) {
                *pValue = 5; // Small stock table
            } else {
                // Default behavior: normal stock table with full points
                *pValue = mockFanMaxPoints;
            }

            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Fans::CurrentFanSpeed: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockFanCurrentPulses;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Fans::CurrentNumOfControlPoints: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockFanCurrentFanPoints;
            mockCurrentReadIndex = 0;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Fans::CurrentFanPoint: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);

            // Check controllable failure first
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            if (pResponse->outReturnCode != KmdSysman::KmdSysmanSuccess) {
                pResponse->outDataSize = 0;
                break;
            }

            if (isFanTableSet) {
                // Return custom fan table points set by test
                mockFanTempSpeed[mockCurrentReadIndex].FanSpeedPercent = mockCurrentReadIndex * 10;
                mockFanTempSpeed[mockCurrentReadIndex].TemperatureDegreesCelsius = mockCurrentReadIndex * 10 + 20;
                *pValue = mockFanTempSpeed[mockCurrentReadIndex].Data;
                mockCurrentReadIndex++;
                pResponse->outDataSize = sizeof(uint32_t);
            } else if (mockReturnNoStockTable) {
                // No stock table available - fail immediately
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            } else if (mockReturnSmallStockTable) {
                // Small stock table - return 5 points then fail
                if (mockCurrentReadIndex < 5) {
                    mockStockFanTable[mockCurrentReadIndex].FanSpeedPercent = mockCurrentReadIndex * 10 - 5;
                    mockStockFanTable[mockCurrentReadIndex].TemperatureDegreesCelsius = mockCurrentReadIndex * 10 + 15;
                    *pValue = mockStockFanTable[mockCurrentReadIndex].Data;
                    mockCurrentReadIndex++;
                    pResponse->outDataSize = sizeof(uint32_t);
                } else {
                    // End of small table
                    *pValue = 0;
                    pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
                    pResponse->outDataSize = 0;
                }
            } else {
                // Default: full stock table available
                mockStockFanTable[mockCurrentReadIndex].FanSpeedPercent = mockCurrentReadIndex * 10 - 5;
                mockStockFanTable[mockCurrentReadIndex].TemperatureDegreesCelsius = mockCurrentReadIndex * 10 + 15;
                *pValue = mockStockFanTable[mockCurrentReadIndex].Data;
                mockCurrentReadIndex++;
                pResponse->outDataSize = sizeof(uint32_t);
            }
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void setFanProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pRequest);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderIn);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Fans::CurrentFanMode: {
            // Just acknowledge the fan mode setting
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
        } break;
        case KmdSysman::Requests::Fans::CurrentFanIndex: {
            // Just acknowledge the fan index setting
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
        } break;
        case KmdSysman::Requests::Fans::CurrentNumOfControlPoints: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockFanCurrentFanPoints = *pValue;

            pResponse->outDataSize = 0;

            // Check controllable failure first
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            if (pResponse->outReturnCode != KmdSysman::KmdSysmanSuccess) {
                break;
            }

            if ((mockFanCurrentFanPoints % 2 == 0) && (mockFanCurrentFanPoints > 0) && (mockFanCurrentFanPoints <= mockFanMaxPoints)) {
                mockCurrentWriteIndex = 0;
            } else if (mockFanCurrentFanPoints == 0) {
                isFanTableSet = false;
                mockCurrentReadIndex = 0;
                mockCurrentWriteIndex = 0;
            } else {
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            }
        } break;
        case KmdSysman::Requests::Fans::CurrentFanPoint: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockFanTempSpeed[mockCurrentWriteIndex++].Data = *pValue;
            isFanTableSet = true;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
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

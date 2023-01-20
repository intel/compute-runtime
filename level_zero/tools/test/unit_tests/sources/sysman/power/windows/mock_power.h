/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_kmd_sys_manager.h"

#include "sysman/power/power_imp.h"

namespace L0 {
namespace ult {

class PowerKmdSysManager : public Mock<MockKmdSysManager> {};

template <>
struct Mock<PowerKmdSysManager> : public PowerKmdSysManager {

    uint32_t mockPowerLimit1Enabled = 1;
    uint32_t mockPowerLimit2Enabled = 1;
    int32_t mockPowerLimit1 = 25000;
    int32_t mockPowerLimit2 = 41000;
    int32_t mockTauPowerLimit1 = 20800;
    uint32_t mockTpdDefault = 34000;
    uint32_t mockMinPowerLimit = 1000;
    uint32_t mockMaxPowerLimit = 80000;
    int32_t mockAcPowerPeak = 0;
    int32_t mockDcPowerPeak = 0;
    uint32_t mockEnergyThreshold = 0;
    uint32_t mockEnergyCounter = 3231121;
    uint32_t mockTimeStamp = 1123412412;
    uint32_t mockEnergyUnit = 14;
    uint64_t mockEnergyCounter64Bit = 32323232323232;
    uint32_t mockFrequencyTimeStamp = 38400000;

    void getActivityProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Activity::TimestampFrequency: {
            uint32_t *pValueFrequency = reinterpret_cast<uint32_t *>(pBuffer);
            *pValueFrequency = mockFrequencyTimeStamp;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void getPowerProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Power::EnergyThresholdSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = static_cast<uint32_t>(this->allowSetCalls);
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::TdpDefault: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockTpdDefault;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::MinPowerLimitDefault: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMinPowerLimit;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::MaxPowerLimitDefault: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxPowerLimit;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::PowerLimit1Enabled: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockPowerLimit1Enabled;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::PowerLimit2Enabled: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockPowerLimit2Enabled;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit1: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            *pValue = mockPowerLimit1;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(int32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit1Tau: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            *pValue = mockTauPowerLimit1;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(int32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit2: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            *pValue = mockPowerLimit2;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(int32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit4Ac: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            *pValue = mockAcPowerPeak;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(int32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit4Dc: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            *pValue = mockDcPowerPeak;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentEnergyThreshold: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockEnergyThreshold;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentEnergyCounter: {
            uint32_t *pValueCounter = reinterpret_cast<uint32_t *>(pBuffer);
            uint64_t *pValueTS = reinterpret_cast<uint64_t *>(pBuffer + sizeof(uint32_t));
            *pValueCounter = mockEnergyCounter;
            *pValueTS = mockTimeStamp;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t) + sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Power::EnergyCounterUnits: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockEnergyUnit;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentEnergyCounter64Bit: {
            uint64_t *pValueCounter = reinterpret_cast<uint64_t *>(pBuffer);
            uint64_t *pValueTS = reinterpret_cast<uint64_t *>(pBuffer + sizeof(uint64_t));
            *pValueCounter = mockEnergyCounter64Bit;
            *pValueTS = mockTimeStamp;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint64_t) + sizeof(uint64_t);
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void setPowerProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pRequest);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderIn);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Power::CurrentPowerLimit1: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            mockPowerLimit1 = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit1Tau: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            mockTauPowerLimit1 = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit2: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            mockPowerLimit2 = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit4Ac: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            mockAcPowerPeak = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit4Dc: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            mockDcPowerPeak = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Power::CurrentEnergyThreshold: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockEnergyThreshold = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
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

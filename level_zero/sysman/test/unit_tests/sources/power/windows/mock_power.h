/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {
namespace ult {
constexpr uint32_t mockLimitCount = 4u;

struct PowerKmdSysManager : public MockKmdSysManager {

    uint32_t mockPowerDomainCount = 2;
    uint32_t mockPowerLimit1Enabled = 1;
    uint32_t mockPowerLimit2Enabled = 1;
    uint32_t mockPowerLimit4Enabled = 1;
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
    uint32_t mockPowerFailure[KmdSysman::Requests::Power::MaxPowerRequests] = {0};

    void getActivityProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
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

    uint32_t getReturnCode(uint32_t powerRequestCode) {
        return mockPowerFailure[powerRequestCode] ? KmdSysman::KmdSysmanFail : KmdSysman::KmdSysmanSuccess;
    }

    void getPowerProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Power::NumPowerDomains: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockPowerDomainCount;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::EnergyThresholdSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = static_cast<uint32_t>(this->allowSetCalls);
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::TdpDefault: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockTpdDefault;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::MinPowerLimitDefault: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMinPowerLimit;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::MaxPowerLimitDefault: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxPowerLimit;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::PowerLimit1Enabled: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockPowerLimit1Enabled;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::PowerLimit2Enabled: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockPowerLimit2Enabled;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit1: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            *pValue = mockPowerLimit1;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(int32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit1Tau: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            *pValue = mockTauPowerLimit1;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(int32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit2: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            *pValue = mockPowerLimit2;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(int32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit4Ac: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            *pValue = mockAcPowerPeak;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(int32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit4Dc: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            *pValue = mockDcPowerPeak;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(int32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentEnergyThreshold: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockEnergyThreshold;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentEnergyCounter: {
            uint32_t *pValueCounter = reinterpret_cast<uint32_t *>(pBuffer);
            uint64_t *pValueTS = reinterpret_cast<uint64_t *>(pBuffer + sizeof(uint32_t));
            *pValueCounter = mockEnergyCounter;
            *pValueTS = mockTimeStamp;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t) + sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Power::EnergyCounterUnits: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockEnergyUnit;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Power::CurrentEnergyCounter64Bit: {
            uint64_t *pValueCounter = reinterpret_cast<uint64_t *>(pBuffer);
            uint64_t *pValueTS = reinterpret_cast<uint64_t *>(pBuffer + sizeof(uint64_t));
            memcpy_s(pValueCounter, sizeof(uint64_t), &mockEnergyCounter64Bit, sizeof(mockEnergyCounter64Bit));
            memcpy_s(pValueTS, sizeof(uint64_t), &mockTimeStamp, sizeof(mockEnergyCounter64Bit));
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint64_t) + sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Power::PowerLimit4Enabled: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockPowerLimit4Enabled;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
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
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit1Tau: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            mockTauPowerLimit1 = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit2: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            mockPowerLimit2 = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit4Ac: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            mockAcPowerPeak = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
        } break;
        case KmdSysman::Requests::Power::CurrentPowerLimit4Dc: {
            int32_t *pValue = reinterpret_cast<int32_t *>(pBuffer);
            mockDcPowerPeak = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
        } break;
        case KmdSysman::Requests::Power::CurrentEnergyThreshold: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockEnergyThreshold = *pValue;
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

class PublicPlatformMonitoringTech : public L0::Sysman::PlatformMonitoringTech {
  public:
    PublicPlatformMonitoringTech(std::vector<wchar_t> deviceInterfaceList, SysmanProductHelper *pSysmanProductHelper) : PlatformMonitoringTech(deviceInterfaceList, pSysmanProductHelper) {}
    using PlatformMonitoringTech::keyOffsetMap;
};

} // namespace ult
} // namespace Sysman
} // namespace L0

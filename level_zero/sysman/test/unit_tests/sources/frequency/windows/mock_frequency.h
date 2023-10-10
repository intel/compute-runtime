/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/frequency/windows/sysman_os_frequency_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockFrequencyKmdSysManager : public MockKmdSysManager {
    uint32_t mockNumberOfDomains = 3;
    uint32_t mockSupportedDomains = 7;
    uint32_t mockDomainType[3] = {ZES_FREQ_DOMAIN_GPU, ZES_FREQ_DOMAIN_MEMORY, ZES_FREQ_DOMAIN_MEDIA};
    bool mockGPUCanControl[3] = {true, false, false};
    bool mockGPUCannotControl[3] = {false, false, false};
    uint32_t mockMinFrequencyRange = 400;
    uint32_t mockMaxFrequencyRange = 1200;
    uint32_t mockRpn[3] = {400, 0, 400};
    uint32_t mockRp0[3] = {1200, 0, 1200};
    uint32_t mockRequestedFrequency = 600;
    uint32_t mockTdpFrequency = 0;
    uint32_t mockResolvedFrequency[3] = {600, 4200, 600};
    uint32_t mockEfficientFrequency = 400;
    uint32_t mockCurrentVoltage = 1100;
    uint32_t mockThrottleReasons = 0;
    uint32_t mockIccMax = 1025;
    uint32_t mockTjMax = 105;

    uint32_t mockIsExtendedModeSupported[3] = {0, 0, 0};
    uint32_t mockIsFixedModeSupported[3] = {0, 0, 0};
    uint32_t mockIsHighVoltModeCapable[3] = {0, 0, 0};
    uint32_t mockIsHighVoltModeEnabled[3] = {0, 0, 0};
    uint32_t mockIsIccMaxSupported = 1;
    uint32_t mockIsOcSupported[3] = {0, 0, 0};
    uint32_t mockIsTjMaxSupported = 1;
    uint32_t mockMaxFactoryDefaultFrequency[3] = {600, 4200, 600};
    uint32_t mockMaxFactoryDefaultVoltage[3] = {1200, 1300, 1200};
    uint32_t mockMaxOcFrequency[3] = {1800, 4500, 1800};
    uint32_t mockMaxOcVoltage[3] = {1300, 1400, 1300};
    uint32_t mockFixedMode[3] = {0, 0, 0};
    uint32_t mockVoltageMode[3] = {0, 0, 0};
    uint32_t mockHighVoltageSupported[3] = {0, 0, 0};
    uint32_t mockHighVoltageEnabled[3] = {0, 0, 0};
    uint32_t mockFrequencyTarget[3] = {0, 0, 0};
    uint32_t mockVoltageTarget[3] = {0, 0, 0};
    uint32_t mockVoltageOffset[3] = {0, 0, 0};

    void getFrequencyProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        KmdSysman::GeneralDomainsType domain = static_cast<KmdSysman::GeneralDomainsType>(pRequest->inCommandParam);

        if (domain < KmdSysman::GeneralDomainsType::GeneralDomainDGPU || domain >= KmdSysman::GeneralDomainsType::GeneralDomainMaxTypes) {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            return;
        }

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Frequency::NumFrequencyDomains: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockNumberOfDomains;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::SupportedFreqDomains: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockSupportedDomains;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::ExtendedOcSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockIsExtendedModeSupported[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::CanControlFrequency: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = allowSetCalls ? mockGPUCanControl[domain] : mockGPUCannotControl[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::FixedModeSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockFixedMode[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::HighVoltageModeSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockHighVoltageSupported[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::HighVoltageEnabled: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockHighVoltageEnabled[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::FrequencyOcSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockIsOcSupported[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::CurrentIccMax: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockIccMax;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::CurrentTjMax: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockTjMax;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::MaxNonOcFrequencyDefault: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxFactoryDefaultFrequency[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::MaxNonOcVoltageDefault: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxFactoryDefaultVoltage[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::MaxOcFrequencyDefault: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxOcFrequency[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::MaxOcVoltageDefault: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxOcVoltage[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::CurrentFixedMode: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockFixedMode[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::CurrentFrequencyTarget: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockFrequencyTarget[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::CurrentVoltageTarget: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockVoltageTarget[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::CurrentVoltageOffset: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockVoltageOffset[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::CurrentVoltageMode: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockVoltageMode[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::FrequencyThrottledEventSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::CurrentFrequencyRange: {
            if (domain == KmdSysman::GeneralDomainsType::GeneralDomainDGPU) {
                uint32_t *pMinFreq = reinterpret_cast<uint32_t *>(pBuffer);
                uint32_t *pMaxFreq = reinterpret_cast<uint32_t *>(pBuffer + sizeof(uint32_t));
                *pMinFreq = mockMinFrequencyRange;
                *pMaxFreq = mockMaxFrequencyRange;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = 2 * sizeof(uint32_t);
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainHBM) {
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainMedia) {
                uint32_t *pMinFreq = reinterpret_cast<uint32_t *>(pBuffer);
                uint32_t *pMaxFreq = reinterpret_cast<uint32_t *>(pBuffer + sizeof(uint32_t));
                *pMinFreq = mockMinFrequencyRange;
                *pMaxFreq = mockMaxFrequencyRange;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = 2 * sizeof(uint32_t);
            }
        } break;
        case KmdSysman::Requests::Frequency::CurrentRequestedFrequency: {
            if (domain == KmdSysman::GeneralDomainsType::GeneralDomainDGPU) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockRequestedFrequency;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainHBM) {
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainMedia) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockRequestedFrequency;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            }
        } break;
        case KmdSysman::Requests::Frequency::CurrentTdpFrequency: {
            if (domain == KmdSysman::GeneralDomainsType::GeneralDomainDGPU) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockTdpFrequency;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainHBM) {
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainMedia) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockTdpFrequency;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            }
        } break;
        case KmdSysman::Requests::Frequency::CurrentResolvedFrequency: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockResolvedFrequency[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Frequency::CurrentEfficientFrequency: {
            if (domain == KmdSysman::GeneralDomainsType::GeneralDomainDGPU) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockEfficientFrequency;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainHBM) {
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainMedia) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockEfficientFrequency;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            }
        } break;
        case KmdSysman::Requests::Frequency::FrequencyRangeMaxDefault: {
            if (domain == KmdSysman::GeneralDomainsType::GeneralDomainDGPU) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockRp0[domain];
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainHBM) {
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainMedia) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockRp0[domain];
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            }
        } break;
        case KmdSysman::Requests::Frequency::FrequencyRangeMinDefault: {
            if (domain == KmdSysman::GeneralDomainsType::GeneralDomainDGPU) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockRpn[domain];
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainHBM) {
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainMedia) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockRpn[domain];
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            }
        } break;
        case KmdSysman::Requests::Frequency::CurrentVoltage: {
            if (domain == KmdSysman::GeneralDomainsType::GeneralDomainDGPU) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockCurrentVoltage;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainHBM) {
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainMedia) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockCurrentVoltage;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            }
        } break;
        case KmdSysman::Requests::Frequency::CurrentThrottleReasons: {
            if (domain == KmdSysman::GeneralDomainsType::GeneralDomainDGPU) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockThrottleReasons;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainHBM) {
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainMedia) {
                uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
                *pValue = mockThrottleReasons;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
                pResponse->outDataSize = sizeof(uint32_t);
            }
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void setFrequencyProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pRequest);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderIn);

        KmdSysman::GeneralDomainsType domain = static_cast<KmdSysman::GeneralDomainsType>(pRequest->inCommandParam);

        if (domain < KmdSysman::GeneralDomainsType::GeneralDomainDGPU || domain >= KmdSysman::GeneralDomainsType::GeneralDomainMaxTypes) {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            return;
        }
        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Frequency::CurrentFrequencyRange: {
            if (domain == KmdSysman::GeneralDomainsType::GeneralDomainDGPU) {
                uint32_t *pMinFreq = reinterpret_cast<uint32_t *>(pBuffer);
                uint32_t *pMaxFreq = reinterpret_cast<uint32_t *>(pBuffer + sizeof(uint32_t));
                mockMinFrequencyRange = *pMinFreq;
                mockMaxFrequencyRange = *pMaxFreq;
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainHBM) {
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::ReturnCodes::DomainServiceNotSupported;
            } else if (domain == KmdSysman::GeneralDomainsType::GeneralDomainMedia) {
                uint32_t *pMinFreq = reinterpret_cast<uint32_t *>(pBuffer);
                uint32_t *pMaxFreq = reinterpret_cast<uint32_t *>(pBuffer + sizeof(uint32_t));
                mockMinFrequencyRange = *pMinFreq;
                mockMaxFrequencyRange = *pMaxFreq;
                pResponse->outDataSize = 0;
                pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            }
        } break;
        case KmdSysman::Requests::Frequency::CurrentFixedMode: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockFixedMode[domain] = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Frequency::CurrentVoltageMode: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockVoltageMode[domain] = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Frequency::CurrentVoltageOffset: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockVoltageOffset[domain] = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Frequency::CurrentVoltageTarget: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockVoltageTarget[domain] = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Frequency::CurrentFrequencyTarget: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockFrequencyTarget[domain] = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Frequency::CurrentIccMax: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockIccMax = *pValue;
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
        } break;
        case KmdSysman::Requests::Frequency::CurrentTjMax: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockTjMax = *pValue;
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

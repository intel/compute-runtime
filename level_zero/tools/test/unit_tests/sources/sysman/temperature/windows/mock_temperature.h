/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/temperature/temperature_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace ult {

constexpr uint32_t temperatureHandleComponentCount = 3u;
class TemperatureKmdSysManager : public Mock<MockKmdSysManager> {};

template <>
struct Mock<TemperatureKmdSysManager> : public TemperatureKmdSysManager {
    uint32_t mockTempDomainCount = temperatureHandleComponentCount;
    uint32_t mockTempGlobal = 26;
    uint32_t mockTempGPU = 25;
    uint32_t mockTempMemory = 23;
    uint32_t mockMaxTemperature = 100;
    bool isIntegrated = false;
    zes_temp_sensors_t mockSensorTypes[3] = {ZES_TEMP_SENSORS_GLOBAL, ZES_TEMP_SENSORS_GPU, ZES_TEMP_SENSORS_MEMORY};

    void getTemperatureProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        KmdSysman::TemperatureDomainsType domain = static_cast<KmdSysman::TemperatureDomainsType>(pRequest->inCommandParam);

        if (domain < KmdSysman::TemperatureDomainsType::TemperatureDomainPackage || domain >= KmdSysman::TemperatureDomainsType::TempetatureMaxDomainTypes) {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            return;
        }

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Temperature::NumTemperatureDomains: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            if (isIntegrated == true) {
                *pValue = 0;
            } else {
                *pValue = mockTempDomainCount;
            }
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Temperature::TempCriticalEventSupported:
        case KmdSysman::Requests::Temperature::TempThreshold1EventSupported:
        case KmdSysman::Requests::Temperature::TempThreshold2EventSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Temperature::MaxTempSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxTemperature;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Temperature::CurrentTemperature: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            switch (domain) {
            case KmdSysman::TemperatureDomainsType::TemperatureDomainPackage: {
                *pValue = mockTempGlobal;
            } break;
            case KmdSysman::TemperatureDomainsType::TemperatureDomainDGPU: {
                *pValue = mockTempGPU;
            } break;
            case KmdSysman::TemperatureDomainsType::TemperatureDomainHBM: {
                *pValue = mockTempMemory;
            } break;
            default:
                break;
            }
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
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

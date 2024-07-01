/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/sysman/source/api/temperature/sysman_temperature_imp.h"
#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::wstring pmtInterfaceName = L"TEST\0";
constexpr uint32_t temperatureHandleComponentCount = 3u;

struct TemperatureKmdSysManager : public MockKmdSysManager {
    uint32_t mockTempDomainCount = temperatureHandleComponentCount;
    uint32_t mockTempGlobal = 26;
    uint32_t mockTempGPU = 25;
    uint32_t mockTempMemory = 23;
    uint32_t mockMaxTemperature = 100;
    bool isIntegrated = false;
    zes_temp_sensors_t mockSensorTypes[3] = {ZES_TEMP_SENSORS_GLOBAL, ZES_TEMP_SENSORS_GPU, ZES_TEMP_SENSORS_MEMORY};
    uint32_t mockTemperatureFailure[KmdSysman::Requests::Temperature::MaxTemperatureRequests] = {0};

    uint32_t getReturnCode(uint32_t temperatureRequestCode) {
        return mockTemperatureFailure[temperatureRequestCode] ? KmdSysman::KmdSysmanFail : KmdSysman::KmdSysmanSuccess;
    }

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
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
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
                UNRECOVERABLE_IF(true);
            }
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
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

struct MockSysmanProductHelperTemp : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
    MockSysmanProductHelperTemp() = default;
    ADDMETHOD_NOBASE(getSensorTemperature, ze_result_t, ZE_RESULT_SUCCESS, (double *pTemperature, zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp));
    ADDMETHOD_NOBASE(isTempModuleSupported, bool, true, (zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp));
};

class PublicWddmTemperatureImp : public L0::Sysman::WddmTemperatureImp {
  public:
    PublicWddmTemperatureImp(L0::Sysman::OsSysman *pOsSysman) : L0::Sysman::WddmTemperatureImp(pOsSysman) {}
    using L0::Sysman::WddmTemperatureImp::type;
};

} // namespace ult
} // namespace Sysman
} // namespace L0

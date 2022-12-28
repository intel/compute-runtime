/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/performance/windows/os_performance_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace ult {

class PerformanceKmdSysManager : public Mock<MockKmdSysManager> {};

template <>
struct Mock<PerformanceKmdSysManager> : public PerformanceKmdSysManager {

    uint32_t mockNumberOfDomains = 1;
    uint32_t mockPerformanceFactor = 0;
    void getPerformanceProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        KmdSysman::ActivityDomainsType domain = static_cast<KmdSysman::ActivityDomainsType>(pRequest->inCommandParam);

        if (domain != KmdSysman::ActivityDomainsType::ActivityDomainMedia) {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            return;
        }

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Performance::NumPerformanceDomains: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockNumberOfDomains;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Performance::Factor: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockPerformanceFactor;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void setPerformanceProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pRequest);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderIn);
        KmdSysman::ActivityDomainsType domain = static_cast<KmdSysman::ActivityDomainsType>(pRequest->inCommandParam);

        if (domain != KmdSysman::ActivityDomainsType::ActivityDomainMedia) {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            return;
        }

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Performance::Factor: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            mockPerformanceFactor = *pValue;
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
    ~Mock() override = default;
};

class PublicWddmPerformanceImp : public L0::WddmPerformanceImp {
  public:
    PublicWddmPerformanceImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_engine_type_flag_t domain) : WddmPerformanceImp(pOsSysman, onSubdevice, subdeviceId, domain) {}
    using WddmPerformanceImp::domain;
    using WddmPerformanceImp::pKmdSysManager;
};

} // namespace ult
} // namespace L0

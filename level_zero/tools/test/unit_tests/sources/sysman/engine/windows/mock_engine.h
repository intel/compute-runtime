/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace ult {

class EngineKmdSysManager : public Mock<MockKmdSysManager> {};

template <>
struct Mock<EngineKmdSysManager> : public EngineKmdSysManager {

    KmdSysman::ActivityDomainsType mockEngineTypes[3] = {KmdSysman::ActivityDomainsType::ActitvityDomainGT, KmdSysman::ActivityDomainsType::ActivityDomainRenderCompute, KmdSysman::ActivityDomainsType::ActivityDomainMedia};
    uint64_t mockActivityCounters[3] = {652115546, 22115546, 4115546};
    uint64_t mockActivityTimeStamps[3] = {456465421541, 456465421545, 456465421548};
    uint32_t mockNumSupportedEngineGroups = 3;

    void getActivityProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        KmdSysman::ActivityDomainsType domain = static_cast<KmdSysman::ActivityDomainsType>(pRequest->inCommandParam);

        if (domain < KmdSysman::ActivityDomainsType::ActitvityDomainGT || domain >= KmdSysman::ActivityDomainsType::ActivityDomainMaxTypes) {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
            return;
        }

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Activity::NumActivityDomains: {
            uint32_t *pValueCounter = reinterpret_cast<uint32_t *>(pBuffer);
            *pValueCounter = mockNumSupportedEngineGroups;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Activity::CurrentActivityCounter: {
            uint64_t *pValueCounter = reinterpret_cast<uint64_t *>(pBuffer);
            uint64_t *pValueTimeStamp = reinterpret_cast<uint64_t *>(pBuffer + sizeof(uint64_t));
            *pValueCounter = mockActivityCounters[domain];
            *pValueTimeStamp = mockActivityTimeStamps[domain];
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = 2 * sizeof(uint64_t);
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void setActivityProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pRequest);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderIn);

        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }

    Mock() = default;
    ~Mock() = default;
};

} // namespace ult
} // namespace L0

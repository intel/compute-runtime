/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/test/unit_tests/sources/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockEngineKmdSysManager : public MockKmdSysManager {

    static const uint32_t mockEngineHandleComponentCount = 15u;
    KmdSysman::ActivityDomainsType mockEngineTypes[mockEngineHandleComponentCount] = {
        KmdSysman::ActivityDomainsType::ActitvityDomainGT,
        KmdSysman::ActivityDomainsType::ActivityDomainCompute,
        KmdSysman::ActivityDomainsType::ActivityDomainMedia,
        KmdSysman::ActivityDomainsType::ActivityDomainCopy,
        KmdSysman::ActivityDomainsType::ActivityDomainComputeSingle,
        KmdSysman::ActivityDomainsType::ActivityDomainRenderSingle,
        KmdSysman::ActivityDomainsType::ActivityDomainMediaDecodeSingle,
        KmdSysman::ActivityDomainsType::ActivityDomainMediaEncodeSingle,
        KmdSysman::ActivityDomainsType::ActivityDomainCopySingle,
        KmdSysman::ActivityDomainsType::ActivityDomainMediaEnhancementSingle,
        KmdSysman::ActivityDomainsType::ActivityDomain3dSingle,
        KmdSysman::ActivityDomainsType::ActivityDomain3dRenderCompute,
        KmdSysman::ActivityDomainsType::ActivityDomainRender,
        KmdSysman::ActivityDomainsType::ActivityDomain3d,
        KmdSysman::ActivityDomainsType::ActivityDomainMediaCodecSingle,
    };

    uint64_t mockActivityCounters[mockEngineHandleComponentCount] = {
        100000,  // ActitvityDomainGT
        200000,  // ActivityDomainCompute
        300000,  // ActivityDomainMedia
        400000,  // ActivityDomainCopy
        500000,  // ActivityDomainComputeSingle
        600000,  // ActivityDomainRenderSingle
        700000,  // ActivityDomainMediaDecodeSingle
        800000,  // ActivityDomainMediaEncodeSingle
        900000,  // ActivityDomainCopySingle
        1000000, // ActivityDomainMediaEnhancementSingle
        1100000, // ActivityDomain3dSingle
        1200000, // ActivityDomain3dRenderCompute
        1300000, // ActivityDomainRender
        1400000, // ActivityDomain3d
        1500000, // ActivityDomainMediaCodecSingle
    };
    uint64_t mockActivityTimeStamps[mockEngineHandleComponentCount] = {4465421, 2566851, 1226621, 1226622, 1226622, 1226622, 1226622, 1226622, 1226622, 1226622, 1226622, 1226622, 1226622, 1226622, 1226622};
    uint32_t mockNumSupportedEngineGroups = mockEngineHandleComponentCount;
    uint32_t mockFrequencyTimeStamp = 38400000;
    uint32_t mockFrequencyActivity = 1200000;
    bool useZeroResponseSizeForEngineInstancesPerGroup = false;

    void getActivityProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
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
        case KmdSysman::Requests::Activity::TimestampFrequency: {
            uint32_t *pValueFrequency = reinterpret_cast<uint32_t *>(pBuffer);
            *pValueFrequency = mockFrequencyTimeStamp;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Activity::ActivityCounterFrequency: {
            uint32_t *pValueFrequency = reinterpret_cast<uint32_t *>(pBuffer);
            *pValueFrequency = mockFrequencyActivity;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Activity::CurrentCounterV2: {
            uint64_t *pValueCounter = reinterpret_cast<uint64_t *>(pBuffer);
            uint64_t *pValueTimeStamp = reinterpret_cast<uint64_t *>(pBuffer + sizeof(uint64_t));
            memcpy_s(pValueCounter, sizeof(uint64_t), &mockActivityCounters[domain], sizeof(uint64_t));
            memcpy_s(pValueTimeStamp, sizeof(uint64_t), &mockActivityTimeStamps[domain], sizeof(uint64_t));
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = 2 * sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Activity::EngineInstancesPerGroup: {
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            if (!useZeroResponseSizeForEngineInstancesPerGroup) {
                pResponse->outDataSize = sizeof(uint32_t);
                uint32_t *numInstances = reinterpret_cast<uint32_t *>(pBuffer);
                if (mockNumSupportedEngineGroups == 0) {
                    *numInstances = 0;
                } else {
                    *numInstances = 1;
                }
            } else {
                pResponse->outDataSize = 0;
            }
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void setActivityProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0

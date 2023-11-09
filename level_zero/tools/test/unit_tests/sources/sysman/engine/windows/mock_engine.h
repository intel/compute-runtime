/*
 * Copyright (C) 2020-2023 Intel Corporation
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

    static const uint32_t mockEngineHandleComponentCount = 10u;
    uint32_t mockGetActivityExtReturnStatus = KmdSysman::KmdSysmanSuccess;
    KmdSysman::ActivityDomainsType mockEngineTypes[mockEngineHandleComponentCount] = {
        KmdSysman::ActivityDomainsType::ActitvityDomainGT,
        KmdSysman::ActivityDomainsType::ActivityDomainRenderCompute,
        KmdSysman::ActivityDomainsType::ActivityDomainMedia,
        KmdSysman::ActivityDomainsType::ActivityDomainCopy,
        KmdSysman::ActivityDomainsType::ActivityDomainComputeSingle,
        KmdSysman::ActivityDomainsType::ActivityDomainRenderSingle,
        KmdSysman::ActivityDomainsType::ActivityDomainMediaCodecSingle,
        KmdSysman::ActivityDomainsType::ActivityDomainMediaCodecSingle,
        KmdSysman::ActivityDomainsType::ActivityDomainMediaEnhancementSingle,
        KmdSysman::ActivityDomainsType::ActivityDomainCopySingle,
    };

    uint64_t mockActivityCounters[mockEngineHandleComponentCount] = {
        652411,
        222115,
        451115,
        451115,
        451115,
        451115,
        451115,
        451115,
        451115,
        451115,
    };
    uint64_t mockActivityTimeStamps[mockEngineHandleComponentCount] = {4465421, 2566851, 1226621, 1226622, 1226622, 1226622, 1226622, 1226622, 1226622, 1226622};
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
            memcpy_s(pValueCounter, sizeof(uint64_t), &mockActivityCounters[domain], sizeof(uint64_t));
            memcpy_s(pValueTimeStamp, sizeof(uint64_t), &mockActivityTimeStamps[domain], sizeof(uint64_t));
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

    Mock() = default;
    ~Mock() override = default;
};

} // namespace ult
} // namespace L0

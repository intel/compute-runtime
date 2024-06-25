/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/memory/sysman_memory_imp.h"
#include "level_zero/sysman/source/api/memory/windows/sysman_os_memory_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/windows/mock_memory_manager.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockMemoryManagerSysman : public MemoryManagerMock {
    MockMemoryManagerSysman(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

struct MockMemoryKmdSysManager : public MockKmdSysManager {

    uint32_t mockMemoryType = KmdSysman::MemoryType::GDDR6;
    uint32_t mockMemoryLocation = KmdSysman::MemoryLocationsType::DeviceMemory;
    uint64_t mockMemoryPhysicalSize = 4294967296;
    uint64_t mockMemoryStolen = 0;
    uint64_t mockMemorySystem = 17179869184;
    uint64_t mockMemoryDedicated = 0;
    uint64_t mockMemoryFree = 4294813696;
    uint32_t mockMemoryBus = 256;
    uint32_t mockMemoryChannels = 2;
    uint32_t mockMemoryMaxBandwidth = 4256000000;
    uint32_t mockMemoryCurrentBandwidthRead = 561321;
    uint32_t mockMemoryCurrentBandwidthWrite = 664521;
    uint32_t mockMemoryDomains = 1;
    uint32_t mockMemoryCurrentTotalAllocableMem = 4294813695;

    void getMemoryProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Memory::NumMemoryDomains: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryDomains;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::MemoryType: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryType;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::MemoryLocation: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryLocation;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::PhysicalSize: {
            uint64_t *pValue = reinterpret_cast<uint64_t *>(pBuffer);
            *pValue = mockMemoryPhysicalSize;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Memory::StolenSize: {
            uint64_t *pValue = reinterpret_cast<uint64_t *>(pBuffer);
            *pValue = mockMemoryStolen;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Memory::SystemSize: {
            uint64_t *pValue = reinterpret_cast<uint64_t *>(pBuffer);
            *pValue = mockMemorySystem;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Memory::DedicatedSize: {
            uint64_t *pValue = reinterpret_cast<uint64_t *>(pBuffer);
            *pValue = mockMemoryDedicated;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Memory::CurrentFreeMemorySize: {
            uint64_t *pValue = reinterpret_cast<uint64_t *>(pBuffer);
            *pValue = mockMemoryFree;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Memory::MemoryWidth: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryBus;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::NumChannels: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryChannels;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::MaxBandwidth: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryMaxBandwidth;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::CurrentBandwidthRead: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryCurrentBandwidthRead;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::CurrentBandwidthWrite: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryCurrentBandwidthWrite;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::CurrentTotalAllocableMem: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryCurrentTotalAllocableMem;
            pResponse->outReturnCode = KmdSysman::KmdSysmanSuccess;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void setMemoryProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }
};

class PublicWddmPowerImp : public L0::Sysman::WddmMemoryImp {
  public:
    using WddmMemoryImp::pKmdSysManager;
};

} // namespace ult
} // namespace Sysman
} // namespace L0

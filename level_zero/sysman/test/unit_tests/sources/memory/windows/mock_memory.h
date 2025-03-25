/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/sysman/source/api/memory/sysman_memory_imp.h"
#include "level_zero/sysman/source/api/memory/windows/sysman_os_memory_imp.h"
#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/windows/mock_memory_manager.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_kmd_sys_manager.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t memoryHandleComponentCount = 1u;
constexpr uint32_t mockMemoryMaxBandwidth = 2;
constexpr uint32_t mockMemoryCurrentBandwidthRead = 3840;
constexpr uint32_t mockMemoryCurrentBandwidthWrite = 2560;
constexpr uint32_t mockMemoryBandwidthTimestamp = 1230000;

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
    uint32_t mockMemoryFailure[KmdSysman::Requests::Memory::MaxMemoryRequests] = {0};

    uint32_t getReturnCode(uint32_t memoryRequestCode) {
        return mockMemoryFailure[memoryRequestCode] ? KmdSysman::KmdSysmanFail : KmdSysman::KmdSysmanSuccess;
    }

    void getMemoryProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Memory::NumMemoryDomains: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryDomains;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::MemoryType: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryType;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::MemoryLocation: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryLocation;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::PhysicalSize: {
            uint64_t *pValue = reinterpret_cast<uint64_t *>(pBuffer);
            *pValue = mockMemoryPhysicalSize;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Memory::StolenSize: {
            uint64_t *pValue = reinterpret_cast<uint64_t *>(pBuffer);
            *pValue = mockMemoryStolen;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Memory::SystemSize: {
            uint64_t *pValue = reinterpret_cast<uint64_t *>(pBuffer);
            *pValue = mockMemorySystem;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Memory::DedicatedSize: {
            uint64_t *pValue = reinterpret_cast<uint64_t *>(pBuffer);
            *pValue = mockMemoryDedicated;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Memory::CurrentFreeMemorySize: {
            uint64_t *pValue = reinterpret_cast<uint64_t *>(pBuffer);
            *pValue = mockMemoryFree;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint64_t);
        } break;
        case KmdSysman::Requests::Memory::MemoryWidth: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryBus;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::NumChannels: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryChannels;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::MaxBandwidth: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryMaxBandwidth;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::CurrentBandwidthRead: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryCurrentBandwidthRead;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::CurrentBandwidthWrite: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryCurrentBandwidthWrite;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Memory::CurrentTotalAllocableMem: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMemoryCurrentTotalAllocableMem;
            pResponse->outReturnCode = getReturnCode(pRequest->inRequestId);
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

class SysmanDeviceMemoryFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockMemoryKmdSysManager> pKmdSysManager;
    L0::Sysman::KmdSysManager *pOriginalKmdSysManager = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pKmdSysManager.reset(new MockMemoryKmdSysManager);

        pKmdSysManager->allowSetCalls = true;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();

        getMemoryHandles(0);
    }

    void TearDown() override {
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    void setLocalSupportedAndReinit(bool supported) {
        pMemoryManager->localMemorySupported[0] = supported;
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pSysmanDeviceImp->pMemoryHandleContext->init(pOsSysman->getSubDeviceCount());
    }

    void clearMemHandleListAndReinit() {
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pSysmanDeviceImp->pMemoryHandleContext->init(pOsSysman->getSubDeviceCount());
    }

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    MockMemoryManagerSysman *pMemoryManager = nullptr;
};

class PublicWddmPowerImp : public L0::Sysman::WddmMemoryImp {
  public:
    using WddmMemoryImp::pKmdSysManager;
};

class PublicPlatformMonitoringTech : public L0::Sysman::PlatformMonitoringTech {
  public:
    PublicPlatformMonitoringTech(std::vector<wchar_t> deviceInterfaceList, SysmanProductHelper *pSysmanProductHelper) : PlatformMonitoringTech(deviceInterfaceList, pSysmanProductHelper) {}
    using PlatformMonitoringTech::keyOffsetMap;
};

struct MockSysmanProductHelperMemory : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
    MockSysmanProductHelperMemory() = default;
    ADDMETHOD_NOBASE(getMemoryBandWidth, ze_result_t, ZE_RESULT_SUCCESS, (zes_mem_bandwidth_t * pBandwidth, WddmSysmanImp *pWddmSysmanImp));
};

} // namespace ult
} // namespace Sysman
} // namespace L0

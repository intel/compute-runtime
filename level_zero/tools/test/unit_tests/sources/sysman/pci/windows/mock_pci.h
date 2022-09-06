/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/pci/windows/os_pci_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_kmd_sys_manager.h"

#include "sysman/pci/pci_imp.h"

namespace L0 {
namespace ult {

struct MockMemoryManagerSysman : public MemoryManagerMock {
    MockMemoryManagerSysman(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
};

class PciKmdSysManager : public Mock<MockKmdSysManager> {};

template <>
struct Mock<PciKmdSysManager> : public PciKmdSysManager {
    //PciCurrentDevice, PciParentDevice, PciRootPort
    uint32_t mockDomain[3] = {0, 0, 0};
    uint32_t mockBus[3] = {0, 0, 3};
    uint32_t mockDevice[3] = {2, 0, 0};
    uint32_t mockFunction[3] = {0, 0, 0};
    uint32_t mockMaxLinkSpeed[3] = {1, 0, 4};
    uint32_t mockMaxLinkWidth[3] = {1, 0, 8};
    uint32_t mockCurrentLinkSpeed[3] = {1, 0, 3};
    uint32_t mockCurrentLinkWidth[3] = {1, 0, 1};
    uint32_t mockResizableBarSupported[3] = {1, 1, 1};
    uint32_t mockResizableBarEnabled[3] = {1, 1, 1};
    uint32_t pciBusReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciDomainReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciDeviceReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciFunctionReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciMaxLinkSpeedReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciMaxLinkWidthReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciCurrentLinkSpeedReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciCurrentLinkWidthReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciResizableBarSupportedReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciResizableBarEnabledReturnCode = KmdSysman::KmdSysmanSuccess;

    void getPciProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        KmdSysman::PciDomainsType domain = static_cast<KmdSysman::PciDomainsType>(pRequest->inCommandParam);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Pci::Domain: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockDomain[domain];
            pResponse->outReturnCode = pciDomainReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::Bus: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockBus[domain];
            pResponse->outReturnCode = pciBusReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::Device: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockDevice[domain];
            pResponse->outReturnCode = pciDeviceReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::Function: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockFunction[domain];
            pResponse->outReturnCode = pciFunctionReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::MaxLinkSpeed: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxLinkSpeed[domain];
            pResponse->outReturnCode = pciMaxLinkSpeedReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::MaxLinkWidth: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxLinkWidth[domain];
            pResponse->outReturnCode = pciMaxLinkWidthReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::CurrentLinkSpeed: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockCurrentLinkSpeed[domain];
            pResponse->outReturnCode = pciCurrentLinkSpeedReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::CurrentLinkWidth: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockCurrentLinkWidth[domain];
            pResponse->outReturnCode = pciCurrentLinkWidthReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::ResizableBarSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockResizableBarSupported[domain];
            pResponse->outReturnCode = pciResizableBarSupportedReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::ResizableBarEnabled: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockResizableBarEnabled[domain];
            pResponse->outReturnCode = pciResizableBarEnabledReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void setPciProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
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

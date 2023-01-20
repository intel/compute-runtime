/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/pci/windows/os_pci_imp.h"

#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/driver/driver_handle.h"

namespace L0 {

ze_result_t WddmPciImp::getProperties(zes_pci_properties_t *properties) {
    properties->haveBandwidthCounters = false;
    properties->havePacketCounters = false;
    properties->haveReplayCounters = false;
    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmPciImp::getPciBdf(zes_pci_properties_t &pciProperties) {
    uint32_t valueSmall = 0;
    uint32_t domain = 0, bus = 0, dev = 0, func = 0;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PciComponent;
    request.paramInfo = (isLmemSupported) ? KmdSysman::PciDomainsType::PciRootPort : KmdSysman::PciDomainsType::PciCurrentDevice;

    request.requestId = KmdSysman::Requests::Pci::Bus;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Pci::Domain;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Pci::Device;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Pci::Function;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&valueSmall, sizeof(uint32_t), vResponses[0].dataBuffer, sizeof(uint32_t));
        bus = valueSmall;
    }

    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&valueSmall, sizeof(uint32_t), vResponses[1].dataBuffer, sizeof(uint32_t));
        domain = valueSmall;
    }

    if (vResponses[2].returnCode == KmdSysman::Success) {
        memcpy_s(&valueSmall, sizeof(uint32_t), vResponses[2].dataBuffer, sizeof(uint32_t));
        dev = valueSmall;
    }

    if (vResponses[3].returnCode == KmdSysman::Success) {
        memcpy_s(&valueSmall, sizeof(uint32_t), vResponses[3].dataBuffer, sizeof(uint32_t));
        func = valueSmall;
    }

    pciProperties.address.domain = domain;
    pciProperties.address.bus = bus;
    pciProperties.address.device = dev;
    pciProperties.address.function = func;

    return ZE_RESULT_SUCCESS;
}

void WddmPciImp::getMaxLinkCaps(double &maxLinkSpeed, int32_t &maxLinkWidth) {
    uint32_t valueSmall = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PciComponent;
    request.requestId = KmdSysman::Requests::Pci::MaxLinkSpeed;
    request.paramInfo = (isLmemSupported) ? KmdSysman::PciDomainsType::PciRootPort : KmdSysman::PciDomainsType::PciCurrentDevice;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        maxLinkSpeed = 0;
    } else {
        memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        maxLinkSpeed = convertPciGenToLinkSpeed(valueSmall);
    }

    request.requestId = KmdSysman::Requests::Pci::MaxLinkWidth;
    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        maxLinkWidth = -1;
        return;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    maxLinkWidth = static_cast<int32_t>(valueSmall);

    return;
}

ze_result_t WddmPciImp::getState(zes_pci_state_t *state) {
    uint32_t valueSmall = 0;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    state->qualityIssues = ZES_PCI_LINK_QUAL_ISSUE_FLAG_FORCE_UINT32;
    state->stabilityIssues = ZES_PCI_LINK_STAB_ISSUE_FLAG_FORCE_UINT32;
    state->status = ZES_PCI_LINK_STATUS_FORCE_UINT32;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PciComponent;
    request.paramInfo = (isLmemSupported) ? KmdSysman::PciDomainsType::PciRootPort : KmdSysman::PciDomainsType::PciCurrentDevice;

    request.requestId = KmdSysman::Requests::Pci::CurrentLinkSpeed;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Pci::CurrentLinkWidth;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&valueSmall, sizeof(uint32_t), vResponses[0].dataBuffer, sizeof(uint32_t));
        state->speed.gen = static_cast<int32_t>(valueSmall);
    }

    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&valueSmall, sizeof(uint32_t), vResponses[1].dataBuffer, sizeof(uint32_t));
        state->speed.width = static_cast<int32_t>(valueSmall);
    }

    return ZE_RESULT_SUCCESS;
}

bool WddmPciImp::resizableBarSupported() {
    uint32_t valueSmall = 0;
    bool supported = false;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PciComponent;
    request.paramInfo = KmdSysman::PciDomainsType::PciCurrentDevice;
    request.requestId = KmdSysman::Requests::Pci::ResizableBarSupported;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        supported = static_cast<bool>(valueSmall);
    }

    return supported;
}

bool WddmPciImp::resizableBarEnabled(uint32_t barIndex) {
    uint32_t valueSmall = 0;
    bool enabled = false;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PciComponent;
    request.paramInfo = KmdSysman::PciDomainsType::PciCurrentDevice;
    request.requestId = KmdSysman::Requests::Pci::ResizableBarEnabled;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        enabled = static_cast<bool>(valueSmall);
    }

    return enabled;
}

ze_result_t WddmPciImp::initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) {
    zes_pci_bar_properties_t *pBarProp = new zes_pci_bar_properties_t;
    memset(pBarProp, 0, sizeof(zes_pci_bar_properties_t));
    pBarProperties.push_back(pBarProp);
    return ZE_RESULT_SUCCESS;
}

WddmPciImp::WddmPciImp(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
    Device *pDevice = pWddmSysmanImp->getDeviceHandle();
    isLmemSupported = pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
}

OsPci *OsPci::create(OsSysman *pOsSysman) {
    WddmPciImp *pWddmPciImp = new WddmPciImp(pOsSysman);
    return static_cast<OsPci *>(pWddmPciImp);
}

} // namespace L0

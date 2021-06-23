/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/pci/windows/os_pci_imp.h"

namespace L0 {

ze_result_t WddmPciImp::getProperties(zes_pci_properties_t *properties) {
    properties->haveBandwidthCounters = false;
    properties->havePacketCounters = false;
    properties->haveReplayCounters = false;
    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmPciImp::getPciBdf(std::string &bdf) {
    uint32_t valueSmall = 0;
    uint32_t domain = 0, bus = 0, dev = 0, func = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PciComponent;
    request.requestId = KmdSysman::Requests::Pci::Bus;

    if (isLmemSupported) {
        request.paramInfo = KmdSysman::PciDomainsType::PciRootPort;
    } else {
        request.paramInfo = KmdSysman::PciDomainsType::PciCurrentDevice;
    }

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    bus = valueSmall;

    request.requestId = KmdSysman::Requests::Pci::Domain;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    domain = valueSmall;

    request.requestId = KmdSysman::Requests::Pci::Device;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    dev = valueSmall;

    request.requestId = KmdSysman::Requests::Pci::Function;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    func = valueSmall;

    bdf = std::to_string(domain) + std::string(":") + std::to_string(bus) + std::string(":") + std::to_string(dev) + std::string(".") + std::to_string(func);

    return status;
}

ze_result_t WddmPciImp::getMaxLinkSpeed(double &maxLinkSpeed) {
    uint32_t valueSmall = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PciComponent;
    request.requestId = KmdSysman::Requests::Pci::MaxLinkSpeed;

    if (isLmemSupported) {
        request.paramInfo = KmdSysman::PciDomainsType::PciRootPort;
    } else {
        request.paramInfo = KmdSysman::PciDomainsType::PciCurrentDevice;
    }

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    maxLinkSpeed = convertPciGenToLinkSpeed(valueSmall);

    return status;
}

ze_result_t WddmPciImp::getMaxLinkWidth(int32_t &maxLinkwidth) {
    uint32_t valueSmall = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PciComponent;
    request.requestId = KmdSysman::Requests::Pci::MaxLinkWidth;

    if (isLmemSupported) {
        request.paramInfo = KmdSysman::PciDomainsType::PciRootPort;
    } else {
        request.paramInfo = KmdSysman::PciDomainsType::PciCurrentDevice;
    }

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    maxLinkwidth = static_cast<int32_t>(valueSmall);

    return status;
}

ze_result_t WddmPciImp::getState(zes_pci_state_t *state) {
    uint32_t valueSmall = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    state->qualityIssues = ZES_PCI_LINK_QUAL_ISSUE_FLAG_FORCE_UINT32;
    state->stabilityIssues = ZES_PCI_LINK_STAB_ISSUE_FLAG_FORCE_UINT32;
    state->status = ZES_PCI_LINK_STATUS_FORCE_UINT32;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PciComponent;
    request.requestId = KmdSysman::Requests::Pci::CurrentLinkSpeed;

    if (isLmemSupported) {
        request.paramInfo = KmdSysman::PciDomainsType::PciRootPort;
    } else {
        request.paramInfo = KmdSysman::PciDomainsType::PciCurrentDevice;
    }

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status == ZE_RESULT_SUCCESS) {
        memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        state->speed.gen = static_cast<int32_t>(valueSmall);
    }

    request.requestId = KmdSysman::Requests::Pci::CurrentLinkWidth;

    status = pKmdSysManager->requestSingle(request, response);

    if (status == ZE_RESULT_SUCCESS) {
        memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        state->speed.width = static_cast<int32_t>(valueSmall);
    }

    return status;
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

bool WddmPciImp::resizableBarEnabled() {
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

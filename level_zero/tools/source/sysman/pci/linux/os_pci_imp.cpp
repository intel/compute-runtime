/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/pci/linux/os_pci_imp.h"

#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

#include "sysman/pci/pci_imp.h"

#include <linux/pci_regs.h>

namespace L0 {

const std::string LinuxPciImp::deviceDir("device");
const std::string LinuxPciImp::resourceFile("device/resource");
const std::string LinuxPciImp::maxLinkSpeedFile("device/max_link_speed");
const std::string LinuxPciImp::maxLinkWidthFile("device/max_link_width");

ze_result_t LinuxPciImp::getProperties(zes_pci_properties_t *properties) {
    properties->haveBandwidthCounters = false;
    properties->havePacketCounters = false;
    properties->haveReplayCounters = false;
    return ZE_RESULT_SUCCESS;
}
ze_result_t LinuxPciImp::getPciBdf(std::string &bdf) {
    std::string bdfDir;
    ze_result_t result = pSysfsAccess->readSymLink(deviceDir, bdfDir);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    const auto loc = bdfDir.find_last_of('/');
    bdf = bdfDir.substr(loc + 1);
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPciImp::getMaxLinkSpeed(double &maxLinkSpeed) {
    ze_result_t result;
    if (isLmemSupported) {
        std::string rootPortPath;
        std::string realRootPath;
        result = pSysfsAccess->getRealPath(deviceDir, realRootPath);
        if (ZE_RESULT_SUCCESS != result) {
            maxLinkSpeed = 0;
            return result;
        }

        // we need to get actual values of speed and width at the Discrete card's root port.
        rootPortPath = pLinuxSysmanImp->getPciRootPortDirectoryPath(realRootPath);

        result = pfsAccess->read(rootPortPath + '/' + "max_link_speed", maxLinkSpeed);
        if (ZE_RESULT_SUCCESS != result) {
            maxLinkSpeed = 0;
            return result;
        }
    } else {
        result = pSysfsAccess->read(maxLinkSpeedFile, maxLinkSpeed);
        if (ZE_RESULT_SUCCESS != result) {
            maxLinkSpeed = 0;
            return result;
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPciImp::getMaxLinkWidth(int32_t &maxLinkwidth) {
    ze_result_t result;
    if (isLmemSupported) {
        std::string rootPortPath;
        std::string realRootPath;
        result = pSysfsAccess->getRealPath(deviceDir, realRootPath);
        if (ZE_RESULT_SUCCESS != result) {
            maxLinkwidth = -1;
            return result;
        }

        // we need to get actual values of speed and width at the Discrete card's root port.
        rootPortPath = pLinuxSysmanImp->getPciRootPortDirectoryPath(realRootPath);

        result = pfsAccess->read(rootPortPath + '/' + "max_link_width", maxLinkwidth);
        if (ZE_RESULT_SUCCESS != result) {
            maxLinkwidth = -1;
            return result;
        }
        if (maxLinkwidth == static_cast<int32_t>(unknownPcieLinkWidth)) {
            maxLinkwidth = -1;
        }
    } else {
        result = pSysfsAccess->read(maxLinkWidthFile, maxLinkwidth);
        if (ZE_RESULT_SUCCESS != result) {
            return result;
        }
        if (maxLinkwidth == static_cast<int32_t>(unknownPcieLinkWidth)) {
            maxLinkwidth = -1;
        }
    }
    return ZE_RESULT_SUCCESS;
}

void getBarBaseAndSize(std::string readBytes, uint64_t &baseAddr, uint64_t &barSize, uint64_t &barFlags) {

    unsigned long long start, end, flags;
    std::stringstream sStreamReadBytes;
    sStreamReadBytes << readBytes;
    sStreamReadBytes >> std::hex >> start;
    sStreamReadBytes >> end;
    sStreamReadBytes >> flags;

    flags &= 0xf;
    barFlags = flags;
    baseAddr = start;
    barSize = end - start + 1;
}

ze_result_t LinuxPciImp::initializeBarProperties(std::vector<zes_pci_bar_properties_t *> &pBarProperties) {
    std::vector<std::string> ReadBytes;
    ze_result_t result = pSysfsAccess->read(resourceFile, ReadBytes);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    for (uint32_t i = 0; i <= maxPciBars; i++) {
        uint64_t baseAddr, barSize, barFlags;
        getBarBaseAndSize(ReadBytes[i], baseAddr, barSize, barFlags);
        if (baseAddr && !(barFlags & 0x1)) { // we do not update for I/O ports
            zes_pci_bar_properties_t *pBarProp = new zes_pci_bar_properties_t;
            memset(pBarProp, 0, sizeof(zes_pci_bar_properties_t));
            pBarProp->index = i;
            pBarProp->base = baseAddr;
            pBarProp->size = barSize;
            // Bar Flags Desc.
            // Bit-0 - Value 0x0 -> MMIO type BAR
            // Bit-0 - Value 0x1 -> I/O type BAR
            if (i == 0) { // GRaphics MMIO is at BAR0, and is a 64-bit
                pBarProp->type = ZES_PCI_BAR_TYPE_MMIO;
            }
            if (i == 2) {
                pBarProp->type = ZES_PCI_BAR_TYPE_MEM; // device memory is always at BAR2
            }
            if (i == 6) { // the 7th entry of resource file is expected to be ROM BAR
                pBarProp->type = ZES_PCI_BAR_TYPE_ROM;
            }
            pBarProperties.push_back(pBarProp);
        }
    }
    if (pBarProperties.size() == 0) {
        result = ZE_RESULT_ERROR_UNKNOWN;
    }
    return result;
}

// Parse PCIe configuration space to see if resizable Bar is supported
bool LinuxPciImp::resizableBarSupported() {
    uint32_t pos = PCI_CFG_SPACE_SIZE;
    uint32_t header = 0;

    if (!configMemory) {
        return false;
    }

    // Minimum 8 bytes per capability. Hence maximum capabilities that
    // could be present in PCI extended configuration space are
    // represented by loopCount.
    auto loopCount = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;
    header = getDwordFromConfig(pos);
    if (!header) {
        return false;
    }

    while (loopCount-- > 0) {
        if (PCI_EXT_CAP_ID(header) == PCI_EXT_CAP_ID_REBAR) {
            return true;
        }
        pos = PCI_EXT_CAP_NEXT(header);
        if (pos < PCI_CFG_SPACE_SIZE) {
            return false;
        }
        header = getDwordFromConfig(pos);
    }
    return false;
}

bool LinuxPciImp::resizableBarEnabled() {
    return false;
}

ze_result_t LinuxPciImp::getState(zes_pci_state_t *state) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void LinuxPciImp::pciExtendedConfigRead() {
    std::string pciConfigNode;
    pSysfsAccess->getRealPath("device/config", pciConfigNode);
    int fdConfig = -1;
    fdConfig = this->openFunction(pciConfigNode.c_str(), O_RDONLY);
    if (fdConfig < 0) {
        return;
    }
    configMemory = std::make_unique<uint8_t[]>(PCI_CFG_SPACE_EXP_SIZE);
    memset(configMemory.get(), 0, PCI_CFG_SPACE_EXP_SIZE);
    this->preadFunction(fdConfig, configMemory.get(), PCI_CFG_SPACE_EXP_SIZE, 0);
    this->closeFunction(fdConfig);
}

LinuxPciImp::LinuxPciImp(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pfsAccess = &pLinuxSysmanImp->getFsAccess();
    Device *pDevice = pLinuxSysmanImp->getDeviceHandle();
    isLmemSupported = pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
    if (pSysfsAccess->isRootUser()) {
        pciExtendedConfigRead();
    }
}

OsPci *OsPci::create(OsSysman *pOsSysman) {
    LinuxPciImp *pLinuxPciImp = new LinuxPciImp(pOsSysman);
    return static_cast<OsPci *>(pLinuxPciImp);
}

} // namespace L0

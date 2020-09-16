/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/pci/linux/os_pci_imp.h"

#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

#include "sysman/linux/os_sysman_imp.h"
#include "sysman/pci/pci_imp.h"

namespace L0 {

const std::string LinuxPciImp::deviceDir("device");
const std::string LinuxPciImp::resourceFile("device/resource");
const std::string LinuxPciImp::maxLinkSpeedFile("device/max_link_speed");
const std::string LinuxPciImp::maxLinkWidthFile("device/max_link_width");

std::string LinuxPciImp::changeDirNLevelsUp(std::string realRootPath, uint8_t nLevel) {
    size_t loc;
    while (nLevel > 0) {
        loc = realRootPath.find_last_of('/');
        realRootPath = realRootPath.substr(0, loc);
        nLevel--;
    }
    return realRootPath;
}
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
        // we need to change the absolute path to two levels up to get actual
        // values of speed and width at the Discrete card's root port.
        // the root port is always at a fixed distance as defined in HW
        rootPortPath = changeDirNLevelsUp(realRootPath, 2);
        if (ZE_RESULT_SUCCESS != result) {
            maxLinkSpeed = 0;
            return result;
        }
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
        // we need to change the absolute path to two levels up to get actual
        // values of speed and width at the Discrete card's root port.
        // the root port is always at a fixed distance as defined in HW
        rootPortPath = changeDirNLevelsUp(realRootPath, 2);
        if (ZE_RESULT_SUCCESS != result) {
            maxLinkwidth = -1;
            return result;
        }
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

ze_result_t LinuxPciImp::getState(zes_pci_state_t *state) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
LinuxPciImp::LinuxPciImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pfsAccess = &pLinuxSysmanImp->getFsAccess();
    Device *pDevice = pLinuxSysmanImp->getDeviceHandle();
    isLmemSupported = pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
}

OsPci *OsPci::create(OsSysman *pOsSysman) {
    LinuxPciImp *pLinuxPciImp = new LinuxPciImp(pOsSysman);
    return static_cast<OsPci *>(pLinuxPciImp);
}

} // namespace L0

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/pci/linux/os_pci_imp.h"

#include "shared/source/utilities/directory.h"

#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

#include "sysman/pci/pci_imp.h"

#include <linux/pci_regs.h>

namespace L0 {

const std::string LinuxPciImp::deviceDir("device");
const std::string LinuxPciImp::resourceFile("device/resource");

ze_result_t LinuxPciImp::getProperties(zes_pci_properties_t *properties) {
    properties->haveBandwidthCounters = false;
    properties->havePacketCounters = false;
    properties->haveReplayCounters = false;
    return ZE_RESULT_SUCCESS;
}
ze_result_t LinuxPciImp::getPciBdf(zes_pci_properties_t &pciProperties) {
    std::string bdfDir;
    ze_result_t result = pSysfsAccess->readSymLink(deviceDir, bdfDir);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    const auto loc = bdfDir.find_last_of('/');
    std::string bdf = bdfDir.substr(loc + 1);
    uint16_t domain = 0;
    uint8_t bus = 0, device = 0, function = 0;
    NEO::parseBdfString(bdf.c_str(), domain, bus, device, function);
    pciProperties.address.domain = static_cast<uint32_t>(domain);
    pciProperties.address.bus = static_cast<uint32_t>(bus);
    pciProperties.address.device = static_cast<uint32_t>(device);
    pciProperties.address.function = static_cast<uint32_t>(function);
    return ZE_RESULT_SUCCESS;
}

void LinuxPciImp::getMaxLinkCaps(double &maxLinkSpeed, int32_t &maxLinkWidth) {
    uint16_t linkCaps = 0;
    auto linkCapPos = getLinkCapabilityPos();
    if (!linkCapPos) {
        maxLinkSpeed = 0;
        maxLinkWidth = -1;
        return;
    }

    linkCaps = getWordFromConfig(linkCapPos, isLmemSupported ? uspConfigMemory.get() : configMemory.get());
    maxLinkSpeed = convertPciGenToLinkSpeed(BITS(linkCaps, 0, 4));
    maxLinkWidth = BITS(linkCaps, 4, 6);

    return;
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
    std::vector<std::string> readBytes;
    ze_result_t result = pSysfsAccess->read(resourceFile, readBytes);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    for (uint32_t i = 0; i <= maxPciBars; i++) {
        uint64_t baseAddr, barSize, barFlags;
        getBarBaseAndSize(readBytes[i], baseAddr, barSize, barFlags);
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

uint32_t LinuxPciImp::getRebarCapabilityPos() {
    uint32_t pos = PCI_CFG_SPACE_SIZE;
    uint32_t header = 0;

    if (!configMemory) {
        return 0;
    }

    // Minimum 8 bytes per capability. Hence maximum capabilities that
    // could be present in PCI extended configuration space are
    // represented by loopCount.
    auto loopCount = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;
    header = getDwordFromConfig(pos);
    if (!header) {
        return 0;
    }

    while (loopCount-- > 0) {
        if (PCI_EXT_CAP_ID(header) == PCI_EXT_CAP_ID_REBAR) {
            return pos;
        }
        pos = PCI_EXT_CAP_NEXT(header);
        if (pos < PCI_CFG_SPACE_SIZE) {
            return 0;
        }
        header = getDwordFromConfig(pos);
    }
    return 0;
}

uint16_t LinuxPciImp::getLinkCapabilityPos() {
    uint16_t pos = PCI_CAPABILITY_LIST;
    uint8_t id, type = 0;
    uint16_t capRegister = 0;
    uint8_t *configMem = isLmemSupported ? uspConfigMemory.get() : configMemory.get();

    if ((!configMem) || (!(getByteFromConfig(PCI_STATUS, configMem) & PCI_STATUS_CAP_LIST))) {
        return 0;
    }

    // Minimum 8 bytes per capability. Hence maximum capabilities that
    // could be present in PCI configuration space are
    // represented by loopCount.
    auto loopCount = (PCI_CFG_SPACE_SIZE) / 8;

    /* Bottom two bits of capability pointer register are reserved and
     * software should mask these bits to get pointer to capability list.
     */
    pos = getByteFromConfig(pos, configMem) & 0xfc;
    while (loopCount-- > 0) {
        id = getByteFromConfig(pos + PCI_CAP_LIST_ID, configMem);
        if (id == PCI_CAP_ID_EXP) {
            capRegister = getWordFromConfig(pos + PCI_CAP_FLAGS, configMem);
            type = BITS(capRegister, 4, 4);
            /* Root Complex Integrated end point and
             * Root Complex Event collector will not implement link capabilities
             */
            if ((type != PCI_EXP_TYPE_RC_END) && (type != PCI_EXP_TYPE_RC_EC)) {
                return pos + PCI_EXP_LNKCAP;
            }
        }

        pos = getByteFromConfig(pos + PCI_CAP_LIST_NEXT, configMem) & 0xfc;
        if (!pos) {
            break;
        }
    }
    return 0;
}

// Parse PCIe configuration space to see if resizable Bar is supported
bool LinuxPciImp::resizableBarSupported() {
    return (getRebarCapabilityPos() > 0);
}

bool LinuxPciImp::resizableBarEnabled(uint32_t barIndex) {
    bool isBarResizable = false;
    uint32_t capabilityRegister = 0, controlRegister = 0;
    uint32_t nBars = 1;
    auto rebarCapabilityPos = getRebarCapabilityPos();

    // If resizable Bar is not supported then return false.
    if (!rebarCapabilityPos) {
        return false;
    }

    // As per PCI spec, resizable BAR's capability structure's 52 byte length could be represented as:
    // --------------------------------------------------------------
    // | byte offset    |   Description of register                 |
    // -------------------------------------------------------------|
    // | +000h          |   PCI Express Extended Capability Header  |
    // -------------------------------------------------------------|
    // | +004h          |   Resizable BAR Capability Register (0)   |
    // -------------------------------------------------------------|
    // | +008h          |   Resizable BAR Control Register (0)      |
    // -------------------------------------------------------------|
    // | +00Ch          |   Resizable BAR Capability Register (1)   |
    // -------------------------------------------------------------|
    // | +010h          |   Resizable BAR Control Register (1)      |
    // -------------------------------------------------------------|
    // | +014h          |   ---                                     |
    // -------------------------------------------------------------|

    // Only first Control register(at offset 008h, as shown above), could tell about number of resizable Bars
    controlRegister = getDwordFromConfig(rebarCapabilityPos + PCI_REBAR_CTRL);
    nBars = BITS(controlRegister, 5, 3); // control register's bits 5,6 and 7 contain number of resizable bars information
    for (auto barNumber = 0u; barNumber < nBars; barNumber++) {
        uint32_t barId = 0;
        controlRegister = getDwordFromConfig(rebarCapabilityPos + PCI_REBAR_CTRL);
        barId = BITS(controlRegister, 0, 3); // Control register's bit 0,1,2 tells the index of bar
        if (barId == barIndex) {
            isBarResizable = true;
            break;
        }
        rebarCapabilityPos += 8;
    }

    if (isBarResizable == false) {
        return false;
    }

    capabilityRegister = getDwordFromConfig(rebarCapabilityPos + PCI_REBAR_CAP);
    // Capability register's bit 4 to 31 indicates supported Bar sizes.
    // In possibleBarSizes, position of each set bit indicates supported bar size. Example,  if set bit
    // position of possibleBarSizes is from 0 to n, then this indicates BAR size from 2^0 MB to 2^n MB
    auto possibleBarSizes = (capabilityRegister & PCI_REBAR_CAP_SIZES) >> 4; // First 4 bits are reserved
    uint32_t largestPossibleBarSize = 0;
    while (possibleBarSizes >>= 1) { // most significant set bit position of possibleBarSizes would tell larget possible bar size
        largestPossibleBarSize++;
    }

    // Control register's bit 8 to 13 indicates current BAR size in encoded form.
    // Example, real value of current size could be 2^currentSize MB
    auto currentSize = BITS(controlRegister, 8, 6);

    // If current size is equal to larget possible BAR size, it indicates resizable BAR is enabled.
    return (currentSize == largestPossibleBarSize);
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

void LinuxPciImp::pciCardBusConfigRead() {
    std::string pciConfigNode;
    std::string rootPortPath;
    pSysfsAccess->getRealPath(deviceDir, pciConfigNode);
    rootPortPath = pLinuxSysmanImp->getPciCardBusDirectoryPath(pciConfigNode);
    pciConfigNode = rootPortPath + "/config";
    int fdConfig = -1;
    fdConfig = this->openFunction(pciConfigNode.c_str(), O_RDONLY);
    if (fdConfig < 0) {
        return;
    }
    uspConfigMemory = std::make_unique<uint8_t[]>(PCI_CFG_SPACE_SIZE);
    memset(uspConfigMemory.get(), 0, PCI_CFG_SPACE_SIZE);
    this->preadFunction(fdConfig, uspConfigMemory.get(), PCI_CFG_SPACE_SIZE, 0);
    this->closeFunction(fdConfig);
}

LinuxPciImp::LinuxPciImp(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    Device *pDevice = pLinuxSysmanImp->getDeviceHandle();
    isLmemSupported = pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
    if (pSysfsAccess->isRootUser()) {
        pciExtendedConfigRead();
        pciCardBusConfigRead();
    }
}

OsPci *OsPci::create(OsSysman *pOsSysman) {
    LinuxPciImp *pLinuxPciImp = new LinuxPciImp(pOsSysman);
    return static_cast<OsPci *>(pLinuxPciImp);
}

} // namespace L0

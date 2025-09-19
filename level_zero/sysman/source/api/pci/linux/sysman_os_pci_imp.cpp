/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/pci/linux/sysman_os_pci_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/file_descriptor.h"
#include "shared/source/utilities/directory.h"

#include "level_zero/sysman/source/api/pci/sysman_pci_imp.h"
#include "level_zero/sysman/source/api/pci/sysman_pci_utils.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

#include <sstream>

namespace L0 {
namespace Sysman {

const std::string LinuxPciImp::deviceDir("device");
const std::string LinuxPciImp::resourceFile("device/resource");

ze_result_t LinuxPciImp::getProperties(zes_pci_properties_t *pProperties) {
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();

    void *pNext = pProperties->pNext;
    while (pNext) {
        auto pExtProps = reinterpret_cast<zes_base_properties_t *>(pNext);
        if (pExtProps->stype == ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTIES) {
            auto pDowngradeExtProps = reinterpret_cast<zes_intel_pci_link_speed_downgrade_exp_properties_t *>(pExtProps);
            auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
            uint32_t downgradeCapable;
            if (pSysmanKmdInterface->readPcieDowngradeAttribute("pcieDowngradeCapable", downgradeCapable) == ZE_RESULT_SUCCESS) {
                pDowngradeExtProps->pciLinkSpeedUpdateCapable = downgradeCapable;
                pDowngradeExtProps->maxPciGenSupported = pSysmanProductHelper->maxPcieGenSupported();
                isPciDowngradePropertiesAvailable = true;
            }
            break;
        }
        pNext = pExtProps->pNext;
    }
    return pSysmanProductHelper->getPciProperties(pProperties);
}

ze_result_t LinuxPciImp::getPciBdf(zes_pci_properties_t &pciProperties) {
    std::string bdfDir;
    ze_result_t result = pSysfsAccess->readSymLink(deviceDir, bdfDir);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): readSymLink() failed to retrieve BDF from %s and returning error:0x%x \n", __FUNCTION__, deviceDir.c_str(), result);
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
    maxLinkSpeed = 0;
    maxLinkWidth = -1;

    auto isIntegratedDevice = pLinuxSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().getHardwareInfo()->capabilityTable.isIntegratedDevice;
    if (isIntegratedDevice) {
        return;
    }

    std::string pciConfigNode = {};
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    if (pSysmanProductHelper->isUpstreamPortConnected()) {
        pSysfsAccess->getRealPath(deviceDir, pciConfigNode);
        std::string cardBusPath = pLinuxSysmanImp->getPciCardBusDirectoryPath(pciConfigNode);
        pciConfigNode = cardBusPath + "/config";
    } else {
        pSysfsAccess->getRealPath("device/config", pciConfigNode);
    }

    std::vector<uint8_t> configMemory(PCI_CFG_SPACE_SIZE);
    if (!getPciConfigMemory(std::move(pciConfigNode), configMemory)) {
        return;
    }

    auto linkCapPos = L0::Sysman::LinuxPciImp::getLinkCapabilityPos(configMemory.data());
    if (!linkCapPos) {
        return;
    }

    uint16_t linkCaps = L0::Sysman::PciUtil::getWordFromConfig(linkCapPos, configMemory.data());
    maxLinkSpeed = convertPciGenToLinkSpeed(bits(linkCaps, 0, 4));
    maxLinkWidth = bits(linkCaps, 4, 6);

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
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): read() failed to read %s and returning error:0x%x \n", __FUNCTION__, resourceFile.c_str(), result);
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
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): BarProperties = %d and returning error:0x%x \n", __FUNCTION__, pBarProperties.size(), result);
        result = ZE_RESULT_ERROR_UNKNOWN;
    }
    return result;
}

uint32_t LinuxPciImp::getRebarCapabilityPos(uint8_t *configMemory, bool isVfBar) {
    uint32_t pos = PCI_CFG_SPACE_SIZE;
    uint32_t header = 0;

    // Minimum 8 bytes per capability. Hence maximum capabilities that
    // could be present in PCI extended configuration space are
    // represented by loopCount.
    auto loopCount = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;
    header = L0::Sysman::PciUtil::getDwordFromConfig(pos, configMemory);
    if (!header) {
        return 0;
    }

    uint32_t capId = isVfBar ? PCI_EXT_CAP_ID_VF_REBAR : PCI_EXT_CAP_ID_REBAR;

    while (loopCount-- > 0) {
        if (PCI_EXT_CAP_ID(header) == capId) {
            return pos;
        }
        pos = PCI_EXT_CAP_NEXT(header);
        if (pos < PCI_CFG_SPACE_SIZE) {
            return 0;
        }
        header = L0::Sysman::PciUtil::getDwordFromConfig(pos, configMemory);
    }
    return 0;
}

uint16_t LinuxPciImp::getLinkCapabilityPos(uint8_t *configMem) {
    uint16_t pos = PCI_CAPABILITY_LIST;
    uint8_t id, type = 0;
    uint16_t capRegister = 0;

    if (!(L0::Sysman::PciUtil::getByteFromConfig(PCI_STATUS, configMem) & PCI_STATUS_CAP_LIST)) {
        return 0;
    }

    // Minimum 8 bytes per capability. Hence maximum capabilities that
    // could be present in PCI configuration space are
    // represented by loopCount.
    auto loopCount = (PCI_CFG_SPACE_SIZE) / 8;

    // Bottom two bits of capability pointer register are reserved and
    // software should mask these bits to get pointer to capability list.

    pos = L0::Sysman::PciUtil::getByteFromConfig(pos, configMem) & 0xfc;
    while (loopCount-- > 0) {
        id = L0::Sysman::PciUtil::getByteFromConfig(pos + PCI_CAP_LIST_ID, configMem);
        if (id == PCI_CAP_ID_EXP) {
            capRegister = L0::Sysman::PciUtil::getWordFromConfig(pos + PCI_CAP_FLAGS, configMem);
            type = bits(capRegister, 4, 4);

            // Root Complex Integrated end point and
            // Root Complex Event collector will not implement link capabilities

            if ((type != PCI_EXP_TYPE_RC_END) && (type != PCI_EXP_TYPE_RC_EC)) {
                return pos + PCI_EXP_LNKCAP;
            }
        }

        pos = L0::Sysman::PciUtil::getByteFromConfig(pos + PCI_CAP_LIST_NEXT, configMem) & 0xfc;
        if (!pos) {
            break;
        }
    }
    return 0;
}

// Parse PCIe configuration space to see if resizable Bar is supported
bool LinuxPciImp::resizableBarSupported() {
    std::string pciConfigNode = {};
    pSysfsAccess->getRealPath("device/config", pciConfigNode);
    std::vector<uint8_t> configMemory(PCI_CFG_SPACE_EXP_SIZE);
    if (!getPciConfigMemory(std::move(pciConfigNode), configMemory)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Unable to get pci config space \n", __FUNCTION__);
        return false;
    }
    return (L0::Sysman::LinuxPciImp::getRebarCapabilityPos(configMemory.data(), false) > 0);
}

bool LinuxPciImp::resizableBarEnabled(uint32_t barIndex) {
    bool isBarResizable = false;
    uint32_t capabilityRegister = 0, controlRegister = 0;
    uint32_t nBars = 1;

    std::string pciConfigNode = {};
    pSysfsAccess->getRealPath("device/config", pciConfigNode);
    std::vector<uint8_t> configMemory(PCI_CFG_SPACE_EXP_SIZE);
    if (!getPciConfigMemory(std::move(pciConfigNode), configMemory)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Unable to get pci config space \n", __FUNCTION__);
        return false;
    }

    auto rebarCapabilityPos = L0::Sysman::LinuxPciImp::getRebarCapabilityPos(configMemory.data(), false);

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
    controlRegister = L0::Sysman::PciUtil::getDwordFromConfig(rebarCapabilityPos + PCI_REBAR_CTRL, configMemory.data());
    nBars = bits(controlRegister, 5, 3); // control register's bits 5,6 and 7 contain number of resizable bars information
    for (auto barNumber = 0u; barNumber < nBars; barNumber++) {
        uint32_t barId = 0;
        controlRegister = L0::Sysman::PciUtil::getDwordFromConfig(rebarCapabilityPos + PCI_REBAR_CTRL, configMemory.data());
        barId = bits(controlRegister, 0, 3); // Control register's bit 0,1,2 tells the index of bar
        if (barId == barIndex) {
            isBarResizable = true;
            break;
        }
        rebarCapabilityPos += 8;
    }

    if (isBarResizable == false) {
        return false;
    }

    capabilityRegister = L0::Sysman::PciUtil::getDwordFromConfig(rebarCapabilityPos + PCI_REBAR_CAP, configMemory.data());
    // Capability register's bit 4 to 31 indicates supported Bar sizes.
    // In possibleBarSizes, position of each set bit indicates supported bar size. Example,  if set bit
    // position of possibleBarSizes is from 0 to n, then this indicates BAR size from 2^0 MB to 2^n MB
    auto possibleBarSizes = (capabilityRegister & PCI_REBAR_CAP_SIZES) >> 4; // First 4 bits are reserved
    uint32_t largestPossibleBarSize = 0;
    while (possibleBarSizes >>= 1) { // most significant set bit position of possibleBarSizes would tell largest possible bar size
        largestPossibleBarSize++;
    }

    // Control register's bit 8 to 13 indicates current BAR size in encoded form.
    // Example, real value of current size could be 2^currentSize MB
    auto currentSize = bits(controlRegister, 8, 6);

    // If current size is equal to largest possible BAR size, it indicates resizable BAR is enabled.
    return (currentSize == largestPossibleBarSize);
}

ze_result_t LinuxPciImp::getState(zes_pci_state_t *state) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    state->qualityIssues = 0;
    state->stabilityIssues = 0;
    state->status = ZES_PCI_LINK_STATUS_UNKNOWN;

    state->speed.gen = -1;
    state->speed.width = -1;
    state->speed.maxBandwidth = -1;

    const void *pNext = state->pNext;
    while (pNext) {
        result = ZE_RESULT_ERROR_INVALID_ARGUMENT;
        auto pExtProps = reinterpret_cast<zes_base_properties_t *>(const_cast<void *>(pNext));
        if (pExtProps->stype == ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE) {
            auto pDowngradeExpState = reinterpret_cast<zes_intel_pci_link_speed_downgrade_exp_state_t *>(pExtProps);
            auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
            uint32_t downgradeStatus;
            result = pSysmanKmdInterface->readPcieDowngradeAttribute("pcieDowngradeStatus", downgradeStatus);
            if (result == ZE_RESULT_SUCCESS) {
                pDowngradeExpState->pciLinkSpeedDowngradeStatus = downgradeStatus;
            }
            break;
        }
        pNext = pExtProps->pNext;
    }
    return result;
}

ze_result_t LinuxPciImp::pciLinkSpeedUpdateExp(ze_bool_t downgradeUpgrade, zes_device_action_t *pendingAction) {
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();

    if (!pSysmanProductHelper->isPcieDowngradeSupported()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    if (pFwInterface == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    uint8_t requestedState = 0x0;
    if (downgradeUpgrade) {
        requestedState = 0x1;
    }

    std::vector<uint8_t> outBuf(maxGfspHeciOutBuffer, 0);
    ze_result_t result = pFwInterface->fwGetGfspConfig(GfspHeciConstants::Cmd::getConfigurationCmd16, outBuf);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    std::vector<uint8_t> inBuf(outBuf.begin() + GfspHeciConstants::GetCmd16BytePostition::pendingStateBytePosition, outBuf.begin() + GfspHeciConstants::GetCmd16BytePostition::pendingStateBytePosition + 4);
    inBuf[GfspHeciConstants::SetCmd15BytePostition::request] &= ~(1 << 1);
    inBuf[GfspHeciConstants::SetCmd15BytePostition::request] |= (requestedState << 1);
    result = pFwInterface->fwSetGfspConfig(GfspHeciConstants::Cmd::setConfigurationCmd15, inBuf, outBuf);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    uint8_t pendingState = (outBuf[GfspHeciConstants::SetCmd15BytePostition::response] >> 1) & 0x1;
    *pendingAction = ZES_DEVICE_ACTION_NONE;
    uint32_t currentState = 0x0;

    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    result = pSysmanKmdInterface->readPcieDowngradeAttribute("pcieDowngradeStatus", currentState);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    if (currentState != pendingState) {
        *pendingAction = ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT;
    }

    return result;
}

ze_result_t LinuxPciImp::getStats(zes_pci_stats_t *stats) {
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    return pSysmanProductHelper->getPciStats(stats, pLinuxSysmanImp);
}

bool LinuxPciImp::getPciConfigMemory(std::string pciPath, std::vector<uint8_t> &configMem) {
    if (!pSysfsAccess->isRootUser()) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Need to be root to read config space \n", __FUNCTION__);
        return false;
    }

    auto fd = NEO::FileDescriptor(pciPath.c_str(), O_RDONLY);
    if (fd < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() Config File Open Failed \n", __FUNCTION__);
        return false;
    }
    if (this->preadFunction(fd, configMem.data(), configMem.size(), 0) < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() Config Mem Read Failed \n", __FUNCTION__);
        return false;
    }
    return true;
}

LinuxPciImp::LinuxPciImp(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsPci *OsPci::create(OsSysman *pOsSysman) {
    LinuxPciImp *pLinuxPciImp = new LinuxPciImp(pOsSysman);
    return static_cast<OsPci *>(pLinuxPciImp);
}

} // namespace Sysman
} // namespace L0

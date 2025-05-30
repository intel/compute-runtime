/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

#include "neo_igfxfmid.h"

namespace L0 {

const std::string LinuxMemoryImp::deviceMemoryHealth("device_memory_health");

void memoryGetTimeStamp(uint64_t &timestamp) {
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
}

void LinuxMemoryImp::init() {
    if (isSubdevice) {
        const std::string baseDir = "gt/gt" + std::to_string(subdeviceId) + "/";
        physicalSizeFile = baseDir + "addr_range";
    }
}

LinuxMemoryImp::LinuxMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = &pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getDeviceHandle();
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pPmt = pLinuxSysmanImp->getPlatformMonitoringTechAccess(subdeviceId);
    init();
}

bool LinuxMemoryImp::isMemoryModuleSupported() {
    return pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
}

ze_result_t LinuxMemoryImp::getProperties(zes_mem_properties_t *pProperties) {
    pProperties->type = ZES_MEM_TYPE_DDR;
    pProperties->numChannels = -1;
    if (pDrm->querySystemInfo()) {
        auto memSystemInfo = pDrm->getSystemInfo();
        if (memSystemInfo != nullptr) {
            pProperties->numChannels = memSystemInfo->getMaxMemoryChannels();
            auto memType = memSystemInfo->getMemoryType();
            switch (memType) {
            case NEO::DeviceBlobConstants::MemoryType::hbm2e:
            case NEO::DeviceBlobConstants::MemoryType::hbm2:
                pProperties->type = ZES_MEM_TYPE_HBM;
                break;
            case NEO::DeviceBlobConstants::MemoryType::lpddr4:
                pProperties->type = ZES_MEM_TYPE_LPDDR4;
                break;
            case NEO::DeviceBlobConstants::MemoryType::lpddr5:
                pProperties->type = ZES_MEM_TYPE_LPDDR5;
                break;
            default:
                pProperties->type = ZES_MEM_TYPE_DDR;
                break;
            }
        }
    }
    pProperties->location = ZES_MEM_LOC_DEVICE;
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subdeviceId;
    pProperties->busWidth = memoryBusWidth; // Hardcode

    pProperties->physicalSize = 0;
    if (isSubdevice) {
        std::string memval;
        ze_result_t result = pSysfsAccess->read(physicalSizeFile, memval);
        uint64_t intval = strtoull(memval.c_str(), nullptr, 16);
        if (ZE_RESULT_SUCCESS != result) {
            pProperties->physicalSize = 0u;
        } else {
            pProperties->physicalSize = intval;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxMemoryImp::getVFIDString(std::string &vfID) {
    uint32_t vf0VfIdVal = 0;
    std::string key = "VF0_VFID";
    auto result = pPmt->readValue(key, vf0VfIdVal);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for VF0_VFID is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    uint32_t vf1VfIdVal = 0;
    key = "VF1_VFID";
    result = pPmt->readValue(key, vf1VfIdVal);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for VF1_VFID is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    // At any point of time only one VF(virtual function) could be active and thus would
    // read greater than zero val. If both VF0 and VF1 are reading 0 or both are reading
    // greater than 0, then we would be confused in taking the decision of correct VF.
    // Lets assume and report this as a error condition
    if (((vf0VfIdVal == 0) && (vf1VfIdVal == 0)) ||
        ((vf0VfIdVal > 0) && (vf1VfIdVal > 0))) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() VF0 returning 0x%x and VF1 returning 0x%x as both should not be the same \n", __FUNCTION__, vf0VfIdVal, vf1VfIdVal);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (vf0VfIdVal > 0) {
        vfID = "VF0";
    }
    if (vf1VfIdVal > 0) {
        vfID = "VF1";
    }
    return result;
}

ze_result_t LinuxMemoryImp::readMcChannelCounters(uint64_t &readCounters, uint64_t &writeCounters) {
    // For DG2 there are 8 memory instances each memory instance has 2 channels there are total 16 MC Channels
    uint32_t numMcChannels = 16u;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    std::vector<std::string> nameOfCounters{"IDI_READS", "IDI_WRITES", "DISPLAY_VC1_READS"};
    std::vector<uint64_t> counterValues(3, 0); // Will store the values of counters metioned in nameOfCounters
    for (uint64_t counterIndex = 0; counterIndex < nameOfCounters.size(); counterIndex++) {
        for (uint32_t mcChannelIndex = 0; mcChannelIndex < numMcChannels; mcChannelIndex++) {
            uint64_t val = 0;
            std::string readCounterKey = nameOfCounters[counterIndex] + "[" + std::to_string(mcChannelIndex) + "]";
            result = pPmt->readValue(readCounterKey, val);
            if (result != ZE_RESULT_SUCCESS) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterKey returning error:0x%x \n", __FUNCTION__, result);
                return result;
            }
            counterValues[counterIndex] += val;
        }
    }
    // PMT counters returns number of transactions that have occured and each tranaction is of 64 bytes
    // Multiplying 32(tranaction size) with number of transactions gives the total reads or writes in bytes
    constexpr uint64_t transactionSize = 32;
    readCounters = (counterValues[0] + counterValues[2]) * transactionSize; // Read counters are summation of total IDI_READS and DISPLAY_VC1_READS
    writeCounters = (counterValues[1]) * transactionSize;                   // Write counters are summation of IDI_WRITES
    return result;
}

void LinuxMemoryImp::getHbmFrequency(PRODUCT_FAMILY productFamily, unsigned short stepping, uint64_t &hbmFrequency) {
    hbmFrequency = 0;
    if (productFamily == IGFX_PVC) {
        if (stepping >= REVISION_B) {
            const std::string baseDir = "gt/gt" + std::to_string(subdeviceId) + "/";
            // Calculating bandwidth based on HBM max frequency
            const std::string hbmRP0FreqFile = baseDir + "mem_RP0_freq_mhz";
            uint64_t hbmFreqValue = 0;
            ze_result_t result = pSysfsAccess->read(hbmRP0FreqFile, hbmFreqValue);
            if (ZE_RESULT_SUCCESS == result) {
                hbmFrequency = hbmFreqValue * 1000 * 1000; // Converting MHz value to Hz
                return;
            }
        } else if (stepping == REVISION_A0) {
            // For IGFX_PVC REV A0 HBM frequency would be 3.2 GT/s = 3.2 * 1000 * 1000 * 1000 T/s = 3200000000 T/s
            hbmFrequency = 3.2 * gigaUnitTransferToUnitTransfer;
        }
    }
}

ze_result_t LinuxMemoryImp::getBandwidthForDg2(zes_mem_bandwidth_t *pBandwidth) {
    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;
    pBandwidth->timestamp = 0;
    pBandwidth->maxBandwidth = 0;
    ze_result_t result = readMcChannelCounters(pBandwidth->readCounter, pBandwidth->writeCounter);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readMcChannelCounters returning error:0x%x  \n", __FUNCTION__, result);
        return result;
    }
    pBandwidth->maxBandwidth = 0u;
    const std::string maxBwFile = "prelim_lmem_max_bw_Mbps";
    uint64_t maxBw = 0;
    result = pSysfsAccess->read(maxBwFile, maxBw);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():pSysfsAccess->read returning error:0x%x  \n", __FUNCTION__, result);
    }
    pBandwidth->maxBandwidth = maxBw * mbpsToBytesPerSecond;
    pBandwidth->timestamp = SysmanDevice::getSysmanTimestamp();
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxMemoryImp::getHbmBandwidth(uint32_t numHbmModules, zes_mem_bandwidth_t *pBandwidth) {
    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;
    pBandwidth->timestamp = 0;
    pBandwidth->maxBandwidth = 0;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    std::string vfId = "";
    result = getVFIDString(vfId);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():getVFIDString returning error:0x%x while retriving VFID string \n", __FUNCTION__, result);
        return result;
    }
    auto &hwInfo = pDevice->getNEODevice()->getHardwareInfo();
    auto productFamily = hwInfo.platform.eProductFamily;
    auto &productHelper = pDevice->getNEODevice()->getProductHelper();
    auto stepping = productHelper.getSteppingFromHwRevId(hwInfo);
    for (auto hbmModuleIndex = 0u; hbmModuleIndex < numHbmModules; hbmModuleIndex++) {
        uint32_t counterValue = 0;
        // To read counters from VFID 0 and HBM module 0, key would be: VF0_HBM0_READ
        std::string readCounterKey = vfId + "_HBM" + std::to_string(hbmModuleIndex) + "_READ";
        result = pPmt->readValue(readCounterKey, counterValue);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterKey returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
        pBandwidth->readCounter += counterValue;

        counterValue = 0;
        // To write counters to VFID 0 and HBM module 0, key would be: VF0_HBM0_Write
        std::string writeCounterKey = vfId + "_HBM" + std::to_string(hbmModuleIndex) + "_WRITE";
        result = pPmt->readValue(writeCounterKey, counterValue);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for writeCounterKey returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
        pBandwidth->writeCounter += counterValue;
    }
    constexpr uint64_t transactionSize = 32;
    pBandwidth->readCounter = pBandwidth->readCounter * transactionSize;
    pBandwidth->writeCounter = pBandwidth->writeCounter * transactionSize;
    pBandwidth->timestamp = SysmanDevice::getSysmanTimestamp();

    uint64_t hbmFrequency = 0;
    getHbmFrequency(productFamily, stepping, hbmFrequency);

    pBandwidth->maxBandwidth = memoryBusWidth * hbmFrequency * numHbmModules; // Value in bytes/secs
    return result;
}

ze_result_t LinuxMemoryImp::getHbmBandwidthPVC(uint32_t numHbmModules, zes_mem_bandwidth_t *pBandwidth) {
    std::string guid = pPmt->getGuid();
    if (guid != guid64BitMemoryCounters) {
        return getHbmBandwidth(numHbmModules, pBandwidth);
    }
    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;
    pBandwidth->timestamp = 0;
    pBandwidth->maxBandwidth = 0;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    std::string vfId = "";
    result = getVFIDString(vfId);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():getVFIDString returning error:0x%x while retriving VFID string \n", __FUNCTION__, result);
        return result;
    }
    auto &hwInfo = pDevice->getNEODevice()->getHardwareInfo();
    auto productFamily = hwInfo.platform.eProductFamily;
    auto &productHelper = pDevice->getNEODevice()->getProductHelper();
    auto stepping = productHelper.getSteppingFromHwRevId(hwInfo);

    uint32_t readCounterL = 0;
    std::string readCounterKey = vfId + "_HBM_READ_L";
    result = pPmt->readValue(readCounterKey, readCounterL);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterL returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    uint32_t readCounterH = 0;
    readCounterKey = vfId + "_HBM_READ_H";
    result = pPmt->readValue(readCounterKey, readCounterH);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterH returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    constexpr uint64_t transactionSize = 32;
    pBandwidth->readCounter = readCounterH;
    pBandwidth->readCounter = (pBandwidth->readCounter << 32) | static_cast<uint64_t>(readCounterL);
    pBandwidth->readCounter = (pBandwidth->readCounter * transactionSize);

    uint32_t writeCounterL = 0;
    std::string writeCounterKey = vfId + "_HBM_WRITE_L";
    result = pPmt->readValue(writeCounterKey, writeCounterL);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for writeCounterL returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    uint32_t writeCounterH = 0;
    writeCounterKey = vfId + "_HBM_WRITE_H";
    result = pPmt->readValue(writeCounterKey, writeCounterH);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for writeCounterH returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    pBandwidth->writeCounter = writeCounterH;
    pBandwidth->writeCounter = (pBandwidth->writeCounter << 32) | static_cast<uint64_t>(writeCounterL);
    pBandwidth->writeCounter = (pBandwidth->writeCounter * transactionSize);

    uint64_t timeStampVal = 0;
    memoryGetTimeStamp(timeStampVal);
    pBandwidth->timestamp = timeStampVal;

    uint64_t hbmFrequency = 0;
    getHbmFrequency(productFamily, stepping, hbmFrequency);

    pBandwidth->maxBandwidth = memoryBusWidth * hbmFrequency * numHbmModules; // Value in bytes/secs
    return result;
}

ze_result_t LinuxMemoryImp::getBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    if (pPmt == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    auto &hwInfo = pDevice->getNEODevice()->getHardwareInfo();
    auto productFamily = hwInfo.platform.eProductFamily;
    uint32_t numHbmModules = 0u;
    switch (productFamily) {
    case IGFX_DG2:
        result = getBandwidthForDg2(pBandwidth);
        break;
    case IGFX_PVC:
        numHbmModules = 4u;
        result = getHbmBandwidthPVC(numHbmModules, pBandwidth);
        break;
    default:
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }
    return result;
}

ze_result_t LinuxMemoryImp::getState(zes_mem_state_t *pState) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    pState->health = ZES_MEM_HEALTH_UNKNOWN;
    FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    auto productFamily = SysmanDeviceImp::getProductFamily(pDevice);
    if ((pFwInterface != nullptr) && (IGFX_PVC == productFamily)) {
        pFwInterface->fwGetMemoryHealthIndicator(&pState->health);
    }
    auto memoryInfo = pDrm->getIoctlHelper()->createMemoryInfo();
    if (memoryInfo != nullptr) {
        auto region = memoryInfo->getMemoryRegion(MemoryBanks::getBankForLocalMemory(subdeviceId));
        pState->free = region.unallocatedSize;
        pState->size = region.probedSize;
    } else {
        pState->free = 0;
        pState->size = 0;
        status = ZE_RESULT_ERROR_UNKNOWN;
        if (errno == ENODEV) {
            status = ZE_RESULT_ERROR_DEVICE_LOST;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Error@ %s():createMemoryInfo failed errno:%d \n", __FUNCTION__, errno);
    }
    return status;
}

std::unique_ptr<OsMemory> OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<LinuxMemoryImp> pLinuxMemoryImp = std::make_unique<LinuxMemoryImp>(pOsSysman, onSubdevice, subdeviceId);
    return pLinuxMemoryImp;
}

} // namespace L0

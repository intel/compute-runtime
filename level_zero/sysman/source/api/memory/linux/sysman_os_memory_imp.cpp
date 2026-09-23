/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/memory/linux/sysman_os_memory_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preprocessor.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <map>
#include <sstream>

namespace L0 {
namespace Sysman {

static const std::map<uint32_t, std::string> memoryVendorIdToNameMap = {
    {0xFFu, "Micron"},
};

ze_result_t LinuxMemoryImp::getProperties(zes_mem_properties_t *pProperties) {
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    return pSysmanProductHelper->getMemoryProperties(pProperties, pLinuxSysmanImp, pDrm, pSysmanKmdInterface, subdeviceId, isSubdevice);
}

ze_result_t LinuxMemoryImp::getVendorId(uint32_t *pVendorId) {
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    return pSysmanProductHelper->getMemoryVendorId(pLinuxSysmanImp, pVendorId);
}

ze_result_t LinuxMemoryImp::getExtensionProperties(void *pNext) {
    while (pNext) {
        auto pExtProps = reinterpret_cast<zes_base_properties_t *>(pNext);
        if (pExtProps->stype == ZES_STRUCTURE_TYPE_MEMORY_VENDOR_INFO_EXT_PROPERTIES) {
            auto pVendorIdProps = reinterpret_cast<zes_memory_vendor_info_ext_properties_t *>(pExtProps);
            // A vendor ID of 0 indicates that the memory vendor ID could not be determined
            if (getVendorId(&pVendorIdProps->vendorId) != ZE_RESULT_SUCCESS) {
                pVendorIdProps->vendorId = 0;
            }
            // A length of 0 indicates that the memory vendor name could not be determined.
            pVendorIdProps->length = 0;
            pVendorIdProps->vendorName[0] = '\0';
            auto vendorNameIterator = memoryVendorIdToNameMap.find(pVendorIdProps->vendorId);
            if (vendorNameIterator != memoryVendorIdToNameMap.end()) {
                const std::string &vendorName = vendorNameIterator->second;
                strncpy_s(pVendorIdProps->vendorName, ZES_MEMORY_VENDOR_NAME_EXT_SIZE, vendorName.c_str(), vendorName.size());
                pVendorIdProps->length = static_cast<uint16_t>(vendorName.length());
            }
        }
        pNext = pExtProps->pNext;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxMemoryImp::getBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    return pSysmanProductHelper->getMemoryBandwidth(pBandwidth, pLinuxSysmanImp, subdeviceId);
}

ze_result_t LinuxMemoryImp::getState(zes_mem_state_t *pState) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    pState->health = ZES_MEM_HEALTH_UNKNOWN;

    if (pLinuxSysmanImp->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        const std::string memFreeKey = "MemFree";
        const std::string memAvailableKey = "MemAvailable";
        std::unordered_set<std::string> keys{memFreeKey, memAvailableKey};
        auto memInfoValues = readMemInfoValues(&pLinuxSysmanImp->getFsAccess(), keys);
        if (memInfoValues.find(memFreeKey) != memInfoValues.end() && memInfoValues.find(memAvailableKey) != memInfoValues.end()) {
            pState->free = memInfoValues[memFreeKey] * 1024;
            pState->size = memInfoValues[memAvailableKey] * 1024;
        } else {
            pState->free = 0;
            pState->size = 0;
            status = ZE_RESULT_ERROR_UNKNOWN;
        }
        return status;
    }

    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    pSysmanProductHelper->getMemoryHealthIndicator(pLinuxSysmanImp, &pState->health);

    std::unique_ptr<NEO::MemoryInfo> memoryInfo;
    auto hwDeviceId = pLinuxSysmanImp->getSysmanHwDeviceIdInstance();
    memoryInfo = pDrm->getIoctlHelper()->createMemoryInfo();
    if (!memoryInfo) {
        pState->free = 0;
        pState->size = 0;
        status = ZE_RESULT_ERROR_UNKNOWN;
        if (errno == ENODEV) {
            status = ZE_RESULT_ERROR_DEVICE_LOST;
        }
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s():createMemoryInfo failed errno:%d \n", NEO_FUNCTION_NAME, errno);
        return status;
    }

    auto region = memoryInfo->getMemoryRegion(MemoryBanks::getBankForLocalMemory(subdeviceId));
    pState->free = region.unallocatedSize;
    pState->size = region.probedSize;
    return status;
}

std::unordered_map<std::string, uint64_t> LinuxMemoryImp::readMemInfoValues(FsAccessInterface *pFsAccess, const std::unordered_set<std::string> &keys) {
    std::unordered_map<std::string, uint64_t> result;
    const std::string memInfoFile = "/proc/meminfo";
    std::vector<std::string> memInfo;

    if (pFsAccess->read(std::move(memInfoFile), memInfo) == ZE_RESULT_SUCCESS) {
        for (const auto &line : memInfo) {
            std::istringstream lineStream(line);
            std::string label, unit;
            uint64_t value = 0;
            lineStream >> label >> value >> unit;
            if (!label.empty() && label.back() == ':') {
                label.pop_back();
            }
            if (keys.count(label)) {
                result[label] = value;
                if (result.size() == keys.size()) {
                    break;
                }
            }
        }
    }
    return result;
}

LinuxMemoryImp::LinuxMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getSysmanDeviceImp();
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
}

bool LinuxMemoryImp::isMemoryModuleSupported() {
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    if (pLinuxSysmanImp->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        return true;
    }
    return gfxCoreHelper.getEnableLocalMemory(pDevice->getHardwareInfo());
}

std::unique_ptr<OsMemory> OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<LinuxMemoryImp> pLinuxMemoryImp = std::make_unique<LinuxMemoryImp>(pOsSysman, onSubdevice, subdeviceId);
    return pLinuxMemoryImp;
}

} // namespace Sysman
} // namespace L0

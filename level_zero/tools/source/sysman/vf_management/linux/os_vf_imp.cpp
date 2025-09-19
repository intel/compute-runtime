/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/vf_management/linux/os_vf_imp.h"

#include "shared/source/os_interface/driver_info.h"
#include "shared/source/utilities/directory.h"

#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

namespace L0 {
static const std::string pathForVfTelemetryPrefix = "iov/vf";

ze_result_t LinuxVfImp::getVfBDFAddress(uint32_t vfIdMinusOne, zes_pci_address_t *address) {
    std::string pathForVfBdf = "device/virtfn";
    std::string vfRealPath = "";
    std::string vfPath = pathForVfBdf + std::to_string(vfIdMinusOne);
    ze_result_t result = pSysfsAccess->getRealPath(vfPath, vfRealPath);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get the real path and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    std::size_t loc = vfRealPath.find_last_of("/");
    if (loc == std::string::npos) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get the last occurrence of '/' and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    std::string vfBdfString = vfRealPath.substr(loc + 1);

    constexpr int vfBdfTokensNum = 4;
    uint16_t domain = -1;
    uint8_t bus = -1, device = -1, function = -1;
    if (NEO::parseBdfString(vfBdfString, domain, bus, device, function) != vfBdfTokensNum) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get the correct token sum and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    address->domain = domain;
    address->bus = bus;
    address->device = device;
    address->function = function;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxVfImp::vfOsGetCapabilities(zes_vf_exp2_capabilities_t *pCapability) {

    ze_result_t result = getVfBDFAddress((vfId - 1), &pCapability->address);
    if (result != ZE_RESULT_SUCCESS) {
        pCapability->address.domain = 0;
        pCapability->address.bus = 0;
        pCapability->address.device = 0;
        pCapability->address.function = 0;
        return result;
    }

    uint64_t vfLmemQuota = 0;
    if (!vfOsGetLocalMemoryQuota(vfLmemQuota)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    pCapability->vfDeviceMemSize = vfLmemQuota;
    pCapability->vfID = vfId;

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxVfImp::vfOsGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) {
    uint64_t vfLmemUsed = 0;
    if (*pCount == 0) {
        *pCount = maxMemoryTypes;
        return ZE_RESULT_SUCCESS;
    }
    if (*pCount > maxMemoryTypes) {
        *pCount = maxMemoryTypes;
    }
    if (!vfOsGetLocalMemoryUsed(vfLmemUsed)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (pMemUtil != nullptr) {
        for (uint32_t i = 0; i < *pCount; i++) {
            pMemUtil[i].vfMemLocation = ZES_MEM_LOC_DEVICE;
            pMemUtil[i].vfMemUtilized = vfLmemUsed;
        }
    }

    return ZE_RESULT_SUCCESS;
}

bool LinuxVfImp::vfOsGetLocalMemoryUsed(uint64_t &lMemUsed) {
    std::string pathForLmemUsed = "/telemetry/lmem_alloc_size";
    std::string pathForDeviceMemUsed = pathForVfTelemetryPrefix + std::to_string(vfId) + pathForLmemUsed;

    auto result = pSysfsAccess->read(pathForDeviceMemUsed.data(), lMemUsed);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read Local Memory Used with error 0x%x \n", __FUNCTION__, result);
        return false;
    }
    return true;
}

bool LinuxVfImp::vfOsGetLocalMemoryQuota(uint64_t &lMemQuota) {
    std::string pathForLmemQuota = "/gt/lmem_quota";
    std::string pathForDeviceMemQuota = pathForVfTelemetryPrefix + std::to_string(vfId) + pathForLmemQuota;

    auto result = pSysfsAccess->read(pathForDeviceMemQuota.data(), lMemQuota);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read Local Memory Quota with error 0x%x \n", __FUNCTION__, result);
        return false;
    }
    return true;
}

ze_result_t LinuxVfImp::vfOsGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) {

    if (pLinuxSysmanImp->isUsingPrelimEnabledKmd == false) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return getEngineUtilization(pCount, pEngineUtil);
}

LinuxVfImp::LinuxVfImp(
    OsSysman *pOsSysman, uint32_t vfId) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    this->vfId = vfId;
}

void LinuxVfImp::cleanup() {
    for (auto pEngineUtilsData : pEngineUtils) {
        DEBUG_BREAK_IF(pEngineUtilsData.busyTicksFd < 0);
        NEO::SysCalls::close(static_cast<int>(pEngineUtilsData.busyTicksFd));
        DEBUG_BREAK_IF(pEngineUtilsData.totalTicksFd < 0);
        NEO::SysCalls::close(static_cast<int>(pEngineUtilsData.totalTicksFd));
    }
    pEngineUtils.clear();
}

LinuxVfImp::~LinuxVfImp() {
    cleanup();
}

std::unique_ptr<OsVf> OsVf::create(
    OsSysman *pOsSysman, uint32_t vfId) {
    std::unique_ptr<LinuxVfImp> pLinuxVfImp = std::make_unique<LinuxVfImp>(pOsSysman, vfId);
    return pLinuxVfImp;
}

uint32_t OsVf::getNumEnabledVfs(OsSysman *pOsSysman) {
    constexpr std::string_view pathForNumberOfVfs = "device/sriov_numvfs";
    uint32_t numberOfVfs = 0;
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    SysfsAccess *pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    auto result = pSysfsAccess->read(pathForNumberOfVfs.data(), numberOfVfs);
    if (result != ZE_RESULT_SUCCESS) {
        numberOfVfs = 0;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read Number Of Vfs with error 0x%x \n", __FUNCTION__, result);
    }
    return numberOfVfs;
}

} // namespace L0

/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"

#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/sysman_const.h"
namespace L0 {
namespace Sysman {

const std::map<uint16_t, std::string> SysmanKmdInterfaceI915::i915EngineClassToSysfsEngineMap = {
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER, "rcs"},
    {static_cast<uint16_t>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COMPUTE), "ccs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY, "bcs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO, "vcs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE, "vecs"}};

static const std::multimap<zes_engine_type_flag_t, std::string> level0EngineTypeToSysfsEngineMap = {
    {ZES_ENGINE_TYPE_FLAG_RENDER, "rcs"},
    {ZES_ENGINE_TYPE_FLAG_COMPUTE, "ccs"},
    {ZES_ENGINE_TYPE_FLAG_DMA, "bcs"},
    {ZES_ENGINE_TYPE_FLAG_MEDIA, "vcs"},
    {ZES_ENGINE_TYPE_FLAG_OTHER, "vecs"}};

SysmanKmdInterface::SysmanKmdInterface() = default;
SysmanKmdInterface::~SysmanKmdInterface() = default;

std::unique_ptr<SysmanKmdInterface> SysmanKmdInterface::create(NEO::Drm &drm, SysmanProductHelper *pSysmanProductHelper) {
    std::unique_ptr<SysmanKmdInterface> pSysmanKmdInterface;
    auto drmVersion = drm.getDrmVersion(drm.getFileDescriptor());
    if ("xe" == drmVersion) {
        pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pSysmanProductHelper);
    } else {
        std::string prelimVersion;
        drm.getPrelimVersion(prelimVersion);
        if (prelimVersion == "") {
            pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915Upstream>(pSysmanProductHelper);
        } else {
            pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915Prelim>(pSysmanProductHelper);
        }
    }

    return pSysmanKmdInterface;
}

ze_result_t SysmanKmdInterface::initFsAccessInterface(const NEO::Drm &drm) {
    pFsAccess = FsAccessInterface::create();
    pProcfsAccess = ProcFsAccessInterface::create();
    std::string deviceName;
    auto result = pProcfsAccess->getFileName(pProcfsAccess->myProcessId(), drm.getFileDescriptor(), deviceName);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to device name and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    pSysfsAccess = SysFsAccessInterface::create(std::move(deviceName));
    return result;
}

FsAccessInterface *SysmanKmdInterface::getFsAccess() {
    UNRECOVERABLE_IF(nullptr == pFsAccess.get());
    return pFsAccess.get();
}

ProcFsAccessInterface *SysmanKmdInterface::getProcFsAccess() {
    UNRECOVERABLE_IF(nullptr == pProcfsAccess.get());
    return pProcfsAccess.get();
}

SysFsAccessInterface *SysmanKmdInterface::getSysFsAccess() {
    UNRECOVERABLE_IF(nullptr == pSysfsAccess.get());
    return pSysfsAccess.get();
}

ze_result_t SysmanKmdInterface::getNumEngineTypeAndInstancesForSubDevices(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                                          NEO::Drm *pDrm,
                                                                          uint32_t subdeviceId) {
    NEO::EngineInfo *engineInfo = nullptr;
    {
        auto hwDeviceId = static_cast<SysmanHwDeviceIdDrm *>(pDrm->getHwDeviceId().get())->getSingleInstance();
        engineInfo = pDrm->getEngineInfo();
    }
    if (engineInfo == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    std::vector<NEO::EngineClassInstance> listOfEngines;
    engineInfo->getListOfEnginesOnATile(subdeviceId, listOfEngines);
    for (const auto &engine : listOfEngines) {
        std::string sysfEngineString = getEngineClassString(engine.engineClass).value_or(" ");
        if (sysfEngineString == " ") {
            continue;
        }

        std::string sysfsEngineDirNode = sysfEngineString + std::to_string(engine.engineInstance);
        auto level0EngineType = sysfsEngineMapToLevel0EngineType.find(sysfEngineString);
        if (level0EngineType == sysfsEngineMapToLevel0EngineType.end()) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "Error@ %s(): unknown engine type: %s and returning error:0x%x \n", __FUNCTION__, sysfEngineString.c_str(),
                                  ZE_RESULT_ERROR_UNKNOWN);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        auto ret = mapOfEngines.find(level0EngineType->second);
        if (ret != mapOfEngines.end()) {
            ret->second.push_back(sysfsEngineDirNode);
        } else {
            std::vector<std::string> engineVec = {};
            engineVec.push_back(sysfsEngineDirNode);
            mapOfEngines.emplace(level0EngineType->second, engineVec);
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t SysmanKmdInterface::getNumEngineTypeAndInstancesForDevice(std::string engineDir, std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                                      SysFsAccessInterface *pSysfsAccess) {
    std::vector<std::string> localListOfAllEngines = {};
    auto result = pSysfsAccess->scanDirEntries(std::move(engineDir), localListOfAllEngines);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to scan directory entries to list all engines and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    for_each(localListOfAllEngines.begin(), localListOfAllEngines.end(),
             [&](std::string &mappedEngine) {
                 for (auto itr = level0EngineTypeToSysfsEngineMap.begin(); itr != level0EngineTypeToSysfsEngineMap.end(); itr++) {
                     char digits[] = "0123456789";
                     auto mappedEngineName = mappedEngine.substr(0, mappedEngine.find_first_of(digits, 0));
                     if (0 == mappedEngineName.compare(itr->second.c_str())) {
                         auto ret = mapOfEngines.find(itr->first);
                         if (ret != mapOfEngines.end()) {
                             ret->second.push_back(mappedEngine);
                         } else {
                             std::vector<std::string> engineVec = {};
                             engineVec.push_back(mappedEngine);
                             mapOfEngines.emplace(itr->first, engineVec);
                         }
                     }
                 }
             });
    return result;
}

SysfsValueUnit SysmanKmdInterface::getNativeUnit(const SysfsName sysfsName) {
    auto sysfsNameToNativeUnitMap = getSysfsNameToNativeUnitMap();
    if (sysfsNameToNativeUnitMap.find(sysfsName) != sysfsNameToNativeUnitMap.end()) {
        return sysfsNameToNativeUnitMap[sysfsName];
    }
    // Entries are expected to be available at sysfsNameToNativeUnitMap
    DEBUG_BREAK_IF(true);
    return SysfsValueUnit::unAvailable;
}

void SysmanKmdInterface::convertSysfsValueUnit(const SysfsValueUnit dstUnit, const SysfsValueUnit srcUnit, const uint64_t srcValue, uint64_t &dstValue) const {
    dstValue = srcValue;

    if (dstUnit != srcUnit) {
        if (dstUnit == SysfsValueUnit::milli && srcUnit == SysfsValueUnit::micro) {
            dstValue = srcValue / 1000u;
        } else if (dstUnit == SysfsValueUnit::micro && srcUnit == SysfsValueUnit::milli) {
            dstValue = srcValue * 1000u;
        }
    }
}

uint32_t SysmanKmdInterface::getEventType() {

    auto pFsAccess = getFsAccess();
    const std::string eventTypeSysfsNode = std::string(sysDevicesDir) + sysmanDeviceDirName + "/" + "type";
    auto eventTypeVal = 0u;
    if (ZE_RESULT_SUCCESS != pFsAccess->read(eventTypeSysfsNode, eventTypeVal)) {
        return 0;
    }
    return eventTypeVal;
}

void SysmanKmdInterface::getWedgedStatusImpl(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) {
    NEO::GemContextCreateExt gcc{};
    auto hwDeviceId = pLinuxSysmanImp->getSysmanHwDeviceIdInstance();
    auto pDrm = pLinuxSysmanImp->getDrm();
    // Device is said to be in wedged if context creation returns EIO.
    auto ret = pDrm->getIoctlHelper()->ioctl(NEO::DrmIoctl::gemContextCreateExt, &gcc);
    if (ret == 0) {
        pDrm->destroyDrmContext(gcc.contextId);
        return;
    }

    if (pDrm->getErrno() == EIO) {
        pState->reset |= ZES_RESET_REASON_FLAG_WEDGED;
    }
}

ze_result_t SysmanKmdInterface::checkErrorNumberAndReturnStatus() {
    if (errno == EMFILE || errno == ENFILE) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): System has run out of file handles. Suggested action is to increase the file handle limit. \n", __FUNCTION__);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void SysmanKmdInterface::updateSysmanDeviceDirName(std::string &dirName) {

    std::string bdfDir = "";
    auto pSysfsAccess = getSysFsAccess();
    auto result = pSysfsAccess->readSymLink(std::string(deviceDir), bdfDir);
    if (ZE_RESULT_SUCCESS != result) {
        dirName = "";
        return;
    }
    const auto loc = bdfDir.find_last_of('/');      // Gives the location of the last occurence of '/' in the bdfDir path. Eg: bdfDir = ../../../0000:03:00.0
    auto bdf = bdfDir.substr(loc + 1);              // The bdf will start after the last location of '/'. Eg: bdf = 0000:03:00.0
    std::replace(bdf.begin(), bdf.end(), ':', '_'); // The ':' is replaced by '_'. Eg: bdf = 0000_03_00.0
    dirName = dirName + "_" + bdf;                  // The final dirName has bdf name appended to the dirName. Eg: i915_0000_03_00.0 or xe_0000_03_00.0
}

const std::string SysmanKmdInterface::getSysmanDeviceDirName() const {
    return sysmanDeviceDirName;
}

std::string SysmanKmdInterfaceI915::getBasePathI915(uint32_t subDeviceId) {
    return "gt/gt" + std::to_string(subDeviceId) + "/";
}

std::string SysmanKmdInterfaceI915::getHwmonNameI915(uint32_t subDeviceId, bool isSubdevice) {
    std::string filePath = isSubdevice ? "i915_gt" + std::to_string(subDeviceId) : "i915";
    return filePath;
}

std::string SysmanKmdInterfaceI915::getEngineBasePathI915(uint32_t subDeviceId) {
    return "engine";
}

std::optional<std::string> SysmanKmdInterfaceI915::getEngineClassStringI915(uint16_t engineClass) {
    auto sysfEngineString = i915EngineClassToSysfsEngineMap.find(static_cast<drm_i915_gem_engine_class>(engineClass));
    if (sysfEngineString == i915EngineClassToSysfsEngineMap.end()) {
        DEBUG_BREAK_IF(true);
        return {};
    }
    return sysfEngineString->second;
}

std::string SysmanKmdInterfaceI915::getGpuBindEntryI915() {
    return "/sys/bus/pci/drivers/i915/bind";
}

std::string SysmanKmdInterfaceI915::getGpuUnBindEntryI915() {
    return "/sys/bus/pci/drivers/i915/unbind";
}

} // namespace Sysman
} // namespace L0

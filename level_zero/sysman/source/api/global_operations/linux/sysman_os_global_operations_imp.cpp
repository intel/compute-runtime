/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/global_operations/linux/sysman_os_global_operations_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/pci_path.h"

#include "level_zero/sysman/source/api/global_operations/sysman_global_operations_imp.h"
#include "level_zero/sysman/source/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/linux/sysman_fs_access.h"
#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/source/sysman_const.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <time.h>

namespace L0 {
namespace Sysman {

const std::string LinuxGlobalOperationsImp::deviceDir("device");
const std::string LinuxGlobalOperationsImp::vendorFile("device/vendor");
const std::string LinuxGlobalOperationsImp::subsystemVendorFile("device/subsystem_vendor");
const std::string LinuxGlobalOperationsImp::driverFile("device/driver");
const std::string LinuxGlobalOperationsImp::functionLevelReset("device/reset");
const std::string LinuxGlobalOperationsImp::clientsDir("clients");
const std::string LinuxGlobalOperationsImp::srcVersionFile("/sys/module/i915/srcversion");
const std::string LinuxGlobalOperationsImp::agamaVersionFile("/sys/module/i915/agama_version");
const std::string LinuxGlobalOperationsImp::ueventWedgedFile("/var/lib/libze_intel_gpu/wedged_file");

// Map engine entries(numeric values) present in /sys/class/drm/card<n>/clients/<client_n>/busy,
// with engine enum defined in leve-zero spec
// Note that entries with int 2 and 3(represented by i915 as CLASS_VIDEO and CLASS_VIDEO_ENHANCE)
// are both mapped to MEDIA, as CLASS_VIDEO represents any media fixed-function hardware.
static const std::map<int, zes_engine_type_flags_t> engineMap = {
    {0, ZES_ENGINE_TYPE_FLAG_3D},
    {1, ZES_ENGINE_TYPE_FLAG_DMA},
    {2, ZES_ENGINE_TYPE_FLAG_MEDIA},
    {3, ZES_ENGINE_TYPE_FLAG_MEDIA},
    {4, ZES_ENGINE_TYPE_FLAG_COMPUTE}};

bool LinuxGlobalOperationsImp::getTelemOffsetAndTelemDir(uint64_t &telemOffset, const std::string &key, std::string &telemDir) {
    std::string &rootPath = pLinuxSysmanImp->getPciRootPath();
    if (rootPath.empty()) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Root path has no value \n", __FUNCTION__);
        return false;
    }

    std::map<uint32_t, std::string> telemPciPath;
    NEO::PmtUtil::getTelemNodesInPciPath(std::string_view(rootPath), telemPciPath);
    uint32_t subDeviceCount = pLinuxSysmanImp->getSubDeviceCount() + 1;
    if (telemPciPath.size() < subDeviceCount) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Number of telemetry nodes:%d is lessthan %d \n", __FUNCTION__, telemPciPath.size(), subDeviceCount);
        return false;
    }

    auto iterator = telemPciPath.begin();
    telemDir = iterator->second;

    std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
    if (!NEO::PmtUtil::readGuid(telemDir, guidString)) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read GUID from %s \n", __FUNCTION__, telemDir.c_str());
        return false;
    }

    uint64_t offset = ULONG_MAX;
    if (!NEO::PmtUtil::readOffset(telemDir, offset)) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read offset from %s\n", __FUNCTION__, telemDir.c_str());
        return false;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    if (ZE_RESULT_SUCCESS == PlatformMonitoringTech::getKeyOffsetMap(guidString.data(), keyOffsetMap)) {
        auto keyOffset = keyOffsetMap.find(key.c_str());
        if (keyOffset != keyOffsetMap.end()) {
            telemOffset = keyOffset->second + offset;
            return true;
        }
    }
    NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to find keyOffset in keyOffsetMap \n", __FUNCTION__);
    return false;
}

bool LinuxGlobalOperationsImp::getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) {
    uint64_t offset = 0;
    std::string telemDir = {};
    if (!LinuxGlobalOperationsImp::getTelemOffsetAndTelemDir(offset, "PPIN", telemDir)) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get telemetry offset and directory for PPIN \n", __FUNCTION__);
        return false;
    }

    uint64_t value;
    ssize_t bytesRead = NEO::PmtUtil::readTelem(telemDir.data(), sizeof(uint64_t), offset, &value);
    if (bytesRead == sizeof(uint64_t)) {
        std::ostringstream telemDataString;
        telemDataString << std::hex << std::showbase << value;
        memcpy_s(serialNumber, ZES_STRING_PROPERTY_SIZE, telemDataString.str().c_str(), telemDataString.str().size());
        return true;
    }
    NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read serial number \n", __FUNCTION__);
    return false;
}

bool LinuxGlobalOperationsImp::getBoardNumber(char (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) {
    uint64_t offset = 0;
    std::string telemDir = {};
    constexpr uint32_t boardNumberSize = 32;
    if (!LinuxGlobalOperationsImp::getTelemOffsetAndTelemDir(offset, "BoardNumber", telemDir)) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get telemetry offset and directory for BoardNumber \n", __FUNCTION__);
        return false;
    }
    std::array<uint8_t, boardNumberSize> value;
    ssize_t bytesRead = NEO::PmtUtil::readTelem(telemDir.data(), boardNumberSize, offset, value.data());
    if (bytesRead == boardNumberSize) {
        // Board Number from PMT is available as multiple uint32_t integers, We need to swap (i.e convert each uint32_t
        // to big endian) elements of each uint32_t to get proper board number string.
        // For instance, Board Number stored in PMT space is as follows (stored as uint32_t - Little endian):
        // 1. BoardNumber0 - 2PTW
        // 2. BoardNumber1 - 0503
        // 3. BoardNumber2 - 0130. BoardNumber is actual combination of all 0, 1 and 2 i.e WTP230500310.
        for (uint32_t i = 0; i < boardNumberSize; i += 4) {
            std::swap(value[i], value[i + 3]);
            std::swap(value[i + 1], value[i + 2]);
        }
        memcpy_s(boardNumber, ZES_STRING_PROPERTY_SIZE, value.data(), bytesRead);
        return true;
    }
    NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read board number \n", __FUNCTION__);
    return false;
}

void LinuxGlobalOperationsImp::getBrandName(char (&brandName)[ZES_STRING_PROPERTY_SIZE]) {
    std::string strVal;
    ze_result_t result = pSysfsAccess->read(subsystemVendorFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        std::strncpy(brandName, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
        return;
    }
    if (strVal.compare(intelPciId) == 0) {
        std::strncpy(brandName, vendorIntel.c_str(), ZES_STRING_PROPERTY_SIZE);
    } else {
        std::strncpy(brandName, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
}

void LinuxGlobalOperationsImp::getModelName(char (&modelName)[ZES_STRING_PROPERTY_SIZE]) {
    auto &hwInfo = pLinuxSysmanImp->getParentSysmanDeviceImp()->getHardwareInfo();
    std::string deviceName = hwInfo.capabilityTable.deviceName;
    if (!deviceName.empty()) {
        std::strncpy(modelName, deviceName.c_str(), ZES_STRING_PROPERTY_SIZE);
        return;
    }

    std::stringstream deviceNameDefault;
    deviceNameDefault << "Intel(R) Graphics";
    deviceNameDefault << " [0x" << std::hex << std::setw(4) << std::setfill('0') << hwInfo.platform.usDeviceID << "]";
    std::strncpy(modelName, deviceNameDefault.str().c_str(), ZES_STRING_PROPERTY_SIZE);
}

void LinuxGlobalOperationsImp::getVendorName(char (&vendorName)[ZES_STRING_PROPERTY_SIZE]) {
    std::string strVal;
    ze_result_t result = pSysfsAccess->read(vendorFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        std::strncpy(vendorName, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
        return;
    }
    if (strVal.compare(intelPciId) == 0) {
        std::strncpy(vendorName, vendorIntel.c_str(), ZES_STRING_PROPERTY_SIZE);
    } else {
        std::strncpy(vendorName, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
}

void LinuxGlobalOperationsImp::getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) {
    std::string strVal;
    std::strncpy(driverVersion, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
    ze_result_t result = pFsAccess->read(agamaVersionFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        if (ZE_RESULT_ERROR_NOT_AVAILABLE != result) {
            NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read driver version from %s and returning error:0x%x \n", __FUNCTION__, agamaVersionFile.c_str(), result);
            return;
        }
        result = pFsAccess->read(srcVersionFile, strVal);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read driver version from %s and returning error:0x%x\n", __FUNCTION__, srcVersionFile.c_str(), result);
            return;
        }
    }
    std::strncpy(driverVersion, strVal.c_str(), ZES_STRING_PROPERTY_SIZE);
    return;
}

bool LinuxGlobalOperationsImp::generateUuidFromPciBusInfo(const NEO::PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) {
    if (pciBusInfo.pciDomain != NEO::PhysicalDevicePciBusInfo::invalidValue) {
        uuid.fill(0);

        /* Device UUID uniquely identifies a device within a system.
         * We generate it based on device information along with PCI information
         * This guarantees uniqueness of UUIDs on a system even when multiple
         * identical Intel GPUs are present.
         */

        /* We want to have UUID matching between different GPU APIs (including outside
         * of compute_runtime project - i.e. other than L0 or OCL). This structure definition
         * has been agreed upon by various Intel driver teams.
         *
         * Consult other driver teams before changing this.
         */

        struct DeviceUUID {
            uint16_t vendorID;
            uint16_t deviceID;
            uint16_t revisionID;
            uint16_t pciDomain;
            uint8_t pciBus;
            uint8_t pciDev;
            uint8_t pciFunc;
            uint8_t reserved[4];
            uint8_t subDeviceID;
        };

        auto &hwInfo = pLinuxSysmanImp->getParentSysmanDeviceImp()->getHardwareInfo();
        DeviceUUID deviceUUID = {};
        deviceUUID.vendorID = 0x8086; // Intel
        deviceUUID.deviceID = hwInfo.platform.usDeviceID;
        deviceUUID.revisionID = hwInfo.platform.usRevId;
        deviceUUID.pciDomain = static_cast<uint16_t>(pciBusInfo.pciDomain);
        deviceUUID.pciBus = static_cast<uint8_t>(pciBusInfo.pciBus);
        deviceUUID.pciDev = static_cast<uint8_t>(pciBusInfo.pciDevice);
        deviceUUID.pciFunc = static_cast<uint8_t>(pciBusInfo.pciFunction);
        deviceUUID.subDeviceID = 0;

        static_assert(sizeof(DeviceUUID) == NEO::ProductHelper::uuidSize);

        memcpy_s(uuid.data(), NEO::ProductHelper::uuidSize, &deviceUUID, sizeof(DeviceUUID));

        return true;
    }
    return false;
}

bool LinuxGlobalOperationsImp::getUuid(std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) {
    auto driverModel = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironment().osInterface->getDriverModel();
    auto &gfxCoreHelper = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    auto &productHelper = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    if (NEO::DebugManager.flags.EnableChipsetUniqueUUID.get() != 0) {
        if (gfxCoreHelper.isChipsetUniqueUUIDSupported()) {
            auto hwDeviceId = pLinuxSysmanImp->getSysmanHwDeviceIdInstance();
            this->uuid.isValid = productHelper.getUuid(driverModel, subDeviceCount, 0u, this->uuid.id);
        }
    }

    if (!this->uuid.isValid && pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironment().osInterface != nullptr) {
        NEO::PhysicalDevicePciBusInfo pciBusInfo = driverModel->getPciBusInfo();
        this->uuid.isValid = generateUuidFromPciBusInfo(pciBusInfo, this->uuid.id);
    }

    if (this->uuid.isValid) {
        uuid = this->uuid.id;
    }

    return this->uuid.isValid;
}

ze_result_t LinuxGlobalOperationsImp::reset(ze_bool_t force) {
    auto hwInfo = pLinuxSysmanImp->getParentSysmanDeviceImp()->getHardwareInfo();
    auto resetType = hwInfo.capabilityTable.isIntegratedDevice ? ZES_RESET_TYPE_FLR : ZES_RESET_TYPE_WARM;

    return resetImpl(force, resetType);
}

ze_result_t LinuxGlobalOperationsImp::resetExt(zes_reset_properties_t *pProperties) {
    return resetImpl(pProperties->force, pProperties->resetType);
}

ze_result_t LinuxGlobalOperationsImp::resetImpl(ze_bool_t force, zes_reset_type_t resetType) {
    if (!pSysfsAccess->isRootUser()) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Not running as root user and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }

    pLinuxSysmanImp->releaseSysmanDeviceResources();
    ze_result_t result = pLinuxSysmanImp->gpuProcessCleanup(force);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): gpuProcessCleanup() failed and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    std::string resetName;
    pSysfsAccess->getRealPath(deviceDir, resetName);
    resetName = pFsAccess->getBaseName(resetName);

    // Unbind the device from the kernel driver.
    result = pSysfsAccess->unbindDevice(resetName);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to unbind device:%s and returning error:0x%x \n", __FUNCTION__, resetName.c_str(), result);
        return result;
    }

    std::vector<::pid_t> processes;
    result = pProcfsAccess->listProcesses(processes);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to list processes and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    std::vector<::pid_t> deviceUsingPids;
    deviceUsingPids.clear();
    for (auto &&pid : processes) {
        std::vector<int> fds;
        pLinuxSysmanImp->getPidFdsForOpenDevice(pid, fds);
        if (!fds.empty()) {
            // Kill all processes that have the device open.
            pProcfsAccess->kill(pid);
            deviceUsingPids.push_back(pid);
        }
    }

    // Wait for all the processes to exit
    // If they don't all exit within resetTimeout
    // just fail reset.
    auto start = std::chrono::steady_clock::now();
    auto end = start;
    for (auto &&pid : deviceUsingPids) {
        while (pProcfsAccess->isAlive(pid)) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() > resetTimeout) {
                NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Timeout reached, device still in use and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
                return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
            }
            struct ::timespec timeout = {.tv_sec = 0, .tv_nsec = 1000};
            ::nanosleep(&timeout, NULL);
            end = std::chrono::steady_clock::now();
        }
    }

    std::string resetPath = {};
    switch (resetType) {
    case ZES_RESET_TYPE_WARM:
        result = pLinuxSysmanImp->osWarmReset();
        break;
    case ZES_RESET_TYPE_COLD:
        result = pLinuxSysmanImp->osColdReset();
        break;
    case ZES_RESET_TYPE_FLR:
        pSysfsAccess->getRealPath(functionLevelReset, resetPath);
        result = pFsAccess->write(resetPath, "1");
        break;
    default:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to reset the device and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    // Rebind the device to the kernel driver.
    result = pSysfsAccess->bindDevice(resetName);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to rebind the device to the kernel driver and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    return pLinuxSysmanImp->reInitSysmanDeviceResources();
}

ze_result_t LinuxGlobalOperationsImp::getMemoryStatsUsedByProcess(std::vector<std::string> &fdFileContents, uint64_t &memSize, uint64_t &sharedSize) {
    const std::string memSizeString("drm-total-vram");
    const std::string sharedSizeString("drm-shared-vram");

    auto convertToBytes = [](std::string unitOfSize) -> std::uint64_t {
        if (unitOfSize.empty()) {
            return 1;
        } else if (unitOfSize == "KiB") {
            return MemoryConstants::kiloByte;
        } else if (unitOfSize == "MiB") {
            return MemoryConstants::megaByte;
        }
        DEBUG_BREAK_IF(1); // Some unknowm unit is exposed by KMD, need a debug
        return 0;
    };

    for (const auto &fileContents : fdFileContents) {
        std::istringstream iss(fileContents);
        std::string label;
        uint64_t value = 0;
        std::string unitOfSize;
        iss >> label >> value >> unitOfSize;

        // Example: consider "fileContents = "drm-total-vram0: 120 MiB""
        // Then if we are here, then label would be "drm-total-vram0:". So remove `:` from label
        label = label.substr(0, label.length() - 1);
        if (label.substr(0, memSizeString.length()) == memSizeString) {
            // Convert Memory obtained to bytes
            value = value * convertToBytes(unitOfSize);
            memSize += value;
        } else if (label.substr(0, sharedSizeString.length()) == sharedSizeString) {
            // Convert Memory obtained to bytes
            value = value * convertToBytes(unitOfSize);
            sharedSize += value;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxGlobalOperationsImp::getListOfEnginesUsedByProcess(std::vector<std::string> &fdFileContents, uint32_t &activeEngines) {
    // Map engine entries present in /proc/<pid>>/fdinfo/<fd>,
    // with engine enum defined in leve-zero spec
    // Note that entries with int "video" and "video_enhance"(represented as CLASS_VIDEO and CLASS_VIDEO_ENHANCE)
    // are both mapped to MEDIA, as CLASS_VIDEO represents any media fixed-function hardware.
    const std::map<std::string, zes_engine_type_flags_t> engineMap = {
        {"drm-engine-render", ZES_ENGINE_TYPE_FLAG_RENDER},
        {"drm-engine-copy", ZES_ENGINE_TYPE_FLAG_DMA},
        {"drm-engine-video", ZES_ENGINE_TYPE_FLAG_MEDIA},
        {"drm-engine-video-enhance", ZES_ENGINE_TYPE_FLAG_MEDIA},
        {"drm-engine-compute", ZES_ENGINE_TYPE_FLAG_COMPUTE}};

    const std::string engineStringPrefix("drm-engine-");
    for (const auto &fileContents : fdFileContents) {
        std::istringstream iss(fileContents);
        std::string label;
        uint64_t value;
        iss >> label >> value;
        // Example: consider "fileContents = "drm-engine-render:      25662044495 ns""
        // Then if we are here, then label would be "drm-engine-render:". So remove `:` from label
        label = label.substr(0, label.length() - 1);

        if ((label.substr(0, engineStringPrefix.length()) == engineStringPrefix) && (value != 0)) {
            auto it = engineMap.find(label);
            if (it == engineMap.end()) {
                NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                                      "Error@ %s(): unknown engine type: %s and returning error:0x%x \n", __FUNCTION__, label.c_str(),
                                      ZE_RESULT_ERROR_UNKNOWN);
                DEBUG_BREAK_IF(1);
                return ZE_RESULT_ERROR_UNKNOWN;
            }
            activeEngines = activeEngines | it->second;
        }
    }
    return ZE_RESULT_SUCCESS;
}

// Example fdinfo format:
//
// # cat /proc/1383/fdinfo/8
// pos:    0
// flags:  02100002
// mnt_id: 21
// ino:    397
// drm-driver:     i915
// drm-client-id:  18
// drm-pdev:       0000:00:02.0
// drm-total-vram0:        512
// drm-total-vram1:        512 KiB
// drm-shared-vram0:       512 KiB
// drm-shared-vram1:       128 MiB
// drm-total-system:       125 MiB
// drm-shared-system:      16 MiB
// drm-active-system:      110 MiB
// drm-resident-system:    125 MiB
// drm-purgeable-system:   2 MiB
// drm-total-stolen-system:        0
// drm-shared-stolen-system:       0
// drm-active-stolen-system:       0
// drm-resident-stolen-system:     0
// drm-purgeable-stolen-system:    0
// drm-engine-render:      25662044495 ns
// drm-engine-copy:        0 ns
// drm-engine-video:       0 ns
// drm-engine-video-enhance:       0 ns
ze_result_t LinuxGlobalOperationsImp::readClientInfoFromFdInfo(std::map<uint64_t, EngineMemoryPairType> &pidClientMap) {
    std::map<::pid_t, std::vector<int>> gpuClientProcessMap; // This map contains processes and their opened gpu File descriptors
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    {
        std::vector<::pid_t> processes;
        result = pProcfsAccess->listProcesses(processes);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Unable to list processes and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }

        for (auto &&pid : processes) {
            std::vector<int> fds;
            pLinuxSysmanImp->getPidFdsForOpenDevice(pid, fds);
            if (!fds.empty()) {
                gpuClientProcessMap.insert({pid, fds});
            }
        }
    }

    // iterate for each process
    for (const auto &gpuClientProcess : gpuClientProcessMap) {
        // iterate over all the opened GPU device file descriptors
        uint64_t pid = static_cast<uint64_t>(gpuClientProcess.first);
        uint32_t activeEngines = 0u; // This contains bit fields of engines used by processes
        uint64_t memSize = 0u;
        uint64_t sharedSize = 0u;
        for (const auto &fd : gpuClientProcess.second) {
            std::string fdInfoPath = "/proc/" + std::to_string(static_cast<int>(pid)) + "/fdinfo/" + std::to_string(fd);
            std::vector<std::string> fdFileContents;
            result = pFsAccess->read(fdInfoPath, fdFileContents);
            if (ZE_RESULT_SUCCESS != result) {
                if (ZE_RESULT_ERROR_NOT_AVAILABLE == result) {
                    // update the result as Success as ZE_RESULT_ERROR_NOT_AVAILABLE is expected if process exited by the time we are readig it.
                    result = ZE_RESULT_SUCCESS;
                    continue;
                } else {
                    return result;
                }
            }

            result = getListOfEnginesUsedByProcess(fdFileContents, activeEngines);
            if (result != ZE_RESULT_SUCCESS) {
                NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                                      "Error@ %s(): List of engines used by process(%d) with fd(%d) could not be retrieved.\n", __FUNCTION__,
                                      pid, fd);
                return result;
            }

            result = getMemoryStatsUsedByProcess(fdFileContents, memSize, sharedSize);
            if (result != ZE_RESULT_SUCCESS) {
                NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                                      "Error@ %s(): Memory used by process(%d) with fd(%d) could not be retrieved.\n", __FUNCTION__,
                                      pid, fd);
                return result;
            }
        }
        DeviceMemStruct totalDeviceMem = {memSize, sharedSize};
        EngineMemoryPairType engineMemoryPair = {static_cast<int64_t>(activeEngines), totalDeviceMem};
        pidClientMap.insert(std::make_pair(pid, engineMemoryPair));
    }
    return result;
}

// Processes in the form of clients are present in sysfs like this:
// # /sys/class/drm/card0/clients$ ls
// 4  5
// # /sys/class/drm/card0/clients/4$ ls
// busy  name  pid
// # /sys/class/drm/card0/clients/4/busy$ ls
// 0  1  2  3
//
// Number of processes(If one process opened drm device multiple times, then multiple entries will be
// present for same process in clients directory) will be the number of clients
// (For example from above example, processes dirs are 4,5)
// Thus total number of times drm connection opened with this device will be 2.
// process.pid = pid (from above example)
// process.engines -> For each client's busy dir, numbers 0,1,2,3 represent engines and they contain
// accumulated nanoseconds each client spent on engines.
// Thus we traverse each file in busy dir for non-zero time and if we find that file say 0,then we could say that
// this engine 0 is used by process.
ze_result_t LinuxGlobalOperationsImp::readClientInfoFromSysfs(std::map<uint64_t, EngineMemoryPairType> &pidClientMap) {
    std::vector<std::string> clientIds;
    ze_result_t result = pSysfsAccess->scanDirEntries(clientsDir, clientIds);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to scan directory entries from %s and returning error:0x%x \n", __FUNCTION__, clientsDir.c_str(), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    for (const auto &clientId : clientIds) {
        // realClientPidPath will be something like: clients/<clientId>/pid
        std::string realClientPidPath = clientsDir + "/" + clientId + "/" + "pid";
        uint64_t pid;
        result = pSysfsAccess->read(realClientPidPath, pid);

        if (ZE_RESULT_SUCCESS != result) {
            std::string bPidString;
            result = pSysfsAccess->read(realClientPidPath, bPidString);
            if (result == ZE_RESULT_SUCCESS) {
                size_t start = bPidString.find("<");
                size_t end = bPidString.find(">");
                std::string bPid = bPidString.substr(start + 1, end - start - 1);
                pid = std::stoull(bPid, nullptr, 10);
            }
        }

        if (ZE_RESULT_SUCCESS != result) {
            if (ZE_RESULT_ERROR_NOT_AVAILABLE == result) {
                // update the result as Success as ZE_RESULT_ERROR_NOT_AVAILABLE is expected if the "realClientPidPath" folder is empty
                // this condition(when encountered) must not prevent the information accumulated for other clientIds
                // this situation occurs when there is no call modifying result,
                result = ZE_RESULT_SUCCESS;
                continue;
            } else {
                return result;
            }
        }
        // Traverse the clients/<clientId>/busy directory to get accelerator engines used by process
        std::vector<std::string> engineNums = {};
        int64_t engineType = 0;
        std::string busyDirForEngines = clientsDir + "/" + clientId + "/" + "busy";
        result = pSysfsAccess->scanDirEntries(busyDirForEngines, engineNums);
        if (ZE_RESULT_SUCCESS != result) {
            if (ZE_RESULT_ERROR_NOT_AVAILABLE == result) {
                // update the result as Success as ZE_RESULT_ERROR_NOT_AVAILABLE is expected if the "realClientPidPath" folder is empty
                // this condition(when encountered) must not prevent the information accumulated for other clientIds
                // this situation occurs when there is no call modifying result,
                // Here its seen when the last element of clientIds returns ZE_RESULT_ERROR_NOT_AVAILABLE for some reason.
                engineType = ZES_ENGINE_TYPE_FLAG_OTHER; // When busy node is absent assign engine type with ZES_ENGINE_TYPE_FLAG_OTHER
            } else {
                return result;
            }
        }
        // Scan all engine files present in /sys/class/drm/card0/clients/<ClientId>/busy and check
        // whether that engine is used by process
        for (const auto &engineNum : engineNums) {
            uint64_t timeSpent = 0;
            std::string engine = busyDirForEngines + "/" + engineNum;
            result = pSysfsAccess->read(engine, timeSpent);
            if (ZE_RESULT_SUCCESS != result) {
                if (ZE_RESULT_ERROR_NOT_AVAILABLE == result) {
                    continue;
                } else {
                    return result;
                }
            }
            if (timeSpent > 0) {
                int i915EnginNumber = stoi(engineNum);
                auto i915MapToL0EngineType = engineMap.find(i915EnginNumber);
                zes_engine_type_flags_t val = ZES_ENGINE_TYPE_FLAG_OTHER;
                if (i915MapToL0EngineType != engineMap.end()) {
                    // Found a valid map
                    val = i915MapToL0EngineType->second;
                }
                // In this for loop we want to retrieve the overall engines used by process
                engineType = engineType | val;
            }
        }

        uint64_t memSize = 0;
        std::string realClientTotalMemoryPath = clientsDir + "/" + clientId + "/" + "total_device_memory_buffer_objects" + "/" + "created_bytes";
        result = pSysfsAccess->read(realClientTotalMemoryPath, memSize);
        if (ZE_RESULT_SUCCESS != result) {
            if (ZE_RESULT_ERROR_NOT_AVAILABLE != result) {
                NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read memory size from:%s and returning error:0x%x \n", __FUNCTION__, realClientTotalMemoryPath.c_str(), result);
                return result;
            }
        }

        uint64_t sharedMemSize = 0;
        std::string realClientTotalSharedMemoryPath = clientsDir + "/" + clientId + "/" + "total_device_memory_buffer_objects" + "/" + "imported_bytes";
        result = pSysfsAccess->read(realClientTotalSharedMemoryPath, sharedMemSize);
        if (ZE_RESULT_SUCCESS != result) {
            if (ZE_RESULT_ERROR_NOT_AVAILABLE != result) {
                NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read shared memory size from:%s and returning error:0x%x \n", __FUNCTION__, realClientTotalSharedMemoryPath.c_str(), result);
                return result;
            }
        }
        DeviceMemStruct totalDeviceMem = {memSize, sharedMemSize};
        EngineMemoryPairType engineMemoryPair = {engineType, totalDeviceMem};
        auto ret = pidClientMap.insert(std::make_pair(pid, engineMemoryPair));
        if (ret.second == false) {
            // insertion failed as entry with same pid already exists in map
            // Now update the EngineMemoryPairType field for the existing pid entry
            EngineMemoryPairType updateEngineMemoryPair;
            auto pidEntryFromMap = pidClientMap.find(pid);
            auto existingEngineType = pidEntryFromMap->second.engineTypeField;
            auto existingdeviceMemorySize = pidEntryFromMap->second.deviceMemStructField.deviceMemorySize;
            auto existingdeviceSharedMemorySize = pidEntryFromMap->second.deviceMemStructField.deviceSharedMemorySize;
            updateEngineMemoryPair.engineTypeField = existingEngineType | engineMemoryPair.engineTypeField;
            updateEngineMemoryPair.deviceMemStructField.deviceMemorySize = existingdeviceMemorySize + engineMemoryPair.deviceMemStructField.deviceMemorySize;
            updateEngineMemoryPair.deviceMemStructField.deviceSharedMemorySize = existingdeviceSharedMemorySize + engineMemoryPair.deviceMemStructField.deviceSharedMemorySize;
            pidClientMap[pid] = updateEngineMemoryPair;
        }
        result = ZE_RESULT_SUCCESS;
    }

    return result;
}

ze_result_t LinuxGlobalOperationsImp::scanProcessesState(std::vector<zes_process_state_t> &pProcessList) {
    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    // Create a map with unique pid as key and EngineMemoryPairType as value
    std::map<uint64_t, EngineMemoryPairType> pidClientMap;

    result = pLinuxSysmanImp->getSysmanKmdInterface()->clientInfoAvailableInFdInfo() ? readClientInfoFromFdInfo(pidClientMap) : readClientInfoFromSysfs(pidClientMap);

    // iterate through all elements of pidClientMap
    for (auto itr = pidClientMap.begin(); itr != pidClientMap.end(); ++itr) {
        zes_process_state_t process{};
        process.processId = static_cast<uint32_t>(itr->first);
        process.memSize = itr->second.deviceMemStructField.deviceMemorySize;
        process.sharedSize = itr->second.deviceMemStructField.deviceSharedMemorySize;
        process.engines = static_cast<uint32_t>(itr->second.engineTypeField);
        pProcessList.push_back(process);
    }
    return result;
}

void LinuxGlobalOperationsImp::getWedgedStatus(zes_device_state_t *pState) {
    NEO::GemContextCreateExt gcc{};
    auto hwDeviceId = pLinuxSysmanImp->getSysmanHwDeviceIdInstance();
    auto pDrm = pLinuxSysmanImp->getDrm();
    // Device is said to be in wedged if context creation returns EIO.
    auto ret = pDrm->getIoctlHelper()->ioctl(NEO::DrmIoctl::GemContextCreateExt, &gcc);
    if (ret == 0) {
        pDrm->destroyDrmContext(gcc.contextId);
        return;
    }

    if (pDrm->getErrno() == EIO) {
        pState->reset |= ZES_RESET_REASON_FLAG_WEDGED;
    }
}
ze_result_t LinuxGlobalOperationsImp::deviceGetState(zes_device_state_t *pState) {
    memset(pState, 0, sizeof(zes_device_state_t));
    pState->repaired = ZES_REPAIR_STATUS_UNSUPPORTED;
    getWedgedStatus(pState);
    getRepairStatus(pState);
    return ZE_RESULT_SUCCESS;
}

LinuxGlobalOperationsImp::LinuxGlobalOperationsImp(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);

    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pProcfsAccess = &pLinuxSysmanImp->getProcfsAccess();
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
    devicePciBdf = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>()->getPciPath();
    rootDeviceIndex = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceIndex();
    uuid.isValid = false;
}

OsGlobalOperations *OsGlobalOperations::create(OsSysman *pOsSysman) {
    LinuxGlobalOperationsImp *pLinuxGlobalOperationsImp = new LinuxGlobalOperationsImp(pOsSysman);
    return static_cast<OsGlobalOperations *>(pLinuxGlobalOperationsImp);
}

} // namespace Sysman
} // namespace L0

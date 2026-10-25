/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/linux/pmt_util.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/linux/system_info.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/utilities/directory.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"

#include <map>
#include <string>
#include <utility>

constexpr static auto gfxProduct = IGFX_PVC;

namespace NEO {
const std::map<std::string, std::pair<uint32_t, uint32_t>> guidUuidOffsetMap = {
    // add new values for guid in the form of {"guid", {offset, size}} for each platform
    {"0x41fe79a5", {64u, 8u}}};
#include "shared/source/os_interface/linux/product_helper_xe_hpc_and_later.inl"

namespace UuidUtil {

static bool uuidReadFromTelem(std::string_view telemDir, std::array<char, PmtUtil::guidStringSize> &guidString, const uint64_t offset,
                              const uint8_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) {
    auto pos = guidUuidOffsetMap.find(guidString.data());
    if (pos != guidUuidOffsetMap.end()) {
        uuid.fill(0);
        int errorNum = 0;
        ssize_t bytesRead = PmtUtil::readTelem(telemDir.data(), pos->second.second, pos->second.first + offset, uuid.data(), errorNum);
        if (bytesRead == pos->second.second) {
            uuid[15] = deviceIndex;
            return true;
        }
    }
    return false;
}

} // namespace UuidUtil

template <>
bool ProductHelperHw<gfxProduct>::getUuid(DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const {
    if (driverModel->getDriverModelType() != DriverModelType::drm) {
        return false;
    }

    auto pDrm = driverModel->as<Drm>();
    std::optional<std::string> rootPciPath = getPciRootPath(pDrm->getFileDescriptor());
    if (!rootPciPath.has_value()) {
        return false;
    }
    std::map<uint32_t, std::string> telemPciPath;
    PmtUtil::getTelemNodesInPciPath(rootPciPath.value(), telemPciPath);
    // number of telem nodes must be same as subdevice count + 1(root device)
    if (telemPciPath.size() < subDeviceCount + 1) {
        return false;
    }

    auto iterator = telemPciPath.begin();
    // use the root node
    std::string telemDir = iterator->second;

    std::array<char, PmtUtil::guidStringSize> guidString;
    int errorNum = 0;
    if (!PmtUtil::readGuid(telemDir, guidString, errorNum)) {
        return false;
    }

    uint64_t offset = std::numeric_limits<uint64_t>::max();
    if (!PmtUtil::readOffset(telemDir, offset, errorNum)) {
        return false;
    }

    return UuidUtil::uuidReadFromTelem(telemDir, guidString, offset, deviceIndex, uuid);
}
} // namespace NEO
#include "shared/source/os_interface/linux/product_helper_before_xe2_drm_slm.inl"
#include "shared/source/xe_hpc_core/os_agnostic_product_helper_xe_hpc_core.inl"
#include "shared/source/xe_hpc_core/pvc/os_agnostic_product_helper_pvc.inl"
namespace NEO {

template <>
uint64_t ProductHelperHw<gfxProduct>::getDeviceMemoryMaxBandWidthInBytesPerSecond(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {
    uint64_t memoryMaxClkRateInMhz = getDeviceMemoryMaxClkRate(hwInfo, osIface, subDeviceIndex);
    if (memoryMaxClkRateInMhz == 0) {
        return 0;
    }

    if (osIface == nullptr) {
        return 0;
    }
    auto driverModel = osIface->getDriverModel();
    if (driverModel == nullptr || driverModel->getDriverModelType() != DriverModelType::drm) {
        return 0;
    }
    auto pDrm = driverModel->as<Drm>();
    auto systemInfo = pDrm->getSystemInfo();
    if (systemInfo == nullptr) {
        return 0;
    }

    // HBM: total bus width (bits) = numHbmStacksPerTile x numChannelsPerHbmStack x 64
    const uint64_t memoryBusWidthInBits =
        static_cast<uint64_t>(systemInfo->getNumHbmStacksPerTile()) *
        static_cast<uint64_t>(systemInfo->getNumChannelsPerHbmStack()) * 64u;
    if (memoryBusWidthInBits == 0) {
        return 0;
    }

    // Bandwidth = Clock Rate (MHz) x 10^6 x Bus Width (bits) / 8 * 2
    // HBM memory clock as reported by sysfs file node requires 2x
    // data-rate multiplier for correct peak BW reporting.
    return memoryMaxClkRateInMhz * 1000 * 1000 * memoryBusWidthInBits / 4;
}

template <>
bool ProductHelperHw<gfxProduct>::isPageFaultSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isDisableScratchPagesSupported() const {
    return true;
}

} // namespace NEO

#include "shared/source/xe_hpc_core/linux/product_helper_xe_hpc_core.inl"

template class NEO::ProductHelperHw<gfxProduct>;

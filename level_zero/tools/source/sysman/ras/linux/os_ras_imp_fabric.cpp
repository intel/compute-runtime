/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"

#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp_prelim.h"

#include "sysman/linux/fs_access.h"
#include "sysman/linux/os_sysman_imp.h"

#include <cstring>
#include <regex>
namespace L0 {

void LinuxRasSourceFabric::getNodes(std::vector<std::string> &nodes, uint32_t subdeviceId, LinuxSysmanImp *pSysmanImp, const zes_ras_error_type_t &type) {
    const uint32_t minBoardStrappedNumber = 0;
    const uint32_t maxBoardStrappedNumber = 31;
    const uint32_t minPortId = 1;
    const uint32_t maxPortId = 8;
    nodes.clear();

    const std::string iafPathStringMfd("/sys/module/iaf/drivers/platform:iaf/");
    const std::string iafPathStringAuxillary("/sys/module/iaf/drivers/auxiliary:iaf/");
    std::string iafPathString("");

    if (pSysmanImp->getSysfsAccess().getRealPath("device/", iafPathString) != ZE_RESULT_SUCCESS) {
        return;
    }

    auto &fsAccess = pSysmanImp->getFsAccess();
    if (fsAccess.directoryExists(iafPathStringMfd)) {
        iafPathString = iafPathString + "/iaf.";
    } else if (fsAccess.directoryExists(iafPathStringAuxillary)) {
        iafPathString = iafPathString + "/i915.iaf.";
    } else {
        return;
    }

    for (auto boardStrappedNumber = minBoardStrappedNumber; boardStrappedNumber <= maxBoardStrappedNumber; boardStrappedNumber++) {

        const auto boardStrappedString(iafPathString + std::to_string(boardStrappedNumber));
        if (!fsAccess.directoryExists(boardStrappedString)) {
            continue;
        }
        const auto subDeviceString(boardStrappedString + "/sd." + std::to_string(subdeviceId));
        std::vector<std::string> subDeviceErrorNodes;

        if (type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            subDeviceErrorNodes.push_back(subDeviceString + "/fw_comm_errors");
            for (auto portId = minPortId; portId <= maxPortId; portId++) {
                subDeviceErrorNodes.push_back(subDeviceString + "/port." + std::to_string(portId) + "/link_degrades");
            }
        } else {
            subDeviceErrorNodes.push_back(subDeviceString + "/sd_failure");
            subDeviceErrorNodes.push_back(subDeviceString + "/fw_error");
            for (auto portId = minPortId; portId <= maxPortId; portId++) {
                subDeviceErrorNodes.push_back(subDeviceString + "/port." + std::to_string(portId) + "/link_failures");
            }
        }

        for (auto &subDeviceErrorNode : subDeviceErrorNodes) {
            if (ZE_RESULT_SUCCESS == fsAccess.canRead(subDeviceErrorNode)) {
                nodes.push_back(subDeviceErrorNode);
            }
        }
    }
}

ze_result_t LinuxRasSourceFabric::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType,
                                                            OsSysman *pOsSysman, ze_device_handle_t deviceHandle) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    NEO::Device *neoDevice = static_cast<Device *>(deviceHandle)->getNEODevice();
    uint32_t subDeviceIndex = neoDevice->isSubDevice() ? static_cast<NEO::SubDevice *>(neoDevice)->getSubDeviceIndex() : 0;

    std::vector<std::string> nodes;
    getNodes(nodes, subDeviceIndex, pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
    if (nodes.size()) {
        errorType.insert(ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
    }
    getNodes(nodes, subDeviceIndex, pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE);
    if (nodes.size()) {
        errorType.insert(ZES_RAS_ERROR_TYPE_CORRECTABLE);
    }

    return ZE_RESULT_SUCCESS;
}

LinuxRasSourceFabric::LinuxRasSourceFabric(OsSysman *pOsSysman, zes_ras_error_type_t type, uint32_t subDeviceId) {

    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    getNodes(errorNodes, subDeviceId, pLinuxSysmanImp, type);
}

uint64_t LinuxRasSourceFabric::getComputeErrorCount() {
    uint64_t currentErrorCount = 0;
    auto &fsAccess = pLinuxSysmanImp->getFsAccess();
    for (const auto &node : errorNodes) {
        uint64_t errorCount = 0;
        fsAccess.read(node, errorCount);
        currentErrorCount += errorCount;
    }
    return currentErrorCount;
}

ze_result_t LinuxRasSourceFabric::osRasGetState(zes_ras_state_t &state, ze_bool_t clear) {

    if (errorNodes.size() == 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::memset(state.category, 0, sizeof(zes_ras_state_t::category));
    uint64_t currentComputeErrorCount = getComputeErrorCount();

    if (clear) {
        baseComputeErrorCount = currentComputeErrorCount;
        currentComputeErrorCount = getComputeErrorCount();
    }
    state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS] = currentComputeErrorCount - baseComputeErrorCount;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
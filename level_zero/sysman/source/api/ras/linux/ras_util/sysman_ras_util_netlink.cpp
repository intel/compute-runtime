/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/api/ras/linux/sysman_os_ras_imp.h"
#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_nl_api.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

std::map<zes_ras_error_category_exp_t, std::string_view> categoryToErrorNameMap = {
    {ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS, "core-compute"},
    {ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS, "device-memory"},
    {ZES_RAS_ERROR_CATEGORY_EXP_SCALE_ERRORS, "scale"},
    {static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_FABRIC_ERRORS), "fabric"},
    {static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_PCIE_ERRORS), "pcie"},
    {static_cast<zes_ras_error_category_exp_t>(ZES_INTEL_RAS_ERROR_CATEGORY_EXP_SOC_INTERNAL_ERRORS), "soc-internal"}};

std::vector<DrmRasNode> NetlinkRasUtil::rasNodes;
std::map<uint32_t, std::vector<DrmErrorCounter>> NetlinkRasUtil::rasErrorList;
std::unique_ptr<DrmNlApi> (*NetlinkRasUtil::createDrmNlApi)() = []() { return std::make_unique<DrmNlApi>(); };

void NetlinkRasUtil::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, LinuxSysmanImp *pLinuxSysmanImp, ze_bool_t isSubDevice, uint32_t subDeviceId) {
    // Query available RAS nodes from netlink interface
    if (rasNodes.empty()) {
        auto pDrmNlApi = createDrmNlApi();
        ze_result_t result = pDrmNlApi->listNodes(rasNodes);
        if (result != ZE_RESULT_SUCCESS) {
            return;
        }
        // Cache the error list for each discovered node
        for (const auto &node : rasNodes) {
            std::vector<DrmErrorCounter> errorList;
            pDrmNlApi->getErrorsList(node.nodeId, errorList);
            rasErrorList[node.nodeId] = std::move(errorList);
        }
    }

    // Get device PCI BDF string to match with RAS nodes
    std::string devicePciBdf = pLinuxSysmanImp->getDevicePciBdf();

    // Determine supported error types based on available RAS nodes info i.e. device and node names
    for (const auto &node : rasNodes) {
        if (node.deviceName == devicePciBdf) {
            if (node.nodeName == "correctable-errors") {
                errorType.insert(ZES_RAS_ERROR_TYPE_CORRECTABLE);
            } else if (node.nodeName == "uncorrectable-errors") {
                errorType.insert(ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
            }
        }
    }
}

uint32_t NetlinkRasUtil::rasGetCategoryCount() {
    return static_cast<uint32_t>(categoryToErrorNameMap.size());
}

std::vector<zes_ras_error_category_exp_t> NetlinkRasUtil::getSupportedErrorCategoriesExp() {
    auto it = rasErrorList.find(rasNodeId);
    if (it == rasErrorList.end()) {
        return {};
    }
    const auto &errorList = it->second;
    std::vector<zes_ras_error_category_exp_t> categories;
    for (const auto &entry : categoryToErrorNameMap) {
        // Only include categories for which we have error counters from netlink
        auto err = std::find_if(errorList.begin(), errorList.end(),
                                [&](const DrmErrorCounter &counter) -> bool {
                                    return (counter.errorName == entry.second);
                                });
        if (err != errorList.end()) {
            categories.push_back(entry.first);
        }
    }
    return categories;
}

ze_result_t NetlinkRasUtil::rasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t NetlinkRasUtil::rasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) {
    // Step 1: Get ErrorsList for this RAS node type (correctable or uncorrectable)
    std::vector<DrmErrorCounter> errorList;
    ze_result_t result = drmNl->getErrorsList(rasNodeId, errorList);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    // Step 2: Aggregate error counts based on category
    uint32_t categoryIdx = 0u;
    for (auto const &rasErrorCatToListOfErrors : categoryToErrorNameMap) {
        if (categoryIdx >= numCategoriesRequested) {
            break;
        }

        uint64_t errorCountForCategory = 0;

        auto err = std::find_if(errorList.begin(), errorList.end(),
                                [&](const DrmErrorCounter &counter) -> bool {
                                    return (counter.errorName == rasErrorCatToListOfErrors.second);
                                });

        if (err != errorList.end()) {
            errorCountForCategory += err->errorValue;
        }

        pState[categoryIdx].category = rasErrorCatToListOfErrors.first;
        pState[categoryIdx].errorCounter = errorCountForCategory;
        categoryIdx++;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t NetlinkRasUtil::rasClearStateExp(zes_ras_error_category_exp_t category) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

NetlinkRasUtil::NetlinkRasUtil(zes_ras_error_type_t type, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) : pLinuxSysmanImp(pLinuxSysmanImp), rasErrorType(type) {
    std::string devicePciBdf = pLinuxSysmanImp->getDevicePciBdf();

    drmNl = std::make_unique<DrmNlApi>();
    UNRECOVERABLE_IF(drmNl.get() == nullptr);

    // Determine the appropriate node name based on RAS error type
    std::string targetNodeName = "";
    if (rasErrorType == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
        targetNodeName = "correctable-errors";
    } else {
        targetNodeName = "uncorrectable-errors";
    }

    // Find the nodeId matching the target node name and device name from the static nodes list
    rasNodeId = 0;
    for (const auto &node : rasNodes) {
        if ((node.nodeName == targetNodeName) && (node.deviceName == devicePciBdf)) {
            rasNodeId = node.nodeId;
            break;
        }
    }
}

NetlinkRasUtil::~NetlinkRasUtil() = default;

} // namespace Sysman
} // namespace L0

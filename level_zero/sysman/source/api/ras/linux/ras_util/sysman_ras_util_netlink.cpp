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

std::map<zes_ras_error_category_exp_t, std::vector<std::string>> categoryToListOfErrors = {
    {ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS, {"core-compute"}},
    {ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS, {"soc-internal"}},
};

std::vector<DrmRasNode> NetlinkRasUtil::rasNodes;
std::unique_ptr<DrmNlApi> (*NetlinkRasUtil::createDrmNlApi)() = []() { return std::make_unique<DrmNlApi>(); };

void NetlinkRasUtil::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, LinuxSysmanImp *pLinuxSysmanImp, ze_bool_t isSubDevice, uint32_t subDeviceId) {
    // Query available RAS nodes from netlink interface
    if (rasNodes.empty()) {
        auto pDrmNlApi = createDrmNlApi();
        ze_result_t result = pDrmNlApi->listNodes(rasNodes);
        if (result != ZE_RESULT_SUCCESS) {
            return;
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
    return static_cast<uint32_t>(categoryToListOfErrors.size());
}

ze_result_t NetlinkRasUtil::rasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    // Step 1: Initialize all category counts to 0
    for (auto const &rasErrorCatToListOfErrors : categoryToListOfErrors) {
        state.category[rasErrorCatToListOfErrors.first] = 0;
    }

    // Step 2: Get ErrorsList for this RAS node type (correctable or uncorrectable)
    std::vector<DrmErrorCounter> errorList;
    ze_result_t result = drmNl->getErrorsList(rasNodeId, errorList);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    // Step 3: Aggregate error counts based on category
    for (auto const &rasErrorCatToListOfErrors : categoryToListOfErrors) {
        for (auto const &errorName : rasErrorCatToListOfErrors.second) {
            auto err = std::find_if(errorList.begin(), errorList.end(),
                                    [&](const DrmErrorCounter &counter) -> bool {
                                        return (counter.errorName == errorName);
                                    });

            if (err != errorList.end()) {
                state.category[rasErrorCatToListOfErrors.first] += err->errorValue;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
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
    for (auto const &rasErrorCatToListOfErrors : categoryToListOfErrors) {
        if (categoryIdx >= numCategoriesRequested) {
            break;
        }

        uint64_t errorCountForCategory = 0;

        for (auto const &errorName : rasErrorCatToListOfErrors.second) {
            auto err = std::find_if(errorList.begin(), errorList.end(),
                                    [&](const DrmErrorCounter &counter) -> bool {
                                        return (counter.errorName == errorName);
                                    });

            if (err != errorList.end()) {
                errorCountForCategory += err->errorValue;
            }
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

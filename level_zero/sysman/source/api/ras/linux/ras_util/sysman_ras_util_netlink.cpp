/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util_netlink_category_map.h"
#include "level_zero/sysman/source/api/ras/linux/sysman_os_ras_imp.h"
#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_nl_api.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

std::vector<DrmRasNode> NetlinkRasUtil::rasNodes;
std::map<uint32_t, std::vector<DrmErrorCounter>> NetlinkRasUtil::rasErrorList;
std::unique_ptr<DrmNlApi> (*NetlinkRasUtil::createDrmNlApi)() = []() { return std::make_unique<DrmNlApi>(); };

ze_result_t NetlinkRasUtil::initializeRasNodes(DrmNlApi *pDrmNl) {
    if (rasNodes.empty()) {
        ze_result_t result = pDrmNl->listNodes(rasNodes);
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Failed to list netlink RAS nodes: 0x%x\n", result);
            return result;
        }
    }
    return ZE_RESULT_SUCCESS;
}

void NetlinkRasUtil::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, LinuxSysmanImp *pLinuxSysmanImp, ze_bool_t isSubDevice, uint32_t subDeviceId) {
    auto pDrmNlApi = createDrmNlApi();

    // Populate rasNodes via initializeRasNodes if not already done.
    if (initializeRasNodes(pDrmNlApi.get()) != ZE_RESULT_SUCCESS) {
        return;
    }

    // Cache the error list for each discovered node if not already populated.
    if (rasErrorList.empty()) {
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
    for (auto const &categoryToErrorPair : categoryToErrorNameMap) {
        if (categoryIdx >= numCategoriesRequested) {
            break;
        }

        uint64_t errorCountForCategory = 0;

        auto err = std::find_if(errorList.begin(), errorList.end(),
                                [&](const DrmErrorCounter &counter) -> bool {
                                    return (counter.errorName == categoryToErrorPair.second);
                                });

        if (err != errorList.end()) {
            errorCountForCategory += err->errorValue;
        }

        pState[categoryIdx].category = categoryToErrorPair.first;
        pState[categoryIdx].errorCounter = errorCountForCategory;
        categoryIdx++;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t NetlinkRasUtil::rasGetStateExp2(const uint32_t count, const zes_ras_error_category_exp_t *pCategories, zes_ras_state_exp2_t *pStates) {
    std::vector<DrmErrorCounter> errorList;
    ze_result_t result = drmNl->getErrorsList(rasNodeId, errorList);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    for (uint32_t i = 0; i < count; i++) {
        pStates[i].errorCounter = 0;
        auto it = categoryToErrorNameMap.find(pCategories[i]);
        if (it != categoryToErrorNameMap.end()) {
            auto err = std::find_if(errorList.begin(), errorList.end(),
                                    [&](const DrmErrorCounter &counter) -> bool {
                                        return counter.errorName == it->second;
                                    });
            if (err != errorList.end()) {
                pStates[i].errorCounter = err->errorValue;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t NetlinkRasUtil::rasClearStateExp(zes_ras_error_category_exp_t category) {

    auto categoryToErrorNameIt = categoryToErrorNameMap.find(category);
    if (categoryToErrorNameIt == categoryToErrorNameMap.end()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): RAS error category not supported by Netlink and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    std::string_view errorName = categoryToErrorNameIt->second;

    auto rasErrorListIt = rasErrorList.find(rasNodeId);
    if (rasErrorListIt == rasErrorList.end()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): No error list found for RAS node and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    const auto &errorList = rasErrorListIt->second;

    auto err = std::find_if(errorList.begin(), errorList.end(),
                            [&](const DrmErrorCounter &counter) -> bool {
                                return (counter.errorName == errorName);
                            });

    if (err == errorList.end()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): No error counter found for RAS error category and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    uint32_t errorId = err->errorId;
    return drmNl->clearErrorCounter(rasNodeId, errorId);
}

ze_result_t NetlinkRasUtil::rasGetConfigExp(const uint32_t count, zes_ras_config_exp_t *pConfig) {
    // For each requested category, look up the corresponding DRM error name via categoryToErrorNameMap,
    // then find the matching error counter in the cached error list and call getErrorThreshold on the
    // DRM netlink interface to populate pConfig[i].threshold.
    auto errListIt = rasErrorList.find(rasNodeId);
    if (errListIt == rasErrorList.end()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): No error list found for RAS node and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    const auto &errorList = errListIt->second;

    for (uint32_t i = 0; i < count; i++) {
        auto categoryIt = categoryToErrorNameMap.find(pConfig[i].category);
        if (categoryIt == categoryToErrorNameMap.end()) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stdout, "Debug@ %s(): RAS category 0x%x not supported by Netlink, skipping \n", __FUNCTION__, pConfig[i].category);
            continue;
        }

        auto errIt = std::find_if(errorList.begin(), errorList.end(),
                                  [&](const DrmErrorCounter &counter) {
                                      return counter.errorName == categoryIt->second;
                                  });
        if (errIt == errorList.end()) {
            continue;
        }

        DrmErrorThreshold threshold = {};
        ze_result_t result = drmNl->getErrorThreshold(rasNodeId, errIt->errorId, threshold);
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): getErrorThreshold failed and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
        pConfig[i].threshold = static_cast<uint64_t>(threshold.threshold);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t NetlinkRasUtil::rasSetConfigExp(const uint32_t count, const zes_ras_config_exp_t *pConfig) {
    // For each requested category, look up the corresponding DRM error name via categoryToErrorNameMap,
    // then find the matching error counter in the cached error list and call setErrorThreshold on the
    // DRM netlink interface.
    auto errListIt = rasErrorList.find(rasNodeId);
    if (errListIt == rasErrorList.end()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): No error list found for RAS node and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    const auto &errorList = errListIt->second;

    for (uint32_t i = 0; i < count; i++) {
        auto categoryIt = categoryToErrorNameMap.find(pConfig[i].category);
        if (categoryIt == categoryToErrorNameMap.end()) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stdout, "Debug@ %s(): RAS category 0x%x not supported by Netlink, skipping \n", __FUNCTION__, pConfig[i].category);
            continue;
        }

        auto errIt = std::find_if(errorList.begin(), errorList.end(),
                                  [&](const DrmErrorCounter &counter) {
                                      return counter.errorName == categoryIt->second;
                                  });
        if (errIt == errorList.end()) {
            continue;
        }

        ze_result_t result = drmNl->setErrorThreshold(rasNodeId, errIt->errorId, static_cast<uint32_t>(pConfig[i].threshold));
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): setErrorThreshold failed and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
    }
    return ZE_RESULT_SUCCESS;
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

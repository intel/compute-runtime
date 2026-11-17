/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_ras_types.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <algorithm>
#include <vector>

namespace L0 {
namespace Sysman {

static const std::map<zes_ras_error_category_exp_t, std::string_view> netlinkCategoryToErrorNameMap = {
    {ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS, "core-compute"},
    {ZES_RAS_ERROR_CATEGORY_EXP_FABRIC_ERRORS, "fabric"},
    {ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS, "device-memory"},
    {ZES_RAS_ERROR_CATEGORY_EXP_PCIE_ERRORS, "pcie"},
    {ZES_RAS_ERROR_CATEGORY_EXP_SCALE_ERRORS, "scale"},
    {ZES_RAS_ERROR_CATEGORY_EXP_SOC_INTERNAL_ERRORS, "soc-internal"}};

std::map<zes_ras_error_category_exp_t, std::string_view> NetlinkRasUtil::createCategoryToErrorNameMap(LinuxSysmanImp *pLinuxSysmanImp) {
    const auto &supportedErrorCategories = pLinuxSysmanImp->getSysmanProductHelper()->getRasNetlinkSupportedErrorCategories();
    std::map<zes_ras_error_category_exp_t, std::string_view> categoryToErrorNameMap;
    for (const auto &entry : netlinkCategoryToErrorNameMap) {
        if (supportedErrorCategories.contains(entry.first)) {
            categoryToErrorNameMap.insert(entry);
        }
    }
    return categoryToErrorNameMap;
}

uint32_t NetlinkRasUtil::rasGetCategoryCount() {
    return static_cast<uint32_t>(this->categoryToErrorNameMap.size());
}

std::vector<zes_ras_error_category_exp_t> NetlinkRasUtil::getSupportedErrorCategoriesExp() {
    auto it = rasErrorList.find(rasNodeId);
    if (it == rasErrorList.end()) {
        return {};
    }
    const auto &errorList = it->second;
    std::vector<zes_ras_error_category_exp_t> categories;
    for (const auto &entry : this->categoryToErrorNameMap) {
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

} // namespace Sysman
} // namespace L0

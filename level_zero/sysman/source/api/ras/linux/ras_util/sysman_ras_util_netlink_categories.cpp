/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util_netlink_category_map.h"
#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_ras_types.h"

#include <algorithm>
#include <vector>

namespace L0 {
namespace Sysman {

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

/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/graph/graph.h"
#include "level_zero/experimental/source/graph/graph_export.h"
#include "level_zero/ze_api.h"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace L0 {

namespace GraphDumpHelper {

void addLaunchKernelAdditionalExtensionParameters(std::vector<std::pair<std::string, std::string>> &params, const ze_base_desc_t *baseDesc) {
    const auto stypeValue = std::to_string(static_cast<uint32_t>(baseDesc->stype));

    params.emplace_back("extension.stype", stypeValue + " (not recognized)");
}

void addMemoryTransferAdditionalExtensionParameters(std::vector<std::pair<std::string, std::string>> &params, const ze_base_desc_t *baseDesc) {
    const auto stypeValue = std::to_string(static_cast<uint32_t>(baseDesc->stype));

    params.emplace_back("extension.stype", stypeValue + " (not recognized)");
}

} // namespace GraphDumpHelper

} // namespace L0

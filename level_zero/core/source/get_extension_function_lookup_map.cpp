/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/get_extension_function_lookup_map.h"

namespace L0 {
std::unordered_map<std::string, void *> getExtensionFunctionsLookupMap() {
    return std::unordered_map<std::string, void *>();
}

} // namespace L0
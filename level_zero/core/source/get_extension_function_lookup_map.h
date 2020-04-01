/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string>
#include <unordered_map>

namespace L0 {
std::unordered_map<std::string, void *> getExtensionFunctionsLookupMap();
} // namespace L0
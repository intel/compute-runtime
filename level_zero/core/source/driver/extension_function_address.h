/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>
#include <unordered_map>

namespace L0 {
struct ExtensionFunctionAddressHelper {
    static void *getExtensionFunctionAddress(const std::string &functionName);

  protected:
    static void *getAdditionalExtensionFunctionAddress(const std::string &functionName);
};
} // namespace L0
